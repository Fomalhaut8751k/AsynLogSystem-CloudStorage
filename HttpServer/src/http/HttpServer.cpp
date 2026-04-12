#include "../../include/http/HttpServer.h"

#include <any>
#include <functional>
#include <memory>
#include <thread>
#include <boost/any.hpp>

namespace http
{

class MaxFileSizeException : public std::exception {
public:
    const char* what() const noexcept override {
        return "The uploaded file is too large and has been rejected";
    }
};

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
    httpCallback_(std::bind(&HttpServer::handleRequest, this, std::placeholders::_1, std::placeholders::_2))
{
    initialize();
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
    try
    {
        // 这层判断只是代表是否支持ssl
        if(useSSL_)
        {
            // logger_->INFO("onMessage useSSL_ is true");
            LOG_INFO("onMessage useSSL_ is true");
            // 1. 查找对应的SSL连接
            auto it = sslConns_.find(conn);
            if(it != sslConns_.end())
            {
                // logger_->INFO("onMessage sslConns_ is not empty");
                LOG_INFO("onMessage sslConns_ is not empty");
                // 2. SSL连接处理数据
                it->second->onRead(conn, buf, receiveTime);

                // 3. 如果SSL握手还未完成，直接返回
                if(!it->second->isHandshakeCompleted())
                {
                    // logger_->INFO("onMessage sslConns_ is not empty");
                    LOG_INFO("onMessage sslConns_ is not empty");
                    return;
                }

                // 4. 从SSL连接的解密缓冲区获取数据
                Buffer* decryptedBuf = it->second->getDecryptedBuffer();
                if(decryptedBuf->readableBytes() == 0)
                {
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
            if(useSSL_)
            {
                auto it = sslConns_.find(conn);
                if(it != sslConns_.end())
                {   
                    it->second->send(response.c_str(), response.length());
                }
                else
                {
                    conn->send(response.c_str());
                }
            }
            else
            {
                conn->send(response.c_str());
            }
            conn->shutdown();
        }
        // 如果buf缓冲区中解析出一个完成的数据包才封装响应报文
        if(context->gotAll())
        {
            // context->request().showAll();
            std::string requestBody = context->request().getBody();
            onRequest(conn, context->request());
            context->reset();
        }
    }
    catch(const std::exception& e)
    {   
        // 捕获异常，返回错误信息
        LOG_ERROR("Exception in onMessage: %s", e.what());
        conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
        if(e.what() == "The uploaded file is too large and has been rejected"){
            conn->stopRead();
        }
        conn->shutdown();
    }
    
}

void HttpServer::onRequest(const TcpConnectionPtr& conn, const HttpRequest& req)
{
    const std::string& connection = req.getHeader("Connection");
    bool close = ((connection == "close") || (req.getVersion() == "HTTP/1.0" && connection != "Keep-Alive"));
    // std::cout << req.getVersion() << " " << close << std::endl;
    HttpResponse response(close);

    // 根据请求报文信息来封装响应报文对象
    httpCallback_(req, &response);  // 执行onHttpCallback函数

    // 可以给response设置一个成员，判断是否请求的是文件，如果是文件设置为true，并且存在文件位置在这里send出去
    Buffer buf;
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

