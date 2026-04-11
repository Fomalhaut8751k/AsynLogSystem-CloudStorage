#ifndef MIDDLEWARE_H
#define MIDDLEWARE_H

#include "../../include/http/HttpRequest.h"
#include "../../include/http/HttpResponse.h"

namespace http
{

namespace middleware
{
// 像一个链表，好像没有值域，只有指针域(nextMiddleware_)
class Middleware
{
public:
    virtual ~Middleware() = default;

    // 请求前处理
    virtual void before(HttpRequest& request) = 0;

    // 响应后处理
    virtual void after(HttpResponse& response) = 0;

    // 设置下一个中间件
    void setNext(std::shared_ptr<Middleware> next) { nextMiddleware_ = next; }
private:
    std::shared_ptr<Middleware> nextMiddleware_;
};

}

}

#endif