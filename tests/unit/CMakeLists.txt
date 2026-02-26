find_package(GTest REQUIRED)

add_executable(unit_tests
    Dummy.cpp
)

target_link_libraries(unit_tests PRIVATE
    GTest::gtest_main
    database_lib
    warnings
)

include(GoogleTest)
gtest_discover_tests(unit_tests)
