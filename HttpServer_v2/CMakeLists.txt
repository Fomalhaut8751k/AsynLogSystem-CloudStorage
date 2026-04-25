set(HTTP_SERVER_V2_SOURCES
    src/http/HttpContext.cpp
    src/http/HttpRequest.cpp
    src/http/HttpResponse.cpp
    src/http/HttpServer.cpp
    src/middleware/MiddlewareChain.cpp
    src/middleware/cors/CorsMiddleware.cpp
    src/router/Router.cpp
    src/session/Session.cpp
    src/session/SessionManager.cpp
    src/session/SessionStorage.cpp
    src/ssl/SslConfig.cpp
    src/ssl/SslConnection.cpp
    src/ssl/SslContext.cpp
)

add_library(HttpServer_v2 STATIC ${HTTP_SERVER_V2_SOURCES})

target_include_directories(HttpServer_v2
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(HttpServer_v2
    PUBLIC
        event
        event_pthreads
        Threads::Threads
)

target_compile_features(HttpServer_v2 PUBLIC cxx_std_17)
target_compile_options(HttpServer_v2 PRIVATE -Wall -Wextra)

install(TARGETS HttpServer_v2
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include)
