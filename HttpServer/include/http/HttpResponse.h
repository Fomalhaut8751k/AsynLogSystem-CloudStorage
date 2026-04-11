#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

#include "mymuduo/TcpServer.h"

#include <map>

namespace http
{

class HttpResponse
{
public:
    enum HttpStatusCode  // HTTP状态码
    {
        kUnknown,
        k200Ok = 200,   // 成功
        k204NoContent = 204,  // 成功但无内容
        k301MovedPermanently = 301,  // 永久重定向
        k400BadRequest = 400,   // 客户端错误
        k401Unauthorized = 401, // 未授权
        k403Forbidden = 403,   // 禁止访问
        k404NotFound = 404,    // 资源未找到
        k409Conflict = 409,     // 冲突
        k500InternalServerError = 500,   // 服务器内部错误
    };

    HttpResponse(bool close = true);

    void setVersion(std::string version) { httpVersion_ = version; }

    void setStatusCode(HttpStatusCode code) { statusCode_ = code; }
    HttpStatusCode getStatusCode() const { return statusCode_; }

    void setStatusMessage(const std::string message) { statusMessage_ = message; }

    void setCloseConnection(bool on) { closeConnection_ = on; }
    bool closeConnection() const { return closeConnection_; }

    void addHeader(const std::string& key, const std::string& value) { headers_[key] = value; }

    void setContentType(const std::string& contentType) { addHeader("Content-Type", contentType); } 
    void setContentLength(uint64_t length) { addHeader("Content-Length", std::to_string(length)); }

    void setBody(const std::string& body) { body_ = body; }

    void setStatusLine(const std::string& version, 
                        HttpStatusCode statusCode, 
                        const std::string& statusMessage);

    void setErrorHeader();

    void appendToBuffer(Buffer* outputBuf) const;

private:
    std::string httpVersion_;
    HttpStatusCode statusCode_;
    std::string statusMessage_;
    bool closeConnection_;
    std::map<std::string, std::string> headers_;
    std::string body_;
    bool isFile_;
};

}

#endif