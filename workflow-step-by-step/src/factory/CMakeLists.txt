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

add_library(${PROJECT_NAME} STATIC ${SRC})
target_link_libraries(${PROJECT_NAME} util)
target_link_libraries(${PROJECT_NAME} protocol)
target_link_libraries(${PROJECT_NAME} kernel)
target_link_libraries(${PROJECT_NAME} manager)
target_link_libraries(${PROJECT_NAME} algorithm)