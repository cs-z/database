cmake_minimum_required(VERSION 3.20)
project(database LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_WERROR "Treat compiler warnings as errors" ON)
option(ENABLE_CLANG_TIDY "Enable static analysis with clang-tidy" OFF)

enable_testing()

# Targets linking to this will inherit compile flags.
add_library(warnings INTERFACE)
if(MSVC)
    target_compile_options(warnings INTERFACE /W4 /permissive-)
    if(ENABLE_WERROR)
        message("WERROR ENABLED")
        target_compile_options(warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(warnings INTERFACE -Wall -Wextra -Wpedantic)
    if(ENABLE_WERROR)
        message("WERROR ENABLED")
        target_compile_options(warnings INTERFACE -Werror)
    endif()
endif()

if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
    if(NOT CLANG_TIDY_EXE)
        message(WARNING "clang-tidy not found. Static analysis will be disabled.")
        set(ENABLE_CLANG_TIDY OFF)
    endif()
endif()
if(ENABLE_CLANG_TIDY)
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXE};-warnings-as-errors=*)
endif()

add_subdirectory(src)
add_subdirectory(tests)

find_program(CLANG_FORMAT_EXE NAMES "clang-format")
if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE SOURCE_FILES
        CONFIGURE_DEPENDS
        "src/*.cpp" "src/*.hpp"
        "tests/*.cpp" "tests/*.hpp"
    )
    add_custom_target(formatFix
        COMMAND ${CLANG_FORMAT_EXE} --version
        COMMAND ${CLANG_FORMAT_EXE} -i ${SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    add_custom_target(formatCheck
        COMMAND ${CLANG_FORMAT_EXE} --version
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
else()
    message(WARNING "clang-format not found. Formatting targets will not be available.")
endif()
