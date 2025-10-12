cmake_minimum_required(VERSION 3.0)
project(AsynLogSystem-CloudStorage)

# 配置编译选项
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -g)

# 设置可执行文件最终存储的路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 配置头文件搜索路径
include_directories(${PROJECT_SOURCE_DIR}/log_system/include)
include_directories(${PROJECT_SOURCE_DIR}/log_system/include/backlog)
include_directories(${PROJECT_SOURCE_DIR}/src/server)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty)

# 加载(源文件的)子目录
add_subdirectory(src)  # 通过三个CMakeLists.txt从当前目录./加载到./src/server

add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)