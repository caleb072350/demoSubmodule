add_library(mymodule mymodule.cpp)
target_link_libraries(mymodule absl::strings absl::base)
target_include_directories(mymodule PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../abseil-cpp/) # 注意路径