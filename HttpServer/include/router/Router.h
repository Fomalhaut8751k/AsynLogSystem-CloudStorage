#ifndef ROUTER_H
#define ROUTER_H

#include <iostream>
#include <unordered_map>
#include <string>
#include <memory>
#include <functional>
#include <regex>
#include <vector>

#include "RouterHandler.h"
#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"

namespace http
{

namespace router
{

/*
    选择注册对象式的路由处理器还是注册回调函数式的处理器取决于处理器执行的复杂程度
    如果是简单的处理可以注册回调函数，否则注册对象式路由处理器（对象中可以封装多个相关函数）
    二者注册一个即可
*/

class Router
{
public:
    using HandlerPtr = std::shared_ptr<RouterHandler>;
    using HandlerCallback = std::function<void(const HttpRequest&, HttpResponse*)>;

    Router() = default;
    ~Router() = default;

    // 路由键（请求方法 + URI） POST /api/users ?
    struct RouteKey
    {
        HttpRequest::Method method;
        std::string path;

        bool operator==(const RouteKey &other) const { return method == other.method && path == other.path; }
    };

    // 为RouteKeyHash
    struct RouteKeyHash
    {
        size_t operator()(const RouteKey& key) const;
    };

    // 注册路由处理器
    void registerHandler(HttpRequest::Method method, const std::string &path, HandlerPtr handler);

    // 注册回调函数形式的处理器
    void registerCallback(HttpRequest::Method method, const std::string &path, const HandlerCallback& callback);

    // 注册动态路由处理器
    void addRegexHandler(HttpRequest::Method method, const std::string &path, HandlerPtr handler);

    // 注册动态路由处理函数
    void addRegexCallback(HttpRequest::Method method, const std::string &path, const HandlerCallback& callback);

    // 处理请求
    bool route(const HttpRequest &req, HttpResponse* resp);

private:
    // 将路径模式切换为正则表达式模式，支持匹配任意路径参数
    std::regex convertToRegex(const std::string &pathPattern);

    // 提取路径参数
    void extractPathParameters(const std::smatch &match, HttpRequest& request);

    struct RouteCallbackObj
    {
        HttpRequest::Method method_;
        std::regex pathRegex_;
        HandlerCallback callback_;

        RouteCallbackObj(HttpRequest::Method method, std::regex pathRegex,
                        const HandlerCallback &callback);
    };

    struct RouteHandlerObj
    {
        HttpRequest::Method method_;
        std::regex pathRegex_;
        HandlerPtr handler_;

        RouteHandlerObj(HttpRequest::Method method, std::regex pathRegex,
                            HandlerPtr handler);
    };

    /* 带regex的就是动态？ */
    std::unordered_map<RouteKey, HandlerPtr, RouteKeyHash> handlers_;  // 精准匹配
    std::unordered_map<RouteKey, HandlerCallback, RouteKeyHash> callbacks_;  // 精准匹配
    std::vector<RouteHandlerObj> regexHandlers_;  // 正则匹配
    std::vector<RouteCallbackObj> regexCallbacks_;  // 正则匹配

};

}


}


#endif