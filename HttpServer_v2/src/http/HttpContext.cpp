#include "../../include/http/HttpContext.h"

#include <algorithm>

namespace http_v2
{

HttpContext::HttpContext(): state_(kExpectRequestLine)
{
    maxFileSize_ = 1;
    maxFileSize_ *= 1024 * 1024;
    maxFileSize_ *= 8 * 1024;
}

int HttpContext::parseRequest(Buffer* buf, TimeStamp receiveTime)
{
    int ok = 1;
    bool hasMore = true;
    while(hasMore)
    {
        if(state_ == kExpectRequestLine)
        {
            const char* crlf = buf->findCRLF();
            if(crlf)
            {
                ok = processRequestLine(buf->peek(), crlf);
                if(ok == 1)
                {
                    request_.setReceiveTime(receiveTime);
                    buf->retrieveUntil(crlf + 2);
                    state_ = kExpectHeaders;
                }
                else
                {
                    hasMore = false;
                }
            }
            else
            {
                hasMore = false;
            }
        }
        else if(state_ == kExpectHeaders)
        {
            const char* crlf = buf->findCRLF();
            if(crlf)
            {
                const char* colon = std::find(buf->peek(), crlf, ':');
                if(colon < crlf)
                {
                    request_.addHeader(buf->peek(), colon, crlf);
                }
                else if(buf->peek() == crlf)
                {
                    if(request_.method() == HttpRequest::kPost || request_.method() == HttpRequest::kPut)
                    {
                        std::string contentLength = request_.getHeader("Content-Length");
                        if(!contentLength.empty())
                        {
                            request_.setContentLength(std::stoull(contentLength));
                            if(request_.contentLength() > 0)
                            {
                                state_ = kExpectBody;
                                if(request_.contentLength() >= maxFileSize_)
                                {
                                    ok = -1;
                                    hasMore = false;
                                }
                            }
                            else
                            {
                                state_ = kGotAll;
                                hasMore = false;
                            }
                        }
                        else
                        {
                            ok = 0;
                            hasMore = false;
                        }
                    }
                    else
                    {
                        state_ = kGotAll;
                        hasMore = false;
                    }
                }
                else
                {
                    ok = 0;
                    hasMore = false;
                }
                buf->retrieveUntil(crlf + 2);
            }
            else
            {
                hasMore = false;
            }
        }
        else if(state_ == kExpectBody)
        {
            if(buf->readableBytes() < request_.contentLength())
            {
                hasMore = false;
                return true;
            }
            request_.setBody(buf->peek(), buf->peek() + request_.contentLength());
            buf->retrieve(request_.contentLength());
            state_ = kGotAll;
            hasMore = false;
        }
    }
    return ok;
}

void HttpContext::reset()
{
    state_ = kExpectRequestLine;
    HttpRequest dummy;
    request_.swap(dummy);
}

bool HttpContext::processRequestLine(const char* begin, const char* end)
{
    bool succeed = false;
    const char* start = begin;
    const char* space = std::find(start, end, ' ');
    if(space != end && request_.setMethod(start, space))
    {
        start = space + 1;
        space = std::find(start, end, ' ');
        if(space != end)
        {
            const char* argumentStart = std::find(start, space, '?');
            if(argumentStart != space)
            {
                request_.setPath(start, argumentStart);
                request_.setQueryParameters(argumentStart + 1, space);
            }
            else
            {
                request_.setPath(start, space);
            }

            start = space + 1;
            succeed = ((end - start == 8) && std::equal(start, end - 1, "HTTP/1."));
            if(succeed)
            {
                if(*(end - 1) == '1') request_.setVersion("HTTP/1.1");
                else if(*(end - 1) == '0') request_.setVersion("HTTP/1.0");
                else succeed = false;
            }
        }
    }
    return succeed;
}

}
