# ==================== 收集源文件 ====================
# 方法一：手动列出（更精确，推荐）
set(HTTP_SERVER_SOURCES
    # http
    src/http/HttpServer.cpp
    src/http/HttpContext.cpp
    src/http/HttpRequest.cpp
    src/http/HttpResponse.cpp
    
    # middleware
    src/middleware/MiddlewareChain.cpp
    src/middleware/cors/CorsMiddleware.cpp
    
    # router
    src/router/Router.cpp
    
    # session
    src/session/Session.cpp
    src/session/SessionManager.cpp
    src/session/SessionStorage.cpp
    
    # ssl
    src/ssl/SslConfig.cpp
    src/ssl/SslContext.cpp
    src/ssl/SslConnection.cpp
    
    # utils/db
    src/utils/db/DbConnection.cpp
    src/utils/db/DbConnectionPool.cpp

    # src/utils/FlieSender.cpp
)

# 方法二：自动收集（如果文件很多）
# file(GLOB_RECURSE HTTP_SERVER_SOURCES
#     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
# )

# ==================== 创建库 ====================
add_library(HttpServer STATIC ${HTTP_SERVER_SOURCES})

# ==================== 设置头文件路径 ====================
target_include_directories(HttpServer
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ==================== 链接依赖 ====================
target_link_libraries(HttpServer
    PUBLIC
        OpenSSL::SSL
        OpenSSL::Crypto
        mymuduo 
    PRIVATE
        Threads::Threads
        log_system    # 如果 HttpServer 使用了日志系统
        # mysqlclient  # 如果使用 MySQL
        # redis        # 如果使用 Redis
)

# ==================== 编译选项 ====================
target_compile_options(HttpServer PRIVATE -Wall -Wextra)

# ==================== 安装规则（可选） ====================
install(TARGETS HttpServer
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)

install(DIRECTORY include/ DESTINATION include)