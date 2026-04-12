# aux_source_directory(. SRC_LIST)

# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

# add_executable(Service ${SRC_LIST})

# target_link_libraries(Service event event_pthreads pthread stdc++fs jsoncpp bundle)

# # 确保 RPATH 设置
# set_target_properties(Service PROPERTIES
#     BUILD_WITH_INSTALL_RPATH TRUE
#     INSTALL_RPATH "$ORIGIN"
#     SKIP_BUILD_RPATH FALSE
# )

# add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)

# ==================== 收集源文件 ====================
aux_source_directory(. SRC_LIST)

# ==================== C++ 标准 ====================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==================== 创建可执行文件 ====================
add_executable(Service ${SRC_LIST})

# ==================== 设置头文件路径 ====================
target_include_directories(Service
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# ==================== 链接库 ====================
target_link_libraries(Service
    PRIVATE
        # 你自己的库
        HttpServer
        log_system
        
        # mymuduo 网络库
        mymuduo

        # 其他可能需要的
        bundle    # 如果 bundle 是某个库，保留；否则删除
        
        # OpenSSL（HttpServer 依赖）
        OpenSSL::SSL
        OpenSSL::Crypto
        
        # 系统库（你原来的）
        event
        event_pthreads
        pthread
        stdc++fs
        jsoncpp

)

# ==================== RPATH 设置 ====================
set_target_properties(Service PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN"
    SKIP_BUILD_RPATH FALSE
)

# ==================== 编译定义 ====================
target_compile_definitions(Service
    PRIVATE
        BOOST_BIND_GLOBAL_PLACEHOLDERS
)

# ==================== 编译选项 ====================
target_compile_options(Service PRIVATE -g -Wall -Wextra)