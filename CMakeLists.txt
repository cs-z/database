cmake_minimum_required(VERSION 3.16)

project(database LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(include)

add_executable(database
	src/main.cpp
	src/common.cpp
	src/error.cpp
	src/type.cpp
)
