add_executable(timer timer.cpp)
target_link_libraries(timer ${Boost_LIBRARIES})

add_executable(asio_resolver boost_socket.cpp)
target_link_libraries(asio_resolver ${Boost_LIBRARIES})

add_executable(asio_server boost_server.cpp)
target_link_libraries(asio_server ${Boost_LIBRARIES})