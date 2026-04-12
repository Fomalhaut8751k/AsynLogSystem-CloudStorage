# cmake_minimum_required(VERSION 3.0)
# project(AsynLogSystem-CloudStorage)

# # 配置编译选项
# set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -g)

# # 设置可执行文件最终存储的路径
# set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# # 配置头文件搜索路径
# include_directories(${PROJECT_SOURCE_DIR}/log_system/include)
# include_directories(${PROJECT_SOURCE_DIR}/log_system/include/backlog)
# include_directories(${PROJECT_SOURCE_DIR}/src/server)
# include_directories(${PROJECT_SOURCE_DIR}/thirdparty)
# include_directories(${PROJECT_SOURCE_DIR}/HttpServer/include/http)
# include_directories(${PROJECT_SOURCE_DIR}/HttpServer/include/middleware)
# include_directories(${PROJECT_SOURCE_DIR}/HttpServer/include/router)
# include_directories(${PROJECT_SOURCE_DIR}/HttpServer/include/session)
# include_directories(${PROJECT_SOURCE_DIR}/HttpServer/include/ssl)
# include_directories(${PROJECT_SOURCE_DIR}/HttpServer/include/utils)
# include_directories(${PROJECT_SOURCE_DIR}/HttpServer/include/utils/db)

# # 加载(源文件的)子目录
# add_subdirectory(src)  # 通过三个CMakeLists.txt从当前目录./加载到./src/server
# # add_subdirectory(HttpServer)

# add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)

cmake_minimum_required(VERSION 3.10)
project(AsynLogSystem-CloudStorage VERSION 1.0.0 LANGUAGES CXX)

# ==================== 全局配置 ====================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Wextra")

# 可执行文件输出路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
# 库文件输出路径
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

# 全局定义
add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)

# ==================== 查找依赖 ====================
# OpenSSL（HttpServer 需要）
find_package(OpenSSL REQUIRED)

# 线程库（几乎所有网络程序都需要）
find_package(Threads REQUIRED)

# MySQL（如果需要）
# find_package(MySQL REQUIRED)

# ==================== 添加子目录 ====================
# 注意顺序：先添加库，后添加应用程序
add_subdirectory(log_system)
add_subdirectory(HttpServer)
add_subdirectory(src)