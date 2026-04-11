#ifndef HTTPCONTEXT_H
#define HTTPCONTEXT_H

#include <iostream>
#include "mymuduo/TcpServer.h"

#include "HttpRequest.h"

/* HTTP请求报文格式如下：
    ----------------------------------------------------------------------------
    请求行 | POST /api/users HTTP/1.1
    ----------------------------------------------------------------------------
          | Host: example.com
          | User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)
          | Content-Type: application/json
    请求头 | Authorization: Bearer abc123xyz
          | Content-Length: 56
          | Accept: application/json
          | Connection: keep-alive
    ----------------------------------------------------------------------------
      空行 |
    ----------------------------------------------------------------------------
    请求体 | {"name": "John Doe", "email": "john@example.com", "age": 30}
    ----------------------------------------------------------------------------
*/

namespace http
{

class HttpContext
{
public:
    enum HttpRequestParseState
    {
        kExpectRequestLine,  // 解析请求行
        kExpectHeaders,   // 解析请求头
        kExpectBody,   // 解析请求体
        kGotAll,   // 解析完成
    };

    HttpContext();

    bool parseRequest(Buffer* buf, TimeStamp receiveTime);
    bool gotAll() const { return state_ == kGotAll; }

    void reset();

    const HttpRequest& request() const { return request_; }
    HttpRequest& request() { return request_; }

private:
    bool processRequestLine(const char* begin, const char* end);

    HttpRequestParseState state_;
    HttpRequest request_;

};

}

#endif