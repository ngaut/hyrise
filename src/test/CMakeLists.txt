set(
    SOURCES
    operators/product_test.cpp
    operators/index_column_scan_test.cpp
    operators/export_binary_test.cpp
    operators/join_test.hpp
    operators/export_csv_test.cpp
    operators/join_full_test.cpp
    operators/difference_test.cpp
    operators/print_test.cpp
    operators/termfactory_test.cpp
    operators/validate_test.cpp
    operators/union_all_test.cpp
    operators/join_equi_test.cpp
    operators/term_test.cpp
    operators/table_scan_test.cpp
    operators/validate_visibility_test.cpp
    operators/insert_test.cpp
    operators/update_test.cpp
    operators/table_scan_like_test.cpp
    operators/rollback_records_test.cpp
    operators/projection_test.cpp
    operators/aggregate_test.cpp
    operators/get_table_test.cpp
    operators/import_binary_test.cpp
    operators/import_csv_test.cpp
    operators/delete_test.cpp
    operators/sort_test.cpp
    base_test.cpp
    scheduler/scheduler_test.cpp
    gtest_case_template.cpp
    base_test.hpp
    lib/all_type_variant_test.cpp
    lib/AllParameterVariant_test.cpp
    network/operator_translator_test.cpp
    network/server_test.cpp
    network/response_builder_test.cpp
    concurrency/transaction_manager_test.cpp
    concurrency/commit_context_test.cpp
    storage/reference_column_test.cpp
    storage/storage_manager_test.cpp
    storage/dictionary_column_test.cpp
    storage/table_test.cpp
    storage/variable_length_key_base_test.cpp
    storage/variable_length_key_store_test.cpp
    storage/multi_column_index_test.cpp
    storage/single_column_index_test.cpp
    storage/adaptive_radix_tree_index_test.cpp
    storage/group_key_index_test.cpp
    storage/composite_group_key_index_test.cpp
    storage/chunk_test.cpp
    storage/variable_length_key_test.cpp
    storage/value_column_test.cpp
    utils/cuckoo_hashtable_test.cpp
    gtest_main.cpp
)

# Both opossumTest and opossumAsan link against these
set(
    LIBRARIES
    gtest
)

# Configure opossumTest
add_executable(opossumTest ${SOURCES})
target_link_libraries(opossumTest opossum ${LIBRARIES})

# Configure opossumCoverageApp
add_executable(opossumCoverageApp ${SOURCES})
target_link_libraries(opossumCoverageApp opossumCoverage ${LIBRARIES} --coverage)
set_target_properties(opossumCoverageApp PROPERTIES COMPILE_FLAGS "-fprofile-arcs -ftest-coverage")

# Configure opossumAsanApp
add_executable(opossumAsanApp ${SOURCES})
target_link_libraries(opossumAsanApp opossumAsan ${LIBRARIES} -fsanitize=address)
set_target_properties(opossumAsanApp PROPERTIES COMPILE_FLAGS "-fsanitize=address -fno-omit-frame-pointer")


