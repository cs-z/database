cmake_minimum_required(VERSION 3.20)

project(database LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/include/*.hpp"
)

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
    target_compile_options(database PRIVATE
        -fsanitize=address
    )
    target_link_options(database PRIVATE
        -fsanitize=address
    )
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if (CLANG_TIDY_EXE)
        set_target_properties(database PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_EXE}"
        )
    endif()
endif()

add_custom_target(formatCheck
    COMMAND ${CMAKE_COMMAND} -E echo "Checking code formatting..."
    COMMAND clang-format --dry-run --Werror ${SOURCES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_custom_target(formatFix
    COMMAND ${CMAKE_COMMAND} -E echo "Fixing code formatting..."
    COMMAND clang-format -i ${SOURCES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
