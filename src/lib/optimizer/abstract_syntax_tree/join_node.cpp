#include "join_node.hpp"

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "common.hpp"
#include "constant_mappings.hpp"
#include "types.hpp"

namespace opossum {

JoinNode::JoinNode(optional<std::pair<ColumnID, ColumnID>> join_column_ids, const ScanType scan_type,
                   const JoinMode join_mode)
    : AbstractASTNode(ASTNodeType::Join),
      _join_column_ids(join_column_ids),
      _scan_type(scan_type),
      _join_mode(join_mode) {}

std::string JoinNode::description() const {
  std::ostringstream desc;

  desc << "Join";
  desc << " [" << join_mode_to_string.at(_join_mode) << "]";

  if (_join_column_ids) {
    desc << " [" << (*_join_column_ids).first;
    desc << " " << scan_type_to_string.left.at(_scan_type);
    desc << " " << (*_join_column_ids).second << "]";
  }

  return desc.str();
}

const std::vector<ColumnID> JoinNode::output_column_ids() const {
  /**
   * Add respective prefix to column names.
   */
  std::vector<std::string> output_column_ids;

  for (auto &column_id : left_child()->output_column_ids()) {
    _output_column_ids.push_back(column_id);
  }

  for (auto &column_id : right_child()->output_column_ids()) {
    _output_column_ids.push_back(column_id);
  }

  return _output_column_ids;
}

const optional<ColumnID> JoinNode::find_column_id_for_column_identifier(ColumnIdentifier &column_identifier) const {
  if (left_child()->table_identifier() == column_identifier.table_name) {
    return left_child()->find_column_id_for_column_identifier(column_identifier);
  }
  if (right_child()->table_identifier() == column_identifier.table_name) {
    auto num_left_columns = left_child()->output_column_ids().size();
    auto found = right_child()->find_column_id_for_column_identifier(column_identifier);
    if (found) {
      return ColumnID{(*found) + num_left_columns};
    }
  }
  return nullopt;
}

optional<std::pair<ColumnID, ColumnID>> JoinNode::join_column_ids() const { return _join_column_ids; }

ScanType JoinNode::scan_type() const { return _scan_type; }

JoinMode JoinNode::join_mode() const { return _join_mode; }

}  // namespace opossum