project(algorithm)

set(SRC
	DNSRoutine.cc
)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../util/)
add_library(${PROJECT_NAME} STATIC ${SRC})