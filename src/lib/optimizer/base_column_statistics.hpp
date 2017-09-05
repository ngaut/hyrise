#pragma once

#include <memory>
#include <ostream>
#include <string>

#include "all_type_variant.hpp"
#include "common.hpp"

namespace opossum {

struct ColumnSelectivityResult;
struct TwoColumnSelectivityResult;

/**
 * Most prediction computation is delegated from table statistics to typed column statistics.
 * This enables the possibility to work with the column type for min and max values.
 *
 * Therefore, column statistics implements functions for all operators
 * so that the corresponding table statistics functions can delegate all predictions to column statistics.
 * These functions return a column selectivity result object combining the selectivity of the operator
 * and if changed the newly created column statistics.
 */
class BaseColumnStatistics {
 public:
  explicit BaseColumnStatistics(const float non_null_value_ratio = 1.f) : _non_null_value_ratio(non_null_value_ratio) {}
  virtual ~BaseColumnStatistics() = default;

  /**
   * Estimate selectivity for predicate with constants.
   * Predict result of a table scan with constant values.
   * @return Selectivity and new column statistics, if selectivity is not 0 or 1.
   */
  virtual ColumnSelectivityResult estimate_selectivity_for_predicate(
      const ScanType scan_type, const AllTypeVariant &value, const optional<AllTypeVariant> &value2 = nullopt) = 0;

  /**
   * Estimate selectivity for predicate with prepared statements.
   * In comparison to predicates with constants, value is not known yet.
   * Therefore, when necessary, default selectivity values are used for predictions.
   * @return Selectivity and new column statistics, if selectivity is not 0 or 1.
   */
  virtual ColumnSelectivityResult estimate_selectivity_for_predicate(
      const ScanType scan_type, const ValuePlaceholder &value, const optional<AllTypeVariant> &value2 = nullopt) = 0;

  /**
   * Estimate selectivity for predicate on columns.
   * In comparison to predicates with constants, value is another column.
   * For predicate "col_left < col_right", selectivity is calculated in column statistics of col_left with parameters
   * scan_type = "<" and right_base_column_statistics = col_right statistics.
   * @return Selectivity and two new column statistics, if selectivity is not 0 or 1.
   */
  virtual TwoColumnSelectivityResult estimate_selectivity_for_two_column_predicate(
      const ScanType scan_type, const std::shared_ptr<BaseColumnStatistics> &right_base_column_statistics,
      const optional<AllTypeVariant> &value2 = nullopt) = 0;

  /**
   * Gets distict count of column.
   * See _distinct_count declaration in column_statistics.hpp for explanation of float type.
   */
  virtual float distinct_count() const = 0;

  /**
   * Copies the derived object and returns a base class pointer to it.
   */
  virtual std::shared_ptr<BaseColumnStatistics> clone() const = 0;

  /**
   * Adjust null value ratio of a column after a left/right/full outer join.
   */
  void set_null_value_ratio(const float null_value_ratio) { _non_null_value_ratio = 1.f - null_value_ratio; }

  /**
   * Gets null value ratio of a column for calculation of null values for left/right/full outer join.
   */
  float null_value_ratio() const { return 1.f - _non_null_value_ratio; }

 protected:
  /**
   * Column statistics uses the non-null value ratio for calculation of selectivity.
   */
  float _non_null_value_ratio;

  /**
   * In order to to call insertion operator on ostream with BaseColumnStatistics with values of ColumnStatistics<T>,
   * std::ostream &operator<< with BaseColumnStatistics calls virtual function print_to_stream
   * This approach allows printing ColumnStatistics<T> without the need to cast BaseColumnStatistics to
   * ColumnStatistics<T>.
   */
  virtual std::ostream &print_to_stream(std::ostream &os) const = 0;
  friend std::ostream &operator<<(std::ostream &os, BaseColumnStatistics &obj);
};

/**
 * Return type of selectivity functions for operations on one column.
 */
struct ColumnSelectivityResult {
  float selectivity;
  std::shared_ptr<BaseColumnStatistics> column_statistics;
};

/**
 * Return type of selectivity functions for operations on two columns.
 */
struct TwoColumnSelectivityResult : public ColumnSelectivityResult {
  TwoColumnSelectivityResult(float selectivity, const std::shared_ptr<BaseColumnStatistics> &column_stats,
                             const std::shared_ptr<BaseColumnStatistics> &second_column_stats)
      : ColumnSelectivityResult{selectivity, column_stats}, second_column_statistics(second_column_stats) {}

  std::shared_ptr<BaseColumnStatistics> second_column_statistics;
};

inline std::ostream &operator<<(std::ostream &os, BaseColumnStatistics &obj) { return obj.print_to_stream(os); }

}  // namespace opossum
