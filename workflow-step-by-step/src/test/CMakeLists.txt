
find_package(fmt REQUIRED)

add_executable(spdlog spdlog.cc)
target_link_libraries(spdlog PRIVATE fmt::fmt)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../kernel/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../factory/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../util/)

add_executable(executor executor.cc)
target_link_libraries(executor kernel)

add_executable(threadTask threadtask.cc)
target_link_libraries(threadTask kernel)
target_link_libraries(threadTask factory)
target_link_libraries(threadTask util)