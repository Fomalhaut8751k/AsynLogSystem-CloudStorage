#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <unordered_map>

#include "mymuduo/TcpConnection.h"
#include "mymuduo/EventLoop.h"
// #include "mymuduo/Alogger.h"
#include "mymuduo/Logger.h"
#include "mymuduo/noncopyable.h"
#include "../utils/JsonUtil.h"

#include "HttpContext.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "../router/Router.h"
#include "../session/SessionManager.h"
#include "../middleware/MiddlewareChain.h"
#include "../middleware/cors/CorsMiddleware.h"
#include "../ssl/SslConnection.h"
#include "../ssl/SslContext.h"
#include "../ssl/SslConfig.h"

class HttpRequest;
class HttpResponse;

namespace http
{

// 大文件分块下载的连接状态，存储在 TcpConnection::downloadContext_ 中
struct DownloadContext {
    std::string file_path;   // 实际读取的文件路径
    uint64_t    pos;         // 下一次读取的起始偏移
    uint64_t    file_size;   // 文件总大小
    bool        close_conn;  // 发完后是否关闭连接
    bool        is_temp;     // 是否是解压出的临时文件，发完后需要删除
};

class HttpServer: noncopyable
{
public:
    using HttpCallback = std::function<void(const http::HttpRequest&, http::HttpResponse*)>;

    // 构造函数
    HttpServer(int port, const std::string& name, bool useSSL = false, TcpServer::Option option = TcpServer::kNoReusePort);

    void setThreadNum(int numThreads) { server_.setThreadNum(numThreads); }

    void start();

    EventLoop* getLoop() const { return server_.getLoop(); }

    void setHttpCallback(const HttpCallback& cb) { httpCallback_ = cb; }

    // 注册静态路由处理器
    void Get(const std::string& path, const HttpCallback& cb) { router_.registerCallback(HttpRequest::kGet, path, cb); }
    void Get(const std::string& path, router::Router::HandlerPtr handler) { router_.registerHandler(HttpRequest::kGet, path, handler); }

    void Post(const std::string& path, const HttpCallback& cb) { router_.registerCallback(HttpRequest::kPost, path, cb); }
    void Post(const std::string& path, router::Router::HandlerPtr handler) { router_.registerHandler(HttpRequest::kPost, path, handler); }

    void Delete(const std::string& path, const HttpCallback& cb) { router_.registerCallback(HttpRequest::kDelete, path, cb); }
    void Delete(const std::string& path, router::Router::HandlerPtr handler) { router_.registerHandler(HttpRequest::kDelete, path, handler); }

    // 注册动态路由处理器
    void addRoute(HttpRequest::Method method, const std::string& path, const router::Router::HandlerPtr& handler) { router_.addRegexHandler(method, path, handler); }
    // 注册动态路由处理函数
    void addRoute(HttpRequest::Method method, const std::string& path, const router::Router::HandlerCallback& callback) { router_.addRegexCallback(method, path, callback); }

    // 设置会话管理器
    void setSeesionManager(std::unique_ptr<session::SessionManager> manager) { sessionManager_ = std::move(manager); }
    // 获取会话管理器
    session::SessionManager* getSessionManager() const { return sessionManager_.get(); }

    // 添加中间件的方法
    void addMiddleware(std::shared_ptr<middleware::Middleware> middleware) { middlewareChain_.addMiddleware(middleware); }

    void enableSSL(bool enable) { useSSL_ = enable; }

    void setSslConfig(const ssl::SslConfig& config);


private:
    void initialize();

    void onConnection(const TcpConnectionPtr& conn);
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, TimeStamp receiveTime);
    void onRequest(const TcpConnectionPtr&, const HttpRequest&);
    std::uint64_t onRequestForDownload(const TcpConnectionPtr&, const HttpRequest&);
    void handleRequest(const HttpRequest& req, HttpResponse* resp);

    void writeCompleteCallback(const TcpConnectionPtr&);

    InetAddress listenAddr_;  // 监听地址
    TcpServer server_;
    EventLoop mainLoop_;  // 主循环
    HttpCallback httpCallback_;  // 回调函数
    router::Router router_;  // 路由
    std::unique_ptr<session::SessionManager> sessionManager_;  // 会话管理器
    middleware::MiddlewareChain middlewareChain_;  // 中间件链
    std::unique_ptr<ssl::SslContext> sslCtx_;  // SSL上下文
    bool useSSL_;  // 是否使用SSL
    std::map<TcpConnectionPtr, std::unique_ptr<ssl::SslConnection>> sslConns_;
    std::uint64_t chunkDownloadSizeThreshold;

    static constexpr uint64_t CHUNK_SIZE = 64 * 1024;  // 每次分块发送 64KB
};


}


#endif