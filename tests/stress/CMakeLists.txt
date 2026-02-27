add_executable(stress_tests
    cache.cpp
)

target_link_libraries(stress_tests PRIVATE
    GTest::gtest_main
    database_lib
    warnings
)

gtest_discover_tests(stress_tests
    PROPERTIES LABELS "stress"
)
