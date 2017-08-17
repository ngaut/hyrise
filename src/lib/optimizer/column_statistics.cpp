#include "column_statistics.hpp"

#include <algorithm>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "all_parameter_variant.hpp"
#include "common.hpp"
#include "operators/aggregate.hpp"
#include "operators/table_wrapper.hpp"
#include "storage/table.hpp"
#include "table_statistics.hpp"
#include "type_cast.hpp"
#include "types.hpp"

namespace opossum {

template <typename ColumnType>
ColumnStatistics<ColumnType>::ColumnStatistics(const ColumnID column_id, const std::weak_ptr<Table> table)
    : _column_id(column_id), _table(table) {}

template <typename ColumnType>
ColumnStatistics<ColumnType>::ColumnStatistics(const ColumnID column_id, float distinct_count, const ColumnType min,
                                               const ColumnType max, const float null_value_ratio)
    : BaseColumnStatistics(null_value_ratio),
      _column_id(column_id),
      _table(std::weak_ptr<Table>()),
      _distinct_count(distinct_count),
      _min(min),
      _max(max) {}

template <typename ColumnType>
float ColumnStatistics<ColumnType>::distinct_count() const {
  if (_distinct_count) {
    return *_distinct_count;
  }

  // Calculation of distinct_count is delegated to aggregate operator.
  auto table = _table.lock();
  DebugAssert(table != nullptr, "Corresponding table of column statistics is deleted.");
  auto table_wrapper = std::make_shared<TableWrapper>(table);
  table_wrapper->execute();
  auto aggregate =
      std::make_shared<Aggregate>(table_wrapper, std::vector<AggregateDefinition>{}, std::vector<ColumnID>{_column_id});
  aggregate->execute();
  auto aggregate_table = aggregate->get_output();
  _distinct_count = aggregate_table->row_count();
  return *_distinct_count;
}

template <typename ColumnType>
ColumnType ColumnStatistics<ColumnType>::min() const {
  if (!_min) {
    initialize_min_max();
  }
  return *_min;
}

template <typename ColumnType>
ColumnType ColumnStatistics<ColumnType>::max() const {
  if (!_max) {
    initialize_min_max();
  }
  return *_max;
}

template <typename ColumnType>
void ColumnStatistics<ColumnType>::initialize_min_max() const {
  // Calculation is delegated to aggregate operator.
  auto table = _table.lock();
  DebugAssert(table != nullptr, "Corresponding table of column statistics is deleted.");

  auto table_wrapper = std::make_shared<TableWrapper>(table);
  table_wrapper->execute();

  auto aggregate_args =
      std::vector<AggregateDefinition>{{_column_id, AggregateFunction::Min}, {_column_id, AggregateFunction::Max}};
  auto aggregate = std::make_shared<Aggregate>(table_wrapper, aggregate_args, std::vector<ColumnID>{});
  aggregate->execute();

  auto aggregate_table = aggregate->get_output();
  _min = aggregate_table->template get_value<ColumnType>(ColumnID{0}, 0);
  _max = aggregate_table->template get_value<ColumnType>(ColumnID{1}, 0);
}

template <typename ColumnType>
ColumnSelectivityResult ColumnStatistics<ColumnType>::create_column_stats_for_range_predicate(ColumnType minimum,
                                                                                              ColumnType maximum) {
  // new minimum/maximum of table cannot be smaller/larger than the current minimum/maximum
  auto common_min = std::max(minimum, min());
  auto common_max = std::min(maximum, max());
  if (common_min == min() && common_max == max()) {
    return {_non_null_value_ratio, nullptr};
  }
  float selectivity = 0.f;
  if (common_max >= common_min) {
    selectivity = estimate_selectivity_for_range(common_min, common_max);
  }
  auto column_statistics =
      std::make_shared<ColumnStatistics>(_column_id, selectivity * distinct_count(), common_min, common_max);
  return {_non_null_value_ratio * selectivity, column_statistics};
}

template <typename ColumnType>
float ColumnStatistics<ColumnType>::estimate_selectivity_for_range(ColumnType minimum, ColumnType maximum) {
  // distinction between integers and decimals
  // for integers the number of possible integers is used within the inclusive ranges
  // for decimals the size of the range is used
  if (std::is_integral<ColumnType>::value) {
    return static_cast<float>(maximum - minimum + 1) / static_cast<float>(max() - min() + 1);
  } else {
    return static_cast<float>(maximum - minimum) / static_cast<float>(max() - min());
  }
}

/**
 * Specialization for strings as they cannot be used in subtractions.
 */
template <>
float ColumnStatistics<std::string>::estimate_selectivity_for_range(std::string minimum, std::string maximum) {
  // TODO(anyone) implement selectivity for range approximation for column type string.
  return (maximum < minimum) ? 0.f : 1.f;
}

template <typename ColumnType>
ColumnSelectivityResult ColumnStatistics<ColumnType>::create_column_stats_for_equals_predicate(ColumnType value) {
  float new_distinct_count = 1.f;
  if (value < min() || value > max()) {
    new_distinct_count = 0.f;
  }
  auto column_statistics = std::make_shared<ColumnStatistics>(_column_id, new_distinct_count, value, value);
  return {_non_null_value_ratio * new_distinct_count / distinct_count(), column_statistics};
}

template <typename ColumnType>
ColumnSelectivityResult ColumnStatistics<ColumnType>::create_column_stats_for_unequals_predicate(ColumnType value) {
  if (value < min() || value > max()) {
    return {_non_null_value_ratio, nullptr};
  }
  auto column_statistics = std::make_shared<ColumnStatistics>(_column_id, distinct_count() - 1, min(), max());
  return {_non_null_value_ratio * (1 - 1.f / distinct_count()), column_statistics};
}

template <typename ColumnType>
ColumnSelectivityResult ColumnStatistics<ColumnType>::estimate_selectivity_for_predicate(
    const ScanType scan_type, const AllTypeVariant &value, const optional<AllTypeVariant> &value2) {
  auto casted_value = type_cast<ColumnType>(value);

  switch (scan_type) {
    case ScanType::OpEquals: {
      return create_column_stats_for_equals_predicate(casted_value);
    }
    case ScanType::OpNotEquals: {
      return create_column_stats_for_unequals_predicate(casted_value);
    }
    case ScanType::OpLessThan: {
      // distinction between integers and decimals
      // for integers "< value" means that the new max is value <= value - 1
      // for decimals "< value" means that the new max is value <= value - ε
      if (std::is_integral<ColumnType>::value) {
        return create_column_stats_for_range_predicate(min(), casted_value - 1);
      }
// intentionally no break
// if ColumnType is a floating point number,
// OpLessThanEquals behaviour is expected instead of OpLessThan
#if __has_cpp_attribute(fallthrough)
      [[fallthrough]];
#endif
    }
    case ScanType::OpLessThanEquals: {
      return create_column_stats_for_range_predicate(min(), casted_value);
    }
    case ScanType::OpGreaterThan: {
      // distinction between integers and decimals
      // for integers "> value" means that the new min value is >= value + 1
      // for decimals "> value" means that the new min value is >= value + ε
      if (std::is_integral<ColumnType>::value) {
        return create_column_stats_for_range_predicate(casted_value + 1, max());
      }
// intentionally no break
// if ColumnType is a floating point number,
// OpGreaterThanEquals behaviour is expected instead of OpGreaterThan
#if __has_cpp_attribute(fallthrough)
      [[fallthrough]];
#endif
    }
    case ScanType::OpGreaterThanEquals: {
      return create_column_stats_for_range_predicate(casted_value, max());
    }
    case ScanType::OpBetween: {
      DebugAssert(static_cast<bool>(value2), "Operator BETWEEN should get two parameters, second is missing!");
      auto casted_value2 = type_cast<ColumnType>(*value2);
      return create_column_stats_for_range_predicate(casted_value, casted_value2);
    }
    default: { return {_non_null_value_ratio, nullptr}; }
  }
}

/**
 * Specialization for strings as they cannot be used in subtractions.
 */
template <>
ColumnSelectivityResult ColumnStatistics<std::string>::estimate_selectivity_for_predicate(
    const ScanType scan_type, const AllTypeVariant &value, const optional<AllTypeVariant> &value2) {
  auto casted_value = type_cast<std::string>(value);
  switch (scan_type) {
    case ScanType::OpEquals: {
      return create_column_stats_for_equals_predicate(casted_value);
    }
    case ScanType::OpNotEquals: {
      return create_column_stats_for_unequals_predicate(casted_value);
    }
    // TODO(anybody) implement other table-scan operators for string.
    default: { return {_non_null_value_ratio, nullptr}; }
  }
}

template <typename ColumnType>
ColumnSelectivityResult ColumnStatistics<ColumnType>::estimate_selectivity_for_predicate(
    const ScanType scan_type, const ValuePlaceholder &value, const optional<AllTypeVariant> &value2) {
  switch (scan_type) {
    case ScanType::OpEquals: {
      auto column_statistics = std::make_shared<ColumnStatistics>(_column_id, 1, min(), max());
      return {1.f / distinct_count(), column_statistics};
    }
    case ScanType::OpNotEquals: {
      auto column_statistics = std::make_shared<ColumnStatistics>(_column_id, distinct_count() - 1, min(), max());
      return {(distinct_count() - 1.f) / distinct_count(), column_statistics};
    }
    case ScanType::OpLessThan:
    case ScanType::OpLessThanEquals:
    case ScanType::OpGreaterThan:
    case ScanType::OpGreaterThanEquals: {
      auto column_statistics = std::make_shared<ColumnStatistics>(
          _column_id, distinct_count() * DEFAULT_OPEN_ENDED_SELECTIVITY, min(), max());
      return {_non_null_value_ratio * DEFAULT_OPEN_ENDED_SELECTIVITY, column_statistics};
    }
    case ScanType::OpBetween: {
      // since the value2 is known,
      // first, statistics for the operation <= value are calculated
      // then, the open ended selectivity is applied on the result
      DebugAssert(static_cast<bool>(value2), "Operator BETWEEN should get two parameters, second is missing!");
      auto casted_value2 = type_cast<ColumnType>(*value2);
      ColumnSelectivityResult output = create_column_stats_for_range_predicate(min(), casted_value2);
      // return, if value2 < min
      if (output.selectivity == 0.f) {
        return output;
      }
      // create statistics, if value2 >= max
      if (output.column_statistics == nullptr) {
        output.column_statistics = std::make_shared<ColumnStatistics>(_column_id, distinct_count(), min(), max());
      }
      // apply default selectivity for open ended
      output.selectivity *= DEFAULT_OPEN_ENDED_SELECTIVITY;
      // column statistics have just been created, therefore, cast to the column type cannot fail
      auto column_statistics = std::dynamic_pointer_cast<ColumnStatistics<ColumnType>>(output.column_statistics);
      *(column_statistics->_distinct_count) *= DEFAULT_OPEN_ENDED_SELECTIVITY;
      return output;
    }
    default: { return {_non_null_value_ratio, nullptr}; }
  }
}

template <typename ColumnType>
TwoColumnSelectivityResult ColumnStatistics<ColumnType>::estimate_selectivity_for_two_column_predicate(
    const ScanType scan_type, const std::shared_ptr<BaseColumnStatistics> &right_base_column_statistics,
    const optional<AllTypeVariant> &value2) {
  auto right_stats = std::dynamic_pointer_cast<ColumnStatistics<ColumnType>>(right_base_column_statistics);
  DebugAssert(right_stats != nullptr, "Cannot compare columns of different type");

  // for aggregate "col_left < col_right": col_left statistics = this and col_right statistics = right_stats

  auto common_min = std::max(min(), right_stats->min());
  auto common_max = std::min(max(), right_stats->max());

  // calculate percentage of values before, in and above the common value range
  float left_overlapping_ratio = estimate_selectivity_for_range(common_min, common_max);
  float right_overlapping_ratio = right_stats->estimate_selectivity_for_range(common_min, common_max);

  float left_below, left_above, right_below, right_above;
  if (std::is_integral<ColumnType>::value) {
    left_below = (min() < common_min) ? estimate_selectivity_for_range(min(), common_min - 1) : 0;
    left_above = (common_max < max()) ? estimate_selectivity_for_range(common_max + 1, max()) : 0;
    bool below_min = right_stats->min() < common_min;
    right_below = below_min ? right_stats->estimate_selectivity_for_range(right_stats->min(), common_min - 1) : 0;
    bool above_max = common_max < right_stats->max();
    right_above = above_max ? right_stats->estimate_selectivity_for_range(common_max + 1, right_stats->max()) : 0;
  } else {
    left_below = estimate_selectivity_for_range(min(), common_min);
    left_above = estimate_selectivity_for_range(common_max, max());
    right_below = right_stats->estimate_selectivity_for_range(right_stats->min(), common_min);
    right_above = right_stats->estimate_selectivity_for_range(common_max, right_stats->max());
  }

  // calculate percentage of distinct values in common value range
  auto left_overlapping_distinct_count = left_overlapping_ratio * distinct_count();
  auto right_overlapping_distinct_count = right_overlapping_ratio * right_stats->distinct_count();

  float equal_values_ratio;
  // calculate percentage of rows with equal values
  if (left_overlapping_distinct_count < right_overlapping_distinct_count) {
    equal_values_ratio = left_overlapping_ratio / right_stats->distinct_count();
  } else {
    equal_values_ratio = right_overlapping_ratio / distinct_count();
  }

  // used for <, <=, > and >= scan_types
  auto estimate_selectivity_for_open_ended_operators = [&](float values_below_ratio, float values_above_ratio,
                                                           ColumnType new_min, ColumnType new_max,
                                                           bool add_equal_values) -> TwoColumnSelectivityResult {
    // selectivity calculated by adding up percentages that values are below, in or above overlapping range
    float selectivity = 0.f;
    // percentage of values on left hand side which are smaller than overlapping range
    selectivity += values_below_ratio;
    // selectivity of not equal numbers n1, n2 in overlapping range where n1 < n2 is 0.5
    selectivity += (left_overlapping_ratio * right_overlapping_ratio - equal_values_ratio) * 0.5f;
    if (add_equal_values) {
      selectivity += equal_values_ratio;
    }
    // percentage of values on right hand side which are greater than overlapping range
    selectivity += values_above_ratio;
    // remove percentage of rows, where one value is below and one value is above the overlapping range
    selectivity -= values_below_ratio * values_above_ratio;

    auto new_left_column_stats = create_column_stats_for_range_predicate(new_min, new_max).column_statistics;
    auto new_right_column_stats =
        right_stats->create_column_stats_for_range_predicate(new_min, new_max).column_statistics;
    return {_non_null_value_ratio * selectivity, new_left_column_stats, new_right_column_stats};
  };

  switch (scan_type) {
    case ScanType::OpEquals: {
      auto overlapping_distinct_count = std::min(left_overlapping_distinct_count, right_overlapping_distinct_count);

      auto new_left_column_stats =
          std::make_shared<ColumnStatistics>(_column_id, overlapping_distinct_count, common_min, common_max);
      auto new_right_column_stats = std::make_shared<ColumnStatistics>(
          right_stats->_column_id, overlapping_distinct_count, common_min, common_max);
      return {equal_values_ratio, new_left_column_stats, new_right_column_stats};
    }
    case ScanType::OpNotEquals: {
      auto new_left_column_stats = std::make_shared<ColumnStatistics>(_column_id, distinct_count(), min(), max());
      auto new_right_column_stats = std::make_shared<ColumnStatistics>(
          right_stats->_column_id, right_stats->distinct_count(), right_stats->min(), right_stats->max());
      return {1.f - equal_values_ratio, new_left_column_stats, new_right_column_stats};
    }
    case ScanType::OpLessThan: {
      return estimate_selectivity_for_open_ended_operators(left_below, right_above, min(), right_stats->max(), false);
    }
    case ScanType::OpLessThanEquals: {
      return estimate_selectivity_for_open_ended_operators(left_below, right_above, min(), right_stats->max(), true);
    }
    case ScanType::OpGreaterThan: {
      return estimate_selectivity_for_open_ended_operators(right_below, left_above, right_stats->min(), max(), false);
    }
    case ScanType::OpGreaterThanEquals: {
      return estimate_selectivity_for_open_ended_operators(right_below, left_above, right_stats->min(), max(), true);
    }
    // case ScanType::OpBetween is not supported for ColumnName as TableScan does not support this
    default: { return {_non_null_value_ratio, nullptr, nullptr}; }
  }
}

/**
 * Specialization for strings as they cannot be used in subtractions.
 */
template <>
TwoColumnSelectivityResult ColumnStatistics<std::string>::estimate_selectivity_for_two_column_predicate(
    const ScanType scan_type, const std::shared_ptr<BaseColumnStatistics> &right_base_column_statistics,
    const optional<AllTypeVariant> &value2) {
  // TODO(anybody) implement special case for strings
  return {_non_null_value_ratio, nullptr, nullptr};
}

template <typename ColumnType>
std::ostream &ColumnStatistics<ColumnType>::print_to_stream(std::ostream &os) const {
  os << "Col Stats id: " << _column_id << std::endl;
  os << "  dist. " << _distinct_count << std::endl;
  os << "  min   " << _min << std::endl;
  os << "  max   " << _max;
  return os;
}

template class ColumnStatistics<int32_t>;
template class ColumnStatistics<int64_t>;
template class ColumnStatistics<float>;
template class ColumnStatistics<double>;
template class ColumnStatistics<std::string>;

}  // namespace opossum