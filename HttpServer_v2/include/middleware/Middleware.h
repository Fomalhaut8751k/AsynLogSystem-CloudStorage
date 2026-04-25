#ifndef HTTPSERVER_V2_MIDDLEWARE_H
#define HTTPSERVER_V2_MIDDLEWARE_H

#include <memory>

#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"

namespace http_v2
{
namespace middleware
{

class Middleware
{
public:
    virtual ~Middleware() = default;
    virtual void before(HttpRequest& request) = 0;
    virtual void after(HttpResponse& response) = 0;
    void setNext(std::shared_ptr<Middleware> next) { nextMiddleware_ = std::move(next); }

private:
    std::shared_ptr<Middleware> nextMiddleware_;
};

}
}

#endif
