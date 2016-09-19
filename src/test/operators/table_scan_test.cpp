#include <iostream>
#include <memory>
#include <utility>

#include "gtest/gtest.h"

#include "../../lib/operators/abstract_operator.hpp"
#include "../../lib/operators/get_table.hpp"
#include "../../lib/operators/print.hpp"
#include "../../lib/operators/table_scan.hpp"
#include "../../lib/storage/storage_manager.hpp"
#include "../../lib/storage/table.hpp"
#include "../../lib/types.hpp"

namespace opossum {

class operators_table_scan : public ::testing::Test {
  virtual void SetUp() {
    test_table = std::make_shared<opossum::Table>(opossum::Table(2));

    test_table->add_column("a", "int");
    test_table->add_column("b", "float");

    test_table->append({123, 456.7f});
    test_table->append({1234, 457.7f});
    test_table->append({12345, 458.7f});

    opossum::StorageManager::get().add_table("table_a", std::move(test_table));

    gt = std::make_shared<opossum::GetTable>("table_a");
  }

 public:
  std::shared_ptr<opossum::Table> test_table;
  std::shared_ptr<opossum::GetTable> gt;
};

TEST_F(operators_table_scan, double_scan_test) {
  auto scan_1 = std::make_shared<opossum::TableScan>(gt, "a", ">=", 1234);
  scan_1->execute();

  auto scan_2 = std::make_shared<opossum::TableScan>(scan_1, "b", "<", 457.9);
  scan_2->execute();

  EXPECT_EQ(type_cast<int>((*(scan_1->get_output()->get_chunk(0).get_column(0)))[0]), 1234);
  EXPECT_EQ(type_cast<float>((*(scan_1->get_output()->get_chunk(0).get_column(1)))[1]), 458.7f);

  EXPECT_EQ(scan_2->get_output()->row_count(), (u_int)1);
}

class operators_table_scan_impl : public ::testing::Test {
  virtual void SetUp() {
    test_table = std::make_shared<opossum::Table>(opossum::Table(2));

    test_table->add_column("a", "int");
    test_table->add_column("b", "float");

    test_table->append({123, 456.7f});
    test_table->append({1234, 457.7f});
    test_table->append({12345, 458.7f});

    opossum::StorageManager::get().add_table("table_a", std::move(test_table));

    gt = std::make_shared<opossum::GetTable>("table_a");
  }

 public:
  std::shared_ptr<opossum::Table> test_table;
  std::shared_ptr<opossum::GetTable> gt;
};

TEST_F(operators_table_scan_impl, single_scan_returns_correct_row_count) {
  std::unique_ptr<AbstractOperatorImpl> scan(
      make_unique_by_column_type<AbstractOperatorImpl, TableScanImpl>("int", gt, "a", ">=", 1234));
  scan->execute();

  EXPECT_EQ(type_cast<int>((*(scan->get_output()->get_chunk(0).get_column(0)))[0]), 1234);
  EXPECT_EQ(type_cast<int>((*(scan->get_output()->get_chunk(0).get_column(0)))[1]), 12345);

  EXPECT_EQ(scan->get_output()->row_count(), (uint32_t)2);
}

TEST_F(operators_table_scan_impl, unknown_operator_throws_exception) {
  std::unique_ptr<AbstractOperatorImpl> scan(
      make_unique_by_column_type<AbstractOperatorImpl, TableScanImpl>("int", gt, "a", "xor", 10));

  EXPECT_THROW(scan->execute(), std::exception);
}

}  // namespace opossum