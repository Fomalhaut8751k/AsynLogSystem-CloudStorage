#ifndef HTTPSERVER_V2_HTTPSERVER_H
#define HTTPSERVER_V2_HTTPSERVER_H

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <event2/util.h>

#include "HttpContext.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "../middleware/MiddlewareChain.h"
#include "../router/Router.h"
#include "../session/SessionManager.h"
#include "../ssl/SslConfig.h"
#include "../ssl/SslContext.h"

namespace http_v2
{

struct DownloadContext
{
    std::string file_path;
    uint64_t pos{0};
    uint64_t file_size{0};
    bool close_conn{false};
    bool is_temp{false};
};

class HttpServer
{
public:
    enum Option
    {
        kNoReusePort,
        kReusePort
    };

    using HttpCallback = std::function<void(const HttpRequest&, HttpResponse*)>;

    HttpServer(int port, const std::string& name, bool useSSL = false, Option option = kNoReusePort);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void setThreadNum(int numThreads) { threadNum_ = numThreads; }
    void start();
    event_base* getLoop() const { return mainBase_; }

    void setHttpCallback(const HttpCallback& cb) { httpCallback_ = cb; }

    void Get(const std::string& path, const HttpCallback& cb) { router_.registerCallback(HttpRequest::kGet, path, cb); }
    void Get(const std::string& path, router::Router::HandlerPtr handler) { router_.registerHandler(HttpRequest::kGet, path, std::move(handler)); }

    void Post(const std::string& path, const HttpCallback& cb) { router_.registerCallback(HttpRequest::kPost, path, cb); }
    void Post(const std::string& path, router::Router::HandlerPtr handler) { router_.registerHandler(HttpRequest::kPost, path, std::move(handler)); }

    void Delete(const std::string& path, const HttpCallback& cb) { router_.registerCallback(HttpRequest::kDelete, path, cb); }
    void Delete(const std::string& path, router::Router::HandlerPtr handler) { router_.registerHandler(HttpRequest::kDelete, path, std::move(handler)); }

    void addRoute(HttpRequest::Method method, const std::string& path, const router::Router::HandlerPtr& handler) { router_.addRegexHandler(method, path, handler); }
    void addRoute(HttpRequest::Method method, const std::string& path, const router::Router::HandlerCallback& callback) { router_.addRegexCallback(method, path, callback); }

    void setSeesionManager(std::unique_ptr<session::SessionManager> manager) { sessionManager_ = std::move(manager); }
    session::SessionManager* getSessionManager() const { return sessionManager_.get(); }

    void addMiddleware(std::shared_ptr<middleware::Middleware> middleware) { middlewareChain_.addMiddleware(std::move(middleware)); }

    void enableSSL(bool enable) { useSSL_ = enable; }
    void setSslConfig(const ssl::SslConfig& config);

private:
    struct Worker
    {
        event_base* base{nullptr};
        event* notifyEvent{nullptr};
        evutil_socket_t notifyReceive{-1};
        evutil_socket_t notifySend{-1};
        std::thread thread;
        std::mutex mutex;
        std::queue<evutil_socket_t> pendingFds;
        HttpServer* server{nullptr};
    };

    struct ConnectionContext;

    void initialize();
    void initializeWorkers();
    void stopWorkers();
    void dispatchToWorker(evutil_socket_t fd);
    void createConnection(event_base* base, evutil_socket_t fd);

    void onConnection(ConnectionContext* ctx);
    void onMessage(ConnectionContext* ctx);
    void onRequest(ConnectionContext* ctx, const HttpRequest& req, bool sendResponse);
    void handleRequest(const HttpRequest& req, HttpResponse* resp);
    bool sendLargeDownload(ConnectionContext* ctx, const HttpRequest& req, HttpResponse* response, bool close);
    void sendNextDownloadChunk(ConnectionContext* ctx);
    void sendResponse(ConnectionContext* ctx, const HttpResponse& response);
    void closeConnection(ConnectionContext* ctx);

    static void acceptCallback(evconnlistener* listener, evutil_socket_t fd, sockaddr* address, int socklen, void* arg);
    static void acceptErrorCallback(evconnlistener* listener, void* arg);
    static void notifyCallback(evutil_socket_t fd, short events, void* arg);
    static void readCallback(bufferevent* bev, void* arg);
    static void writeCallback(bufferevent* bev, void* arg);
    static void eventCallback(bufferevent* bev, short events, void* arg);

    int port_;
    std::string name_;
    bool useSSL_;
    Option option_;
    int threadNum_{0};
    event_base* mainBase_{nullptr};
    evconnlistener* listener_{nullptr};
    std::vector<std::unique_ptr<Worker>> workers_;
    std::atomic<size_t> nextWorker_{0};
    bool started_{false};

    HttpCallback httpCallback_;
    router::Router router_;
    std::unique_ptr<session::SessionManager> sessionManager_;
    middleware::MiddlewareChain middlewareChain_;
    std::unique_ptr<ssl::SslContext> sslCtx_;

    static constexpr uint64_t LARGE_UPLOAD_THRESHOLD = 256ULL * 1024ULL * 1024ULL;
    static constexpr uint64_t LARGE_DOWNLOAD_THRESHOLD = 1024ULL * 1024ULL;
    static constexpr uint64_t CHUNK_SIZE = 64ULL * 1024ULL;
};

}

#endif
