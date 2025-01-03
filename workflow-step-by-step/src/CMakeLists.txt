cmake_minimum_required(VERSION 3.16)
project(demo)

set(CMAKE_CXX_STANDARD 17) # C++ 17
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Debug)

add_subdirectory(tutorial)
add_subdirectory(util)
add_subdirectory(factory)
add_subdirectory(kernel)
add_subdirectory(manager)
add_subdirectory(protocol)
add_subdirectory(algorithm)
add_subdirectory(test)
