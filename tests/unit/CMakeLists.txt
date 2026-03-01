add_executable(unit_tests
    cache.cpp
    posix_file.cpp
)

target_link_libraries(unit_tests PRIVATE
    GTest::gtest_main
    database_lib
    warnings
)

gtest_discover_tests(unit_tests
    PROPERTIES LABELS "unit"
)
