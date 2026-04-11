#include "../../include/http/HttpResponse.h"

namespace http
{

HttpResponse::HttpResponse(bool close):
    statusCode_(kUnknown),
    closeConnection_(close)
{

}


void HttpResponse::setStatusLine(const std::string& version, 
                        HttpStatusCode statusCode, 
                        const std::string& statusMessage)
{
    // setVersion(version);
    // setStatusCode(statusCode);
    // setStatusMessage(statusMessage);
    httpVersion_ = version;
    statusCode_ = statusCode;
    statusMessage_ = statusMessage;
}


void HttpResponse::setErrorHeader()
{

}

    
void HttpResponse::appendToBuffer(Buffer* outputBuf) const
{
    // HttpResponse封装的信息格式化输出
    char buf[32];
    // 状态信息有长有短，不方便定义一个固定大小的内存存储
    snprintf(buf, sizeof buf, "%s %d ", httpVersion_.c_str(), statusCode_);

    outputBuf->append(buf);
    outputBuf->append(statusMessage_);
    outputBuf->append("\r\n");

    if(closeConnection_)
    {
        outputBuf->append("Connection: close\r\n");
    }
    else
    {
        outputBuf->append("Connection: keep-Alive\r\n");
    }
    
    for(const auto& header: headers_)
    {
        outputBuf->append(header.first);
        outputBuf->append(": ");
        outputBuf->append(header.second);
        outputBuf->append("\r\n");
    }
    outputBuf->append("\r\n");  // 空行
    outputBuf->append(body_);   // 响应体
}

}