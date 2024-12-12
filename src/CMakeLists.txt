add_subdirectory(mymodule)

add_executable(test main.cpp)

target_link_libraries(test mymodule absl::base absl::strings)

add_subdirectory(base64module)

target_include_directories(test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/base64module)
target_include_directories(test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/mymodule)
target_include_directories(test PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/../googletest/googletest/include)

target_link_libraries(test mybase64 gtest gtest_main)

enable_testing()
# 添加测试
add_executable(myProjectTest test/test_main.cpp)
target_link_libraries(myProjectTest gtest gtest_main)
add_test(NAME test1 COMMAND myProjectTest)


# 添加测试
add_executable(googleTestF test/test_testF.cpp)
target_link_libraries(googleTestF gtest gtest_main)
add_test(NAME test2 COMMAND googleTestF)


