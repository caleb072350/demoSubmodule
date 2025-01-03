project(factory)

set(SRC
	HttpTaskImpl.cc
	WFTaskFactory.cc
	Workflow.cc
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../kernel/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../util/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../protocol/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../algorithm/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../manager/)

find_package(fmt REQUIRED)

add_library(${PROJECT_NAME} STATIC ${SRC})
target_link_libraries(${PROJECT_NAME} PRIVATE util)
target_link_libraries(${PROJECT_NAME} PRIVATE protocol)
target_link_libraries(${PROJECT_NAME} PRIVATE kernel)
target_link_libraries(${PROJECT_NAME} PRIVATE manager)
target_link_libraries(${PROJECT_NAME} PRIVATE algorithm)
target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)