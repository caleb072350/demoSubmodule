add_library(mybase64 base64.cpp)
target_link_libraries(mybase64 base64)
target_include_directories(mybase64 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../aklomp-base64/include) # 注意路径