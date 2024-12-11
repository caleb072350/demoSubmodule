add_subdirectory(mymodule)

add_executable(test main.cpp)

target_link_libraries(test mymodule absl::base absl::strings)

add_subdirectory(base64module)

target_include_directories(test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/base64module)

target_link_libraries(test mybase64)
