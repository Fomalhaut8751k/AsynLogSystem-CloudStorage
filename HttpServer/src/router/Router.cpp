#include "../../include/router/Router.h"

namespace http
{

namespace router
{

size_t Router::RouteKeyHash::operator()(const RouteKey& key) const
{
    size_t methodHash = std::hash<int>{}(static_cast<int>(key.method));
    size_t pathHash = std::hash<std::string>{}(key.path);
    return methodHash * 31 + pathHash;
}

// 注册路由处理器
void Router::registerHandler(HttpRequest::Method method, const std::string &path, HandlerPtr handler)
{
    RouteKey key{method, path};  // 方法和URL
    handlers_[key] = std::move(handler);   // URL到函数的映射————路由
}

// 注册回调函数形式的处理器
void Router::registerCallback(HttpRequest::Method method, const std::string &path, const HandlerCallback& callback)
{
    RouteKey key{method, path};
    callbacks_[key] = std::move(callback);  // 同上
}

// 注册动态路由处理器
void Router::addRegexHandler(HttpRequest::Method method, const std::string &path, HandlerPtr handler)
{
    std::regex pathRegex = convertToRegex(path);
    regexHandlers_.emplace_back(method, pathRegex, handler);
}

// 注册动态路由处理函数
void Router::addRegexCallback(HttpRequest::Method method, const std::string &path, const HandlerCallback& callback)
{
    std::regex pathRegex = convertToRegex(path);
    regexCallbacks_.emplace_back(method, pathRegex, callback);
}

// 处理请求
bool Router::route(const HttpRequest &req, HttpResponse* resp)
{
    /*  GET /api/search?q=keyword&page=1 HTTP/1.1 【这是请求行】
        Method: GET
        Path: /api/search
    */
    RouteKey key{req.method(), req.path()};

    // 查找处理器
    auto handleIt = handlers_.find(key);
    if(handleIt != handlers_.end())
    {   // HandlerPtr::handle()方法
        handleIt->second->handle(req, resp);
        return true;
    }

    // 查找回调函数
    auto callbackIt = callbacks_.find(key);
    if(callbackIt != callbacks_.end())
    {
        callbackIt->second(req, resp);
        return true;
    }

    // 查找动态路由处理器
    for(const auto &[method, pathRegex, handler]: regexHandlers_)
    {
        std::smatch match;
        std::string pathStr(req.path());
        // 如果方法匹配并且动态路由匹配，则执行处理器
        if(method == req.method() && std::regex_match(pathStr, match, pathRegex))
        {
            HttpRequest newReq(req);
            extractPathParameters(match, newReq);

            handler->handle(newReq, resp);
            return true;
        }
    }

    // 查找动态路由回调函数
    for(const auto &[method, pathRegex, callback]: regexCallbacks_){
        std::smatch match;
        std::string pathStr(req.path());
        // 如果方法匹配并且动态路由匹配，则执行处理器
        if(method == req.method() && std::regex_match(pathStr, match, pathRegex))
        {
            HttpRequest newReq(req);
            extractPathParameters(match, newReq);

            callback(newReq, resp);
            return true;
        }
    }

    return false;
}


std::regex Router::convertToRegex(const std::string &pathPattern)
{   
    std::string regexPattern = "^" + std::regex_replace(pathPattern, std::regex(R"(/:([^/]+))"), R"(/([^/]+))") + "$";
    return std::regex(regexPattern);
}

void Router::extractPathParameters(const std::smatch &match, HttpRequest& request)
{
    for(size_t i = 1; i < match.size(); ++i)
    {   // 键 和 值
        request.setPathParameters("param" + std::to_string(i), match[i].str());
    }
}

Router::RouteCallbackObj::RouteCallbackObj(HttpRequest::Method method, 
                    std::regex pathRegex, const HandlerCallback &callback):
    method_(method),
    pathRegex_(pathRegex),
    callback_(callback)
{

}

Router::RouteHandlerObj::RouteHandlerObj(HttpRequest::Method method, 
                    std::regex pathRegex, HandlerPtr handler):
    method_(method), 
    pathRegex_(pathRegex),
    handler_(handler)   
{

}


}

}
