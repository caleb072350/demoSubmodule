add_subdirectory(mymodule)

add_executable(test main.cpp)

target_link_libraries(test mymodule absl::base absl::strings)
