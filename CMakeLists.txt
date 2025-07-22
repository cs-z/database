cmake_minimum_required(VERSION 3.16)

project(database LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(database
    src/ast.cpp
    src/common.cpp
    src/error.cpp
    src/lexer.cpp
    src/main.cpp
    src/op.cpp
    src/parse.cpp
    src/token.cpp
    src/type.cpp
    src/value.cpp
    src/expr.cpp
)

target_include_directories(database PRIVATE include)

target_compile_options(database PRIVATE -Wall -Wextra -Wpedantic)
