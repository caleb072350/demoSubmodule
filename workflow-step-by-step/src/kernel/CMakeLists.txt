cmake_minimum_required(VERSION 3.16)
project(testpool)

find_package(Threads REQUIRED)

# 设置构建类型为 Debug
set(CMAKE_BUILD_TYPE Debug)

add_executable(testpool testpool.c thrdpool.c list.h)

# 链接 pthread 库
target_link_libraries(testpool PRIVATE ${CMAKE_THREAD_LIBS_INIT})

add_executable(aio_exam linux_aio.cc linux_aio_utils.cc)
# target_link_libraries(aio_exam PRIVATE aio)

add_executable(rbtree rbtreetest.c rbtree.c)

# 查找线程库
find_package(Threads REQUIRED)
# 查找 OpenSSL
find_package(OpenSSL REQUIRED)
add_executable(poller pollertest.c poller.c rbtree.c)
target_link_libraries(poller PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto)

add_executable(client clienttest.c)