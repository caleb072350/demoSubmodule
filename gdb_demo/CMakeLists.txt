cmake_minimum_required(VERSION 3.10)
project(gdbDemo)

# 设置构建类型为 Debug
set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_STANDARD 11)

add_executable(gdbDemo main.cpp)