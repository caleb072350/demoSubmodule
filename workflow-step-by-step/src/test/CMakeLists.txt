
find_package(fmt REQUIRED)

add_executable(spdlog spdlog.cc)
target_link_libraries(spdlog PRIVATE fmt::fmt)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../kernel/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../factory/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../util/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../manager/)

add_executable(executor executor.cc)
target_link_libraries(executor kernel)

add_executable(threadTask threadtask.cc)
target_link_libraries(threadTask kernel)
target_link_libraries(threadTask factory)
target_link_libraries(threadTask util)

add_executable(timerTask timertasktest.cc)
target_link_libraries(timerTask kernel)
target_link_libraries(timerTask fmt::fmt)
target_link_libraries(timerTask manager)
target_link_libraries(timerTask factory)