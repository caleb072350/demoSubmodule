project(util)

set(SRC
	EncodeStream.cc
	StringUtil.cc
	URIParser.cc
	MD5Util.cc
	logger.cc
)

find_package(OpenSSL REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../kernel/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../protocol/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../algorithm/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../manager/)

add_library(${PROJECT_NAME} STATIC ${SRC})
target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::Crypto OpenSSL::SSL)