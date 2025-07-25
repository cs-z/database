cmake_minimum_required(VERSION 3.16)

project(database LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_BUILD_TYPE Debug)

file(GLOB SOURCES "src/*.cpp")
add_executable(database ${SOURCES})

target_include_directories(database PRIVATE include)

target_compile_options(database PRIVATE -Wall -Wextra -Wpedantic)
