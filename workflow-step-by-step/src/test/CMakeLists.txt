
find_package(fmt REQUIRED)

add_executable(spdlog spdlog.cc)
target_link_libraries(spdlog PRIVATE fmt::fmt)