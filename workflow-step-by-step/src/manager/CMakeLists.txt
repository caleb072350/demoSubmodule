cmake_minimum_required(VERSION 3.16)
project(manager)

set(SRC
	DNSCache.cc
	RouteManager.cc
	UpstreamManager.cc
	WFGlobal.cc
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../util/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../kernel/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../factory/)

find_package(fmt REQUIRED)

add_library(${PROJECT_NAME} STATIC ${SRC})
target_link_libraries(${PROJECT_NAME} PRIVATE kernel)
target_link_libraries(${PROJECT_NAME} PRIVATE util)
target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)