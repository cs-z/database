include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

set_target_properties(gtest gtest_main gmock gmock_main PROPERTIES
    CXX_CLANG_TIDY ""
)

add_subdirectory(unit)
add_subdirectory(stress)
