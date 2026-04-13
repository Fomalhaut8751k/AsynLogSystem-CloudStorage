#include "../../include/http/HttpContext.h"

#include <cstring>

namespace http
{

HttpContext::HttpContext():
    state_(kExpectRequestLine)
{
    maxFileSize_ = 1;
    maxFileSize_ *= (1024 * 1024);
    maxFileSize_ *= (8 * 1024);
}


// 将报文解析出来的关键信息封装到HttpRequest对象里面去
int HttpContext::parseRequest(Buffer* buf, TimeStamp receiveTime)
{
    int ok = 1;  // 解析每行请求格式是否正确
    bool hasMore = true;
    while(hasMore)
    {
        /* POST /api/users HTTP/1.1 */
        if(state_ == kExpectRequestLine)
        {
            const char* crlf = buf->findCRLF();  // 从buffer中找“\r\n”
            /*
                const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF+2)；
                return crlf == beginWrite() ? NULL : crlf;
                在[peek(), beginWrite())中找，找不到就是beginWrite()
            */
            if(crlf)
            {
                ok = processRequestLine(buf->peek(), crlf);  // 可读区域的起始位置，到crlf行结束符之前
                if(ok == 1)
                {
                    request_.setReceiveTime(receiveTime);
                    buf->retrieveUntil(crlf+2);  // 包含了crlf,表示这一段已经读取
                    state_ = kExpectHeaders;  // 检测完请求行后，接下来就是检测请求头
                }   
                else
                {
                    hasMore = false;  // 如果检测有误就退出
                }
            }
            else
            {
                hasMore = false;
            }
        }

        /* Host: example.com
           User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)
           Content-Type: application/json
           Authorization: Bearer abc123xyz
           Content-Length: 56
           Accept: application/json
           Connection: keep-alive

        */
        else if(state_ == kExpectHeaders)
        {
            // 虽然有很多对，但是外面的循环可以帮助我们一个一个处理，只要state_不变
            const char* crlf = buf->findCRLF();
            if(crlf)
            {
                const char* colon = std::find(buf->peek(), crlf, ':');
                if(colon < crlf)
                {
                    request_.addHeader(buf->peek(), colon, crlf);
                }
                else if(buf->peek() == crlf)  // 空行， 说明头结束了
                {
                    // 根据请求方法和Content-Length判断是否需要继续读取body
                    if(request_.method() == HttpRequest::kPost || 
                        request_.method() == HttpRequest::kPut)  // 只有这两种方法需要body
                    {
                        std::string contentLength = request_.getHeader("Content-Length");
                        if(!contentLength.empty())
                        {
                            request_.setContentLength(std::stoull(contentLength));
                            if(request_.contentLength() > 0)
                            {
                                state_ = kExpectBody;  // 大于0说明需要继续读取body

                                // 补充，限制上传文件的大小
                                if(request_.contentLength() >= maxFileSize_)
                                {   // 超过了512MB，就不再读取请求体中的数据
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
                            // POST/PUT 请求没有 Content-Length, 是HTTP语法错误
                            ok = 0;
                            hasMore = false;
                        }
                    }
                    else  // GET/HEAD/DELETE 等方法直接完成，不需要请求体
                    {
                        state_ = kGotAll;
                        hasMore = false;
                    }
                }
                else  // Header行格式错误
                {
                    ok = 0; 
                    hasMore = false;
                }
                buf->retrieveUntil(crlf + 2);  // 开始读指针指向下一行数据
            }
            else
            {
                hasMore = false;
            }    
        }
        else if(state_ == kExpectBody)
        {
            // 检查缓冲区中是否有足够的数据
            if(buf->readableBytes() < request_.contentLength()){
                // std::cerr << std::string(buf->peek(), buf->readableBytes()) << std::endl;
                // buf->retrieve(buf->readableBytes());
                hasMore = false;  // 数据不完整，等待更多数据
                return true;
            }
            
            // 只读取Content-Length指定的长度
            std::string body(buf->peek(), buf->peek() + request_.contentLength());
            request_.setBody(body);

            // 准备移动读指针
            buf->retrieve(request_.contentLength());

            state_ = kGotAll;
            hasMore = false;
        }
    }
    return ok;  // ok为false代表报文语法解析错误
}


void HttpContext::reset()
{
    state_ = kExpectRequestLine;
    HttpRequest dummyData;
    request_.swap(dummyData);  // swap中包含各种成员变量的交换
}


bool HttpContext::processRequestLine(const char* begin, const char* end)
{
    /* 举个请求行的例子
        不带参数： GET /api/users HTTP/1.1
        带参数： GET /api/products?page=2&limit=20&sort=name&order=asc HTTP/1.1 (分页请求)
    */
    bool succeed = false;
    const char* start = begin;
    const char* space = std::find(start, end, ' ');  // 找到第一个空格
    if(space != end && request_.setMethod(start, space))  // 左闭右开，故截取的就是POST
    {
        start = space + 1;
        space = std::find(start, end, ' '); 
        if(space != end)
        {
            const char* argumentStart = std::find(start, end, '?');  // 参数从？后面开始
            if(argumentStart != end)  // 请求中带有参数
            {
                request_.setPath(start, argumentStart);   // /api/products
                request_.setQueryParameters(argumentStart + 1, space);  // 让request_自己分割
            }
            else  // 请求中不带有参数
            {
                request_.setPath(start, space);
            }

            start = space + 1;  // 来到HTTP/1.1的‘H’处
            // 满足以下条件就说明请求行没有问题
            succeed = ((end - start == 8) && std::equal(start, end-1, "HTTP/1."));
            // std::cerr << *(end-1) << std::endl;
            if(succeed)
            {   // HTTP的两种版本，定义了客户端和服务器之间如何交换数据
                if(*(end-1) == '1')
                {
                    request_.setVersion("HTTP/1.1");
                }
                else if(*(end - 1) == '0')
                {
                    request_.setVersion("HTTP/1.0");
                }
                else
                {
                    succeed = false;
                }
                
            }
             
        }
    }
    return succeed;
}


}
