#ifndef HTTPSERVER_V2_HTTPCONTEXT_H
#define HTTPSERVER_V2_HTTPCONTEXT_H

#include "Buffer.h"
#include "HttpRequest.h"

namespace http_v2
{

class HttpContext
{
public:
    enum HttpRequestParseState
    {
        kExpectRequestLine,
        kExpectHeaders,
        kExpectBody,
        kGotAll,
    };

    HttpContext();

    int parseRequest(Buffer* buf, TimeStamp receiveTime);
    bool gotHeader() const { return state_ == kExpectBody; }
    bool gotAll() const { return state_ == kGotAll; }
    void reset();

    const HttpRequest& request() const { return request_; }
    HttpRequest& request() { return request_; }

private:
    bool processRequestLine(const char* begin, const char* end);

    HttpRequestParseState state_;
    HttpRequest request_;
    std::uint64_t maxFileSize_;
};

}

#endif
