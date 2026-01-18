cmake_minimum_required(VERSION 3.20)

project(database LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(database ${SOURCES})

target_include_directories(database PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(database PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    # sanitizer
    target_compile_options(database PRIVATE
        -fsanitize=address
    )
    target_link_options(database PRIVATE
        -fsanitize=address
    )
    # clang-tidy
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if (CLANG_TIDY_EXE)
        set_target_properties(database PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_EXE}"
        )
    endif()
endif()
