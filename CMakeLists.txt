cmake_minimum_required(VERSION 3.0)
project(kv)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++17")

# 配置头文件的搜索路径
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROHECT_SOURCE_DIR}/src/filter)
include_directories(${PROHECT_SOURCE_DIR}/src/utils)


aux_source_directory(. PROJECT_SRC_LIST)
aux_source_directory(./src SRC_LIST)
aux_source_directory(./src/filter SRC_FILTER_LIST)
aux_source_directory(./src/utils SRC_UTILS_LIST)
add_executable(${PROJECT_NAME} ${SRC_UTILS_LIST} ${SRC_FILTER_LIST} ${SRC_LIST} ${PROJECT_SRC_LIST})
target_link_libraries(${PROJECT_NAME} pthread)

enable_testing()

# 添加测试目录
add_subdirectory(tests)