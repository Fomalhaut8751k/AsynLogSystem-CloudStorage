#include "../../include/http/HttpResponse.h"

#include <cstdio>

namespace http_v2
{

namespace
{

std::string sanitizeHeaderValue(const std::string& value)
{
    std::string sanitized = value;
    for(char& ch: sanitized)
    {
        if(ch == '\r' || ch == '\n') ch = ' ';
    }
    return sanitized;
}

}

HttpResponse::HttpResponse(bool close):
    httpVersion_("HTTP/1.1"),
    statusCode_(kUnknown),
    closeConnection_(close)
{
}

std::string HttpResponse::getHeader(const std::string& key) const
{
    auto it = headers_.find(key);
    return it == headers_.end() ? "" : it->second;
}

void HttpResponse::setStatusLine(const std::string& version, HttpStatusCode statusCode, const std::string& statusMessage)
{
    httpVersion_ = version.empty() ? "HTTP/1.1" : version;
    statusCode_ = statusCode;
    statusMessage_ = statusMessage;
}

void HttpResponse::setErrorHeader()
{
    if(statusCode_ == kUnknown)
    {
        setStatusLine("HTTP/1.1", k500InternalServerError, "Internal Server Error");
    }
}

void HttpResponse::appendHeaderToBuffer(Buffer* outputBuf) const
{
    char buf[64];
    snprintf(buf, sizeof buf, "%s %d ", httpVersion_.empty() ? "HTTP/1.1" : httpVersion_.c_str(), statusCode_);
    outputBuf->append(buf);
    outputBuf->append(statusMessage_);
    outputBuf->append("\r\n");

    outputBuf->append(closeConnection_ ? "Connection: close\r\n" : "Connection: keep-Alive\r\n");

    for(const auto& header: headers_)
    {
        outputBuf->append(header.first);
        outputBuf->append(": ");
        // outputBuf->append(header.second);
        outputBuf->append(sanitizeHeaderValue(header.second));
        outputBuf->append("\r\n");
    }
    outputBuf->append("\r\n");
}

void HttpResponse::appendToBuffer(Buffer* outputBuf) const
{
    appendHeaderToBuffer(outputBuf);
    outputBuf->append(body_);
}

void HttpResponse::appendToBufferWithoutBody(Buffer* outputBuf) const
{
    appendHeaderToBuffer(outputBuf);
}

void HttpResponse::appendToBufferWithBody(Buffer* outputBuf) const
{
    outputBuf->append(body_);
}

}
