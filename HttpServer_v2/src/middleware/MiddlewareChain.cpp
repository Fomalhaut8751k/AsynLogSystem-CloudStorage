#include "../../include/middleware/MiddlewareChain.h"

namespace http_v2
{
namespace middleware
{

void MiddlewareChain::addMiddleware(std::shared_ptr<Middleware> middleware)
{
    middlewares_.push_back(std::move(middleware));
}

void MiddlewareChain::processBefore(HttpRequest& request)
{
    for(auto& middleware: middlewares_)
    {
        middleware->before(request);
    }
}

void MiddlewareChain::processAfter(HttpResponse& response)
{
    for(auto it = middlewares_.rbegin(); it != middlewares_.rend(); ++it)
    {
        (*it)->after(response);
    }
}

}
}
