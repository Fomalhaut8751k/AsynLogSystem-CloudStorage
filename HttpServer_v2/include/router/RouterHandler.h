#ifndef HTTPSERVER_V2_ROUTERHANDLER_H
#define HTTPSERVER_V2_ROUTERHANDLER_H

#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"

namespace http_v2
{
namespace router
{

class RouterHandler
{
public:
    virtual ~RouterHandler() = default;
    virtual void handle(const HttpRequest& req, HttpResponse* resp) = 0;
};

}
}

#endif
