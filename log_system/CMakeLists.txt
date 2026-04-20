# header-only 库，不需要编译
add_library(log_system INTERFACE)

# 设置头文件搜索路径
target_include_directories(log_system
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/backlog
)

# 如果有任何编译定义需要传递给使用者
target_compile_definitions(log_system
    INTERFACE
        # 例如：-DLOG_USE_ASYNC
)

# 如果 log_system 依赖其他库（如 pthread）
target_link_libraries(log_system
    INTERFACE
        Threads::Threads
)