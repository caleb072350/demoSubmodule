cmake_minimum_required(VERSION 3.16)
project(demo)

set(CMAKE_CXX_STANDARD 17) # C++ 17
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(abseil-cpp)

add_subdirectory(aklomp-base64)

add_subdirectory(src)