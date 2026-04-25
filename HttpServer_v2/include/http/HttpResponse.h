#ifndef HTTPSERVER_V2_HTTPRESPONSE_H
#define HTTPSERVER_V2_HTTPRESPONSE_H

#include <iostream>
#include <map>
#include <string>

#include "Buffer.h"

namespace http_v2
{

class HttpResponse
{
public:
    enum HttpStatusCode
    {
        kUnknown,
        k200Ok = 200,
        k204NoContent = 204,
        k301MovedPermanently = 301,
        k400BadRequest = 400,
        k401Unauthorized = 401,
        k403Forbidden = 403,
        k404NotFound = 404,
        k409Conflict = 409,
        k500InternalServerError = 500,
    };

    explicit HttpResponse(bool close = true);

    void setVersion(std::string version) { httpVersion_ = std::move(version); }
    void setStatusCode(HttpStatusCode code) { statusCode_ = code; }
    HttpStatusCode getStatusCode() const { return statusCode_; }

    void setStatusMessage(const std::string& message) { statusMessage_ = message; }
    void setCloseConnection(bool on) { closeConnection_ = on; }
    bool closeConnection() const { return closeConnection_; }

    void addHeader(const std::string& key, const std::string& value) { headers_[key] = value; }
    void removeHeader(const std::string& key) { headers_.erase(key); }
    std::string getHeader(const std::string& key) const;

    void setContentType(const std::string& contentType) { addHeader("Content-Type", contentType); }
    void setContentLength(uint64_t length) { addHeader("Content-Length", std::to_string(length)); }
    void setBody(const std::string& body) { body_ = body; }
    std::string getBody() const { return body_; }

    void setStatusLine(const std::string& version, HttpStatusCode statusCode, const std::string& statusMessage);
    void setErrorHeader();

    void appendToBuffer(Buffer* outputBuf) const;
    void appendToBufferWithoutBody(Buffer* outputBuf) const;
    void appendToBufferWithBody(Buffer* outputBuf) const;

private:
    void appendHeaderToBuffer(Buffer* outputBuf) const;

    std::string httpVersion_;
    HttpStatusCode statusCode_;
    std::string statusMessage_;
    bool closeConnection_;
    std::map<std::string, std::string> headers_;
    std::string body_;
};

}

#endif
