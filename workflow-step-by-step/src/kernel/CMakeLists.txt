cmake_minimum_required(VERSION 3.16)
project(testpool)

find_package(Threads REQUIRED)

add_executable(testpool testpool.c thrdpool.c list.h)

# 链接 pthread 库
target_link_libraries(testpool PRIVATE ${CMAKE_THREAD_LIBS_INIT})