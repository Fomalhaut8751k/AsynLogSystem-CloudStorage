#include "../../include/router/Router.h"

namespace http_v2
{
namespace router
{

size_t Router::RouteKeyHash::operator()(const RouteKey& key) const
{
    return std::hash<int>()(static_cast<int>(key.method)) ^ (std::hash<std::string>()(key.path) << 1);
}

Router::RouteCallbackObj::RouteCallbackObj(HttpRequest::Method method, std::regex pathRegex, const HandlerCallback& callback):
    method_(method), pathRegex_(std::move(pathRegex)), callback_(callback)
{
}

Router::RouteHandlerObj::RouteHandlerObj(HttpRequest::Method method, std::regex pathRegex, HandlerPtr handler):
    method_(method), pathRegex_(std::move(pathRegex)), handler_(std::move(handler))
{
}

void Router::registerHandler(HttpRequest::Method method, const std::string& path, HandlerPtr handler)
{
    handlers_[RouteKey{method, path}] = std::move(handler);
}

void Router::registerCallback(HttpRequest::Method method, const std::string& path, const HandlerCallback& callback)
{
    callbacks_[RouteKey{method, path}] = callback;
}

void Router::addRegexHandler(HttpRequest::Method method, const std::string& path, HandlerPtr handler)
{
    regexHandlers_.emplace_back(method, convertToRegex(path), std::move(handler));
}

void Router::addRegexCallback(HttpRequest::Method method, const std::string& path, const HandlerCallback& callback)
{
    regexCallbacks_.emplace_back(method, convertToRegex(path), callback);
}

bool Router::route(const HttpRequest& req, HttpResponse* resp)
{
    std::string requestPath = req.path();
    RouteKey key{req.method(), requestPath};

    auto callbackIt = callbacks_.find(key);
    if(callbackIt != callbacks_.end())
    {
        callbackIt->second(req, resp);
        return true;
    }

    auto handlerIt = handlers_.find(key);
    if(handlerIt != handlers_.end())
    {
        handlerIt->second->handle(req, resp);
        return true;
    }

    for(const auto& item: regexCallbacks_)
    {
        if(item.method_ != req.method()) continue;
        std::smatch match;
        if(std::regex_match(requestPath, match, item.pathRegex_))
        {
            HttpRequest mutableReq(req);
            extractPathParameters(match, mutableReq);
            item.callback_(mutableReq, resp);
            return true;
        }
    }

    for(const auto& item: regexHandlers_)
    {
        if(item.method_ != req.method()) continue;
        std::smatch match;
        if(std::regex_match(requestPath, match, item.pathRegex_))
        {
            HttpRequest mutableReq(req);
            extractPathParameters(match, mutableReq);
            item.handler_->handle(mutableReq, resp);
            return true;
        }
    }

    return false;
}

std::regex Router::convertToRegex(const std::string& pathPattern)
{
    return std::regex(pathPattern);
}

void Router::extractPathParameters(const std::smatch& match, HttpRequest& request)
{
    for(size_t i = 1; i < match.size(); ++i)
    {
        request.setPathParameters(std::to_string(i), match[i].str());
    }
}

}
}
