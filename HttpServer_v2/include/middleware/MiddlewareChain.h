#ifndef HTTPSERVER_V2_MIDDLEWARECHAIN_H
#define HTTPSERVER_V2_MIDDLEWARECHAIN_H

#include <memory>
#include <vector>

#include "Middleware.h"

namespace http_v2
{
namespace middleware
{

class MiddlewareChain
{
public:
    void addMiddleware(std::shared_ptr<Middleware> middleware);
    void processBefore(HttpRequest& request);
    void processAfter(HttpResponse& response);

private:
    std::vector<std::shared_ptr<Middleware>> middlewares_;
};

}
}

#endif
