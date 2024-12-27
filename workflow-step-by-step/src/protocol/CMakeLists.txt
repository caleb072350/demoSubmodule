project(protocol)

set(SRC
	http_parser.c
	HttpUtil.cc
	HttpMessage.cc
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../kernel/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../util/)

find_package(OpenSSL REQUIRED)

add_library(${PROJECT_NAME} STATIC ${SRC})
target_link_libraries(${PROJECT_NAME} util)
target_link_libraries(${PROJECT_NAME} OpenSSL::SSL OpenSSL::Crypto)