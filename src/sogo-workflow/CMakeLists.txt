add_executable(helloworld helloworld.cpp)
target_link_libraries(helloworld workflow)

add_executable(mywget wget.cpp)
target_link_libraries(mywget workflow)

add_executable(myredis-cli redis_cli.cpp)
target_link_libraries(myredis-cli workflow)

add_executable(wget_to_redis wget_to_redis.cpp)
target_link_libraries(wget_to_redis workflow)