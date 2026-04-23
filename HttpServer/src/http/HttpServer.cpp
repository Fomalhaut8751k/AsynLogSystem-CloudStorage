#include "../../include/http/HttpServer.h"

#include <any>
#include <functional>
#include <memory>
#include <thread>
#include <fcntl.h>
#include <boost/any.hpp>

namespace http
{

class MaxFileSizeException : public std::exception {
public:
    const char* what() const noexcept override {
        return "The uploaded file is too large and has been rejected";
    }
};

size_t findLastCompleteCharBoundary(const unsigned char* data, size_t len) {
    if (len == 0) return 0;
    
    // 从末尾向前查找，最多检查 4 个字节
    for (int i = 0; i < 4 && i < (int)len; i++) {
        unsigned char c = data[len - 1 - i];
        
        // 找到 ASCII 字符（最高位为0）
        if ((c & 0x80) == 0) {
            // ASCII 是完整字符，返回这个字符的末尾位置
            return len - i;
        }
        
        // 找到 UTF-8 起始字节
        if ((c & 0xC0) == 0xC0) {
            // 计算这个字符应该有多少字节
            int charLen = 1;
            if ((c & 0xE0) == 0xC0) charLen = 2;
            else if ((c & 0xF0) == 0xE0) charLen = 3;
            else if ((c & 0xF8) == 0xF0) charLen = 4;
            
            // 检查这个字符是否完整
            if (i + 1 >= charLen) {
                // 字符完整，返回这个字符的末尾位置
                return len - i;
            } else {
                // 字符不完整，返回这个字符的开始位置（即回退到上一个字符）
                return len - i - 1;
            }
        }
        
        // 如果是续字节（10xxxxxx），继续往前找
    }
    
    // 没找到有效边界，返回 0
    return 0;
}

// 默认的http回应函数
void defaultHttpCallback(const HttpRequest&, HttpResponse* resp)
{
    resp->setStatusCode(HttpResponse::k404NotFound);
    resp->setStatusMessage("Not Found");
    resp->setCloseConnection(true);
}

// 构造函数
HttpServer::HttpServer(int port, const std::string& name, bool useSSL, TcpServer::Option option):
    listenAddr_(port, "0.0.0.0"),
    server_(&mainLoop_, listenAddr_, name, option),
    useSSL_(useSSL),
    chunkDownloadSizeThreshold(1024 * 1024),
    httpCallback_(std::bind(&HttpServer::handleRequest, this, std::placeholders::_1, std::placeholders::_2))
{
    initialize();
    chunkDownloadSizeThreshold *= 1024;  // 一个G就需要分块传输
}

// 服务器运行函数
void HttpServer::start()
{   
    // LOG_INFO("HttpServer[" + server_.name() + "] starts listening on" + server_.ipPort());
    // logger_->WARN("HttpServer[" + server_.name() + "] starts listening on" + server_.ipPort());
    LOG_INFO("HttpServer[%s] starts listening on %s", server_.name().c_str(), server_.ipPort().c_str());
    server_.start();
    mainLoop_.loop();
}

void HttpServer::initialize()
{
    /*
        设置回调函数，用户自己定义的回调函数
        onConnection: 当有用户连接或者断开的时候调用的函数
        onMessage： 当TcpConnection接收到客户端的消息时调用的函数
    */
    server_.setConnectionCallback(std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    server_.setWriteCompleteCallback(std::bind(&HttpServer::writeCompleteCallback, this, std::placeholders::_1));
}

void HttpServer::setSslConfig(const ssl::SslConfig& config)
{
    if(useSSL_)
    {
        sslCtx_ = std::make_unique<ssl::SslContext>(config);
        if(!sslCtx_->initialize())
        {
            // logger_->ERROR("Failed to initialize SSL context");
            LOG_ERROR("Failed to initialize SSL context");
            useSSL_ = false;
            // abort();
        }
    }
}

void HttpServer::onConnection(const TcpConnectionPtr& conn)
{
    if(conn->connected())  // 新用户连接
    {
        if(useSSL_)
        {
            auto sslConn = std::make_unique<ssl::SslConnection>(conn, sslCtx_.get());
            sslConn->setMessageCallback(std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            sslConns_[conn] = std::move(sslConn);
            sslConns_[conn]->startHandshake();
        }
        conn->setContext(HttpContext());
    }
    else  // 老用户断开连接
    {
        if(useSSL_)
        {
            sslConns_.erase(conn);
        }
    }
}

void HttpServer::onMessage(const TcpConnectionPtr& conn, Buffer* buf, TimeStamp receiveTime)
{
    try{
        // 这层判断只是代表是否支持ssl
        if(useSSL_){
            // logger_->INFO("onMessage useSSL_ is true");
            LOG_INFO("onMessage useSSL_ is true");
            // 1. 查找对应的SSL连接
            auto it = sslConns_.find(conn);
            if(it != sslConns_.end()){
                // logger_->INFO("onMessage sslConns_ is not empty");
                LOG_INFO("onMessage sslConns_ is not empty");
                // 2. SSL连接处理数据
                it->second->onRead(conn, buf, receiveTime);

                // 3. 如果SSL握手还未完成，直接返回
                if(!it->second->isHandshakeCompleted()){
                    // logger_->INFO("onMessage sslConns_ is not empty");
                    LOG_INFO("onMessage sslConns_ is not empty");
                    return;
                }

                // 4. 从SSL连接的解密缓冲区获取数据
                Buffer* decryptedBuf = it->second->getDecryptedBuffer();
                if(decryptedBuf->readableBytes() == 0){
                    return;  // 没有解密后的数据
                }

                // 5. 使用解密后的数据进行HTTP处理
                buf = decryptedBuf;  // 将buf指向解密后的数据
                // logger_->INFO("onMessage decryptedBuf is not empty");
                LOG_INFO("onMessage decryptedBuf is not empty");
            }
        }
        // HttpContext对象用于解析处buf中的请求报文，并把报文的关键信息封装到HttpRequest对象中
        HttpContext* context = boost::any_cast<HttpContext>(conn->getMutableContext());
        int ret = context->parseRequest(buf, receiveTime);
        if(ret <= 0)  // 解析一个http请求, 1是成功, 0是失败, -1是文件太大不给传
        {
            // 如果解析HTTP报文中出错
            std::string response = "";
            if(ret == -1){  // 文件太大，不给传，直接关闭连接
                throw MaxFileSizeException();
            }
            else{
                response = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
            }
            if(useSSL_){
                auto it = sslConns_.find(conn);
                if(it != sslConns_.end()){   
                    it->second->send(response.c_str(), response.length());
                }
                else{
                    conn->send(response.c_str());
                }
            }
            else{
                conn->send(response.c_str());
            }
            conn->shutdown();
        }
        // 如果解析完请求头，发现文件大小大于256MB(实际可以设置比内存还大的值)
        if(context->gotHeader() && context->request().contentLength() > 256 * 1024 * 1024){
            LOG_INFO("The file is too large; initiating segmented upload");
            // 每次都直接读取buffer中的一点数据，而不是等待完整的请求体后再解析
            size_t len = buf->readableBytes();
            if(len == 0) return;     
            std::string part = std::string(buf->peek(), buf->readableBytes());
            buf->retrieve(buf->readableBytes());
            context->request().setBody(part);
            onRequest(conn, context->request());
        }
        // 如果buf缓冲区中解析出一个完成的数据包才封装响应报文
        if(context->gotAll()){
            onRequest(conn, context->request());
            // 检查 conn->getContext() 是否被 DownloadContext 占用
            // 如果是大文件下载，context_ 已经被替换，不应该 reset HttpContext
            auto* downloadCtx = boost::any_cast<std::shared_ptr<DownloadContext>>(conn->getMutableContext());
            if(!downloadCtx || !*downloadCtx){
                // 不是大文件下载场景，正常 reset
                context->reset();
            }
            // 如果是大文件下载，不 reset，让 HttpContext 保持原样
        }
    }
    catch(const std::exception& e){   
        // 捕获异常，返回错误信息
        LOG_ERROR("Exception in onMessage: %s", e.what());
        conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
        if(strcmp(e.what(), "The uploaded file is too large and has been rejected") == 0){
            conn->stopRead();
        }
        conn->shutdown();
    }
    
}
// outputBuffer_ 完全排空后触发，驱动大文件分块发送
void HttpServer::writeCompleteCallback(const TcpConnectionPtr& conn){
    // 取出 shared_ptr，立即拷贝到局部变量，之后 boost::any 怎么变都不影响 ctx 的生命周期
    auto* sptrPtr = boost::any_cast<std::shared_ptr<DownloadContext>>(conn->getMutableContext());
    if(!sptrPtr || !*sptrPtr) return;
    std::shared_ptr<DownloadContext> ctx = *sptrPtr;
    if(ctx->file_size == 0) return;

    // 所有分块已发完
    if(ctx->pos >= ctx->file_size){
        LOG_INFO("writeCompleteCallback: download complete, file: %s", ctx->file_path.c_str());
        // [分块下载] 如果是深度存储解压出的临时文件，发完后删除
        if(ctx->is_temp){
            ::remove(ctx->file_path.c_str());
            LOG_INFO("writeCompleteCallback: temp file removed: %s", ctx->file_path.c_str());
        }
        // 清空下载上下文，避免下一次请求误触发
        // ctx 仍持有 shared_ptr，close_conn 访问安全
        conn->setContext(boost::any{});
        if(ctx->close_conn){
            conn->shutdown();
        }
        return;
    }

    // 计算本次分块大小，最后一块可能不足 CHUNK_SIZE
    uint64_t chunk_size = std::min(CHUNK_SIZE, ctx->file_size - ctx->pos);

    // 从文件中读取本块数据
    std::string chunk;
    // [分块下载] 按偏移读取，避免一次性把整个大文件载入内存
    int fd = ::open(ctx->file_path.c_str(), O_RDONLY);
    if(fd < 0){
        LOG_ERROR("writeCompleteCallback: open file failed: %s", ctx->file_path.c_str());
        conn->shutdown();
        return;
    }
    chunk.resize(chunk_size);
    ssize_t n = ::pread(fd, &chunk[0], chunk_size, ctx->pos);
    ::close(fd);
    if(n <= 0){
        LOG_ERROR("writeCompleteCallback: pread failed, pos=%lu", ctx->pos);
        conn->shutdown();
        return;
    }
    chunk.resize(n);

    // 更新偏移后再 send，send 可能同步触发下一次 writeCompleteCallback
    ctx->pos += n;
    LOG_INFO("writeCompleteCallback: sending chunk, pos=%lu / %lu", ctx->pos, ctx->file_size);
    conn->send(chunk);
}


void HttpServer::onRequest(const TcpConnectionPtr& conn, const HttpRequest& req)
{
    const std::string& connection = req.getHeader("Connection");
    bool close = ((connection == "close") || (req.getVersion() == "HTTP/1.0" && connection != "Keep-Alive"));
    // std::cout << req.getVersion() << " " << close << std::endl;
    HttpResponse response(close);

    // 对于下载的特殊处理
    if(req.method() == HttpRequest::kGet && req.path() == "/download"){
        std::uint64_t file_size = std::stoull(req.getHeader("FileSize"));
        // [分块下载] 文件超过 1MB 时走分块路径，避免一次性把大文件载入内存再 send
        if(file_size >= 1024 * 1024){
            LOG_INFO("Large file download, size=%lu, will use chunked sending", file_size);

            // 1. 先通过 httpCallback_ 让 Download() 完成解压、查元数据等准备工作，
            //    并把 DownloadContext 写入 conn->downloadContext_
            //    Download() 里检测到 resp->getBody() == "chunk_prepare" 时只做准备不发数据
            response.setBody("chunk_prepare");
            httpCallback_(req, &response);

            // 2. 从 response header 里取出 Download() 写好的文件路径和 is_temp 标记
            std::string file_path = response.getHeader("X-File-Path");
            bool is_temp = (response.getHeader("X-Is-Temp") == "1");

            if(file_path.empty()){
                // Download() 准备失败（文件不存在等），直接返回错误响应
                Buffer buf;
                response.appendToBuffer(&buf);
                conn->send(&buf);
                return;
            }

            // 3. 初始化 DownloadContext 并存入连接（用 shared_ptr 管理生命周期）
            auto ctxPtr = std::make_shared<DownloadContext>();
            ctxPtr->file_path  = file_path;
            ctxPtr->pos        = 0;
            ctxPtr->file_size  = file_size;
            ctxPtr->close_conn = close;
            ctxPtr->is_temp    = is_temp;
            conn->setContext(ctxPtr);

            // 4. 发送响应头（不含 body）
            response.setStatusLine(req.getVersion(), HttpResponse::k200Ok, "OK");
            response.addHeader("Content-Length", std::to_string(file_size));
            response.addHeader("Content-Type", "application/octet-stream");
            Buffer buf;
            response.appendToBufferWithoutBody(&buf);
            conn->send(&buf);

            // 5. 发送第一个 chunk，后续由 writeCompleteCallback 驱动
            uint64_t first_chunk = std::min(CHUNK_SIZE, file_size);
            std::string chunk;
            chunk.resize(first_chunk);
            int fd = ::open(file_path.c_str(), O_RDONLY);
            if(fd >= 0){
                ssize_t n = ::pread(fd, &chunk[0], first_chunk, 0);
                ::close(fd);
                if(n > 0){
                    chunk.resize(n);
                    // 更新 pos 后再 send
                    auto* sptrPtr = boost::any_cast<std::shared_ptr<DownloadContext>>(conn->getMutableContext());
                    if(sptrPtr && *sptrPtr){
                        (*sptrPtr)->pos = n;
                    }
                    conn->send(chunk);
                }
            }
            return;
        }
    }
    
    Buffer buf;
    // 根据请求报文信息来封装响应报文对象
    httpCallback_(req, &response);  // 执行onHttpCallback函数

    // 可以给response设置一个成员，判断是否请求的是文件，如果是文件设置为true，并且存在文件位置在这里send出去
    
    response.appendToBuffer(&buf);
    // 打印完整的响应内容用于测试
    // logger_->INFO("Sending response:\n" + std::string(buf.peek(), static_cast<int>(buf.readableBytes())));
    // LOG_INFO("Sending response:\n%s", std::string(buf.peek(), static_cast<int>(buf.readableBytes())).c_str());
    
    conn->send(&buf);
    // 如果是短连接的话，返回响应报文后就断开连接
    response.setCloseConnection(close);
    if(response.closeConnection())
    {
        conn->shutdown();
    }
}


// 执行请求对应的路由处理函数
void HttpServer::handleRequest(const HttpRequest& req, HttpResponse* resp)
{
    try{
        // 处理请求的中间件
        HttpRequest mutableReq = req;
        middlewareChain_.processBefore(mutableReq);

        // 路由处理
        if(!router_.route(mutableReq, resp))
        {
            // logger_->INFO("请求的啥，url: " + req.method() + std::string(" ") + req.path()); 
            LOG_INFO("请求的啥，url: %d %s", req.method(), req.path().c_str());
            // logger_->INFO("未找到路径，返回404");
            LOG_INFO("未找到路径，返回404");
            resp->setStatusCode(HttpResponse::k404NotFound);
            resp->setStatusMessage("Not Found");
            resp->setCloseConnection(true);
        }
        // 处理响应后的中间件
        middlewareChain_.processAfter(*resp);
    }catch(const HttpResponse& res){
        // 处理中间件抛出的响应(如CORS预检请求)
        *resp = res;
    }
    catch(const std::exception& e)
    {
        // 错误处理
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setBody(e.what());
    }
}

}

