add_executable(test_wget tutorial-01-wget.cc)

find_package(fmt REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../kernel/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../protocol/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../factory/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../util/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../algorithm/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../manager/)

target_link_libraries(test_wget protocol)
target_link_libraries(test_wget factory)
target_link_libraries(test_wget fmt::fmt)