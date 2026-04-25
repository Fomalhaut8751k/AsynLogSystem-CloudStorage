#include "../../include/http/HttpServer.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/thread.h>

namespace http_v2
{

namespace
{

const char* libeventSeverityName(int severity)
{
    switch(severity)
    {
    case EVENT_LOG_DEBUG: return "DEBUG";
    case EVENT_LOG_MSG: return "INFO";
    case EVENT_LOG_WARN: return "WARN";
    case EVENT_LOG_ERR: return "ERROR";
    default: return "UNKNOWN";
    }
}

void consoleLog(const std::string& source, const std::string& message)
{
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    std::cout << "[" << source << "] " << message << std::endl;
}

void libeventLogCallback(int severity, const char* msg)
{
    consoleLog(std::string("libevent/") + libeventSeverityName(severity), msg ? msg : "");
}

void installLibeventLogCallback()
{
    static std::once_flag once;
    std::call_once(once, []() {
        event_set_log_callback(&libeventLogCallback);
        consoleLog("HttpServer_v2", "libevent log callback installed");
    });
}

void defaultHttpCallback(const HttpRequest&, HttpResponse* resp)
{
    resp->setStatusLine("HTTP/1.1", HttpResponse::k404NotFound, "Not Found");
    resp->setCloseConnection(true);
}

void writeAllNotify(evutil_socket_t fd)
{
    char byte = 'x';
    (void)::send(fd, &byte, 1, 0);
}

}

struct HttpServer::ConnectionContext
{
    HttpServer* server{nullptr};
    bufferevent* bev{nullptr};
    HttpContext httpContext;
    Buffer input;
    std::shared_ptr<DownloadContext> download;
    bool closeAfterWrite{false};
    uint64_t streamedUploadReceived{0};
};

HttpServer::HttpServer(int port, const std::string& name, bool useSSL, Option option):
    port_(port),
    name_(name),
    useSSL_(useSSL),
    option_(option),
    httpCallback_(defaultHttpCallback)
{
    installLibeventLogCallback();
    evthread_use_pthreads();
    mainBase_ = event_base_new();
    if(!mainBase_)
    {
        throw std::runtime_error("event_base_new failed");
    }
    if(useSSL_)
    {
        consoleLog("HttpServer_v2", "SSL requested, but the libevent SSL path is not implemented; running plain HTTP");
        useSSL_ = false;
    }
    consoleLog("HttpServer_v2", "server object created: name=" + name_ + ", port=" + std::to_string(port_));
}

HttpServer::~HttpServer()
{
    consoleLog("HttpServer_v2", "server shutting down");
    if(listener_) evconnlistener_free(listener_);
    if(mainBase_) event_base_loopbreak(mainBase_);
    stopWorkers();
    if(mainBase_) event_base_free(mainBase_);
}

void HttpServer::setSslConfig(const ssl::SslConfig& config)
{
    sslCtx_ = std::make_unique<ssl::SslContext>(config);
    if(useSSL_ && !sslCtx_->initialize())
    {
        consoleLog("HttpServer_v2", "SSL config ignored because SSL is not implemented");
        useSSL_ = false;
    }
}

void HttpServer::start()
{
    if(started_) return;
    started_ = true;

    consoleLog("HttpServer_v2", "starting server, worker threads=" + std::to_string(std::max(0, threadNum_)));
    initializeWorkers();

    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(0);
    sin.sin_port = htons(static_cast<uint16_t>(port_));

    int flags = LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE;
#ifdef LEV_OPT_REUSEABLE_PORT
    if(option_ == kReusePort) flags |= LEV_OPT_REUSEABLE_PORT;
#endif

    listener_ = evconnlistener_new_bind(
        mainBase_,
        &HttpServer::acceptCallback,
        this,
        flags,
        -1,
        reinterpret_cast<sockaddr*>(&sin),
        sizeof(sin));

    if(!listener_)
    {
        throw std::runtime_error("evconnlistener_new_bind failed on port " + std::to_string(port_));
    }
    evconnlistener_set_error_cb(listener_, &HttpServer::acceptErrorCallback);

    consoleLog("HttpServer_v2", "listening on 0.0.0.0:" + std::to_string(port_));
    event_base_dispatch(mainBase_);
}

void HttpServer::initializeWorkers()
{
    int count = std::max(0, threadNum_);
    if(count == 0)
    {
        consoleLog("HttpServer_v2", "worker mode disabled; connections run on main event_base");
    }
    for(int i = 0; i < count; ++i)
    {
        auto worker = std::make_unique<Worker>();
        worker->server = this;
        worker->base = event_base_new();
        if(!worker->base)
        {
            throw std::runtime_error("event_base_new failed for worker");
        }

        evutil_socket_t fds[2];
        if(evutil_socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0)
        {
            event_base_free(worker->base);
            throw std::runtime_error("evutil_socketpair failed");
        }
        worker->notifyReceive = fds[0];
        worker->notifySend = fds[1];
        evutil_make_socket_nonblocking(worker->notifyReceive);
        evutil_make_socket_nonblocking(worker->notifySend);

        worker->notifyEvent = event_new(worker->base, worker->notifyReceive, EV_READ | EV_PERSIST, &HttpServer::notifyCallback, worker.get());
        event_add(worker->notifyEvent, nullptr);

        worker->thread = std::thread([base = worker->base]() {
            event_base_dispatch(base);
        });

        consoleLog("HttpServer_v2", "worker event loop started: index=" + std::to_string(i));
        workers_.push_back(std::move(worker));
    }
}

void HttpServer::stopWorkers()
{
    for(auto& worker: workers_)
    {
        if(worker->base) event_base_loopbreak(worker->base);
        if(worker->notifySend != -1) writeAllNotify(worker->notifySend);
    }

    for(auto& worker: workers_)
    {
        if(worker->thread.joinable()) worker->thread.join();
        if(worker->notifyEvent) event_free(worker->notifyEvent);
        if(worker->notifyReceive != -1) evutil_closesocket(worker->notifyReceive);
        if(worker->notifySend != -1) evutil_closesocket(worker->notifySend);
        if(worker->base) event_base_free(worker->base);
    }
    workers_.clear();
}

void HttpServer::acceptCallback(evconnlistener*, evutil_socket_t fd, sockaddr*, int, void* arg)
{
    auto* server = static_cast<HttpServer*>(arg);
    evutil_make_socket_nonblocking(fd);
    consoleLog("HttpServer_v2", "accepted connection fd=" + std::to_string(fd));
    server->dispatchToWorker(fd);
}

void HttpServer::acceptErrorCallback(evconnlistener* listener, void* arg)
{
    auto* server = static_cast<HttpServer*>(arg);
    int err = EVUTIL_SOCKET_ERROR();
    consoleLog("HttpServer_v2", std::string("accept error: ") + evutil_socket_error_to_string(err));
    event_base_loopexit(evconnlistener_get_base(listener), nullptr);
    server->started_ = false;
}

void HttpServer::dispatchToWorker(evutil_socket_t fd)
{
    if(workers_.empty())
    {
        consoleLog("HttpServer_v2", "dispatch connection to main event loop");
        createConnection(mainBase_, fd);
        return;
    }

    size_t workerIndex = nextWorker_++ % workers_.size();
    Worker* worker = workers_[workerIndex].get();
    {
        std::lock_guard<std::mutex> lock(worker->mutex);
        worker->pendingFds.push(fd);
    }
    consoleLog("HttpServer_v2", "dispatch connection fd=" + std::to_string(fd) + " to worker=" + std::to_string(workerIndex));
    writeAllNotify(worker->notifySend);
}

void HttpServer::notifyCallback(evutil_socket_t fd, short, void* arg)
{
    auto* worker = static_cast<Worker*>(arg);
    char buf[64];
    while(::recv(fd, buf, sizeof(buf), 0) > 0) {}

    while(true)
    {
        evutil_socket_t clientFd = -1;
        {
            std::lock_guard<std::mutex> lock(worker->mutex);
            if(worker->pendingFds.empty()) break;
            clientFd = worker->pendingFds.front();
            worker->pendingFds.pop();
        }
        consoleLog("HttpServer_v2", "worker creates connection fd=" + std::to_string(clientFd));
        worker->server->createConnection(worker->base, clientFd);
    }
}

void HttpServer::createConnection(event_base* base, evutil_socket_t fd)
{
    // 每条 TCP 连接都有独立上下文，保存 HTTP 解析状态、输入缓冲区和大文件下载状态。
    auto* ctx = new ConnectionContext();
    ctx->server = this;

    // BEV_OPT_CLOSE_ON_FREE 让 bufferevent_free() 同时关闭 fd，避免手动重复管理 socket 生命周期。
    ctx->bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    if(!ctx->bev)
    {
        consoleLog("HttpServer_v2", "bufferevent_socket_new failed for fd=" + std::to_string(fd));
        evutil_closesocket(fd);
        delete ctx;
        return;
    }
    consoleLog("HttpServer_v2", "connection context created fd=" + std::to_string(fd));

    // read 负责解析请求，write 负责大文件下载续传和延迟关闭，event 负责异常/对端关闭清理。
    bufferevent_setcb(ctx->bev, &HttpServer::readCallback, &HttpServer::writeCallback, &HttpServer::eventCallback, ctx);

    // 同时打开读写事件；写回调只会在输出缓冲区排空等条件满足时由 libevent 触发。
    bufferevent_enable(ctx->bev, EV_READ | EV_WRITE);
    onConnection(ctx);
}

void HttpServer::onConnection(ConnectionContext*)
{
}

void HttpServer::readCallback(bufferevent*, void* arg)
{
    auto* ctx = static_cast<ConnectionContext*>(arg);
    ctx->server->onMessage(ctx);
}

void HttpServer::writeCallback(bufferevent*, void* arg)
{
    auto* ctx = static_cast<ConnectionContext*>(arg);

    // 大文件下载时，写完成回调就是“发送下一块”的节流点，避免一次性把整个文件塞进输出缓冲区。
    if(ctx->download && ctx->download->file_size > 0)
    {
        consoleLog("HttpServer_v2", "write complete; continue large download");
        ctx->server->sendNextDownloadChunk(ctx);
        return;
    }

    // 普通响应如果要求短连接，必须等输出缓冲区完全写完后再关闭，否则响应可能被截断。
    evbuffer* output = bufferevent_get_output(ctx->bev);
    if(ctx->closeAfterWrite && evbuffer_get_length(output) == 0)
    {
        consoleLog("HttpServer_v2", "output drained; closing connection");
        ctx->server->closeConnection(ctx);
    }
}

void HttpServer::eventCallback(bufferevent*, short events, void* arg)
{
    auto* ctx = static_cast<ConnectionContext*>(arg);
    if(events & (BEV_EVENT_EOF | BEV_EVENT_ERROR | BEV_EVENT_TIMEOUT))
    {
        if(events & BEV_EVENT_EOF) consoleLog("HttpServer_v2", "connection closed by peer");
        if(events & BEV_EVENT_ERROR) consoleLog("HttpServer_v2", "connection event error");
        if(events & BEV_EVENT_TIMEOUT) consoleLog("HttpServer_v2", "connection timeout");
        ctx->server->closeConnection(ctx);
    }
}

void HttpServer::onMessage(ConnectionContext* ctx)
{
    evbuffer* input = bufferevent_get_input(ctx->bev);
    while(evbuffer_get_length(input) > 0)
    {
        char tmp[8192];
        int n = evbuffer_remove(input, tmp, sizeof(tmp));
        if(n > 0) ctx->input.append(tmp, static_cast<size_t>(n));
    }
    if(ctx->input.readableBytes() > 0)
    {
        consoleLog("HttpServer_v2", "read bytes buffered=" + std::to_string(ctx->input.readableBytes()));
    }

    while(ctx->input.readableBytes() > 0)
    {   // 尝试解析HTTP请求报文
        int ret = ctx->httpContext.parseRequest(&ctx->input, TimeStamp::now());
        if(ret <= 0)
        {
            HttpResponse response(true);
            response.setStatusLine("HTTP/1.1", HttpResponse::k400BadRequest, "Bad Request");
            sendResponse(ctx, response);
            ctx->closeAfterWrite = true;
            consoleLog("HttpServer_v2", "bad request; scheduled close");
            return;
        }

        HttpRequest& request = ctx->httpContext.request();
        // 如果已经解析了HTTP请求头，并且请求体长度大于LARGE_UPLOAD_THRESHOLD，则认为是一个大文件上传请求
        if(ctx->httpContext.gotHeader() && request.contentLength() > LARGE_UPLOAD_THRESHOLD)
        {
            size_t len = ctx->input.readableBytes();
            if(len == 0) return;

            uint64_t totalLength = request.contentLength();
            std::string part(ctx->input.peek(), ctx->input.peek() + len);
            ctx->input.retrieve(len);
            request.setBody(part);

            bool firstChunk = (ctx->streamedUploadReceived == 0);
            ctx->streamedUploadReceived += len;
            if(firstChunk)
            {
                request.setContentLength(len);
            }

            bool finalChunk = ctx->streamedUploadReceived >= totalLength;
            consoleLog("HttpServer_v2", "large upload chunk: bytes=" + std::to_string(len) +
                ", received=" + std::to_string(ctx->streamedUploadReceived) +
                "/" + std::to_string(totalLength) +
                (finalChunk ? ", final=true" : ", final=false"));
            onRequest(ctx, request, finalChunk);
            request.setContentLength(totalLength);

            if(finalChunk)
            {
                ctx->streamedUploadReceived = 0;
                ctx->httpContext.reset();
            }
            continue;
        }

        if(ctx->httpContext.gotAll())
        {
            consoleLog("HttpServer_v2", "request complete: path=" + request.path() + ", body=" + std::to_string(request.getBody().size()));
            onRequest(ctx, request, true);
            if(!ctx->download)
            {
                ctx->httpContext.reset();
            }
            continue;
        }

        break;
    }
}

void HttpServer::onRequest(ConnectionContext* ctx, const HttpRequest& req, bool sendNow)
{
    const std::string connection = req.getHeader("Connection");
    bool close = (connection == "close") || (req.getVersion() == "HTTP/1.0" && connection != "Keep-Alive");
    HttpResponse response(close);

    if(req.method() == HttpRequest::kGet && req.path() == "/download")
    {
        std::string fileSizeHeader = req.getHeader("FileSize");
        if(fileSizeHeader.empty()) fileSizeHeader = req.getQueryParameters("filesize");
        uint64_t fileSize = fileSizeHeader.empty() ? 0 : std::stoull(fileSizeHeader);
        if(fileSize >= LARGE_DOWNLOAD_THRESHOLD)
        {
            consoleLog("HttpServer_v2", "large download requested: size=" + std::to_string(fileSize));
            if(sendLargeDownload(ctx, req, &response, close))
            {
                return;
            }
        }
    }

    handleRequest(req, &response);
    if(sendNow)
    {
        consoleLog("HttpServer_v2", "send response: path=" + req.path());
        sendResponse(ctx, response);
        if(response.closeConnection())
        {
            ctx->closeAfterWrite = true;
        }
    }
}

bool HttpServer::sendLargeDownload(ConnectionContext* ctx, const HttpRequest& req, HttpResponse* response, bool close)
{
    // 告诉业务层只做下载准备：查元数据、必要时解压，并通过响应头回传真实文件路径。
    response->setBody("chunk_prepare");
    handleRequest(req, response);

    std::string filePath = response->getHeader("X-File-Path");
    // 业务层没有给出文件路径，通常代表文件不存在或准备失败，直接发送业务层生成的错误响应。
    if(filePath.empty())
    {
        consoleLog("HttpServer_v2", "large download prepare failed; sending normal response");
        sendResponse(ctx, *response);
        ctx->closeAfterWrite = response->closeConnection();
        return true;
    }

    // 下载状态挂在连接上；后续 writeCallback 会继续使用这些偏移和清理信息发送下一块。
    auto download = std::make_shared<DownloadContext>();
    download->file_path = filePath;
    std::string requestedSize = req.getHeader("FileSize");
    if(requestedSize.empty()) requestedSize = req.getQueryParameters("filesize");
    download->file_size = requestedSize.empty() ? 0 : std::stoull(requestedSize);
    struct stat st{};
    if(::stat(filePath.c_str(), &st) == 0 && st.st_size >= 0)
    {
        uint64_t actualSize = static_cast<uint64_t>(st.st_size);
        if(download->file_size != actualSize)
        {
            consoleLog("HttpServer_v2", "large download size corrected: requested=" +
                std::to_string(download->file_size) + ", actual=" + std::to_string(actualSize));
        }
        download->file_size = actualSize;
    }
    download->close_conn = close;
    download->is_temp = response->getHeader("X-Is-Temp") == "1";
    ctx->download = download;
    consoleLog("HttpServer_v2", "large download prepared: file=" + filePath +
        ", size=" + std::to_string(download->file_size) +
        (download->is_temp ? ", temp=true" : ", temp=false"));

    // X-File-Path/X-Is-Temp 只用于服务端内部控制，不能写入客户端响应头。
    response->removeHeader("X-File-Path");
    response->removeHeader("X-Is-Temp");
    response->setStatusLine(req.getVersion(), HttpResponse::k200Ok, "OK");
    response->addHeader("Content-Length", std::to_string(download->file_size));
    response->addHeader("Content-Type", "application/octet-stream");

    // 大文件响应先只发送 HTTP 头，body 由 sendNextDownloadChunk() 分批写入 bufferevent。
    Buffer header;
    response->appendToBufferWithoutBody(&header);
    bufferevent_write(ctx->bev, header.peek(), header.readableBytes());

    // 立即发送第一块；之后由 libevent 写完成回调驱动下一块，避免一次性堆满输出缓冲区。
    sendNextDownloadChunk(ctx);
    return true;
}

void HttpServer::sendNextDownloadChunk(ConnectionContext* ctx)
{
    if(!ctx->download) return;
    std::shared_ptr<DownloadContext> download = ctx->download;

    // 所有文件块已经写入 libevent 输出缓冲区，清理连接上的下载状态。
    if(download->pos >= download->file_size)
    {
        // 深度存储下载会先解压出临时文件；最后一块发送后才能删除它。
        if(download->is_temp)
        {
            ::remove(download->file_path.c_str());
            consoleLog("HttpServer_v2", "large download temp file removed: " + download->file_path);
        }
        consoleLog("HttpServer_v2", "large download complete: file=" + download->file_path);
        bool close = download->close_conn;
        ctx->download.reset();
        ctx->httpContext.reset();
        if(close)
        {
            ctx->closeAfterWrite = true;
            evbuffer* output = bufferevent_get_output(ctx->bev);
            if(evbuffer_get_length(output) == 0)
            {
                closeConnection(ctx);
            }
        }
        return;
    }

    // 每次最多读取 64KB，最后一块按剩余字节数缩小，保证内存占用稳定。
    uint64_t chunkSize = std::min(CHUNK_SIZE, download->file_size - download->pos);
    uint64_t oldPos = download->pos;
    std::string chunk;
    chunk.resize(static_cast<size_t>(chunkSize));

    int fd = ::open(download->file_path.c_str(), O_RDONLY);
    if(fd < 0)
    {
        consoleLog("HttpServer_v2", "open failed for " + download->file_path);
        closeConnection(ctx);
        return;
    }

    // 使用 pread 按偏移读取，避免维护共享文件指针，也方便后续在写回调中继续推进。
    ssize_t n = ::pread(fd, &chunk[0], chunk.size(), static_cast<off_t>(download->pos));
    ::close(fd);
    if(n <= 0)
    {
        consoleLog("HttpServer_v2", "pread failed for " + download->file_path);
        closeConnection(ctx);
        return;
    }

    // 先推进偏移再写入；如果 libevent 很快触发下一次写完成，状态已经是最新的。
    download->pos += static_cast<uint64_t>(n);
    if(oldPos == 0 || download->pos >= download->file_size || (oldPos / (64ULL * 1024ULL * 1024ULL)) != (download->pos / (64ULL * 1024ULL * 1024ULL)))
    {
        consoleLog("HttpServer_v2", "large download progress: " + std::to_string(download->pos) +
            "/" + std::to_string(download->file_size));
    }
    bufferevent_write(ctx->bev, chunk.data(), static_cast<size_t>(n));
}

void HttpServer::handleRequest(const HttpRequest& req, HttpResponse* resp)
{
    try
    {
        HttpRequest mutableReq = req;
        middlewareChain_.processBefore(mutableReq);

        if(!router_.route(mutableReq, resp))
        {
            resp->setStatusLine(req.getVersion(), HttpResponse::k404NotFound, "Not Found");
            resp->setCloseConnection(true);
        }

        middlewareChain_.processAfter(*resp);
    }
    catch(const HttpResponse& response)
    {
        *resp = response;
        middlewareChain_.processAfter(*resp);
    }
    catch(const std::exception& e)
    {
        resp->setStatusLine(req.getVersion(), HttpResponse::k500InternalServerError, "Internal Server Error");
        resp->setBody(e.what());
        resp->setCloseConnection(true);
    }
}

void HttpServer::sendResponse(ConnectionContext* ctx, const HttpResponse& response)
{
    Buffer output;
    response.appendToBuffer(&output);
    consoleLog("HttpServer_v2", "write response bytes=" + std::to_string(output.readableBytes()));
    bufferevent_write(ctx->bev, output.peek(), output.readableBytes());
}

void HttpServer::closeConnection(ConnectionContext* ctx)
{
    if(!ctx) return;
    consoleLog("HttpServer_v2", "close connection");
    if(ctx->download && ctx->download->is_temp)
    {
        ::remove(ctx->download->file_path.c_str());
    }
    bufferevent* bev = ctx->bev;
    ctx->bev = nullptr;
    if(bev) bufferevent_free(bev);
    delete ctx;
}

}
