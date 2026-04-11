#ifndef ROUTERHANDLER_H
#define ROUTERHANDLER_H

#include <string>
#include <memory>

#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"

namespace http
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