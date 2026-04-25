#ifndef HTTPSERVER_V2_ROUTER_H
#define HTTPSERVER_V2_ROUTER_H

#include <functional>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include "RouterHandler.h"

namespace http_v2
{
namespace router
{

class Router
{
public:
    using HandlerPtr = std::shared_ptr<RouterHandler>;
    using HandlerCallback = std::function<void(const HttpRequest&, HttpResponse*)>;

    struct RouteKey
    {
        HttpRequest::Method method;
        std::string path;
        bool operator==(const RouteKey& other) const { return method == other.method && path == other.path; }
    };

    struct RouteKeyHash
    {
        size_t operator()(const RouteKey& key) const;
    };

    void registerHandler(HttpRequest::Method method, const std::string& path, HandlerPtr handler);
    void registerCallback(HttpRequest::Method method, const std::string& path, const HandlerCallback& callback);
    void addRegexHandler(HttpRequest::Method method, const std::string& path, HandlerPtr handler);
    void addRegexCallback(HttpRequest::Method method, const std::string& path, const HandlerCallback& callback);

    bool route(const HttpRequest& req, HttpResponse* resp);

private:
    std::regex convertToRegex(const std::string& pathPattern);
    void extractPathParameters(const std::smatch& match, HttpRequest& request);

    struct RouteCallbackObj
    {
        HttpRequest::Method method_;
        std::regex pathRegex_;
        HandlerCallback callback_;
        RouteCallbackObj(HttpRequest::Method method, std::regex pathRegex, const HandlerCallback& callback);
    };

    struct RouteHandlerObj
    {
        HttpRequest::Method method_;
        std::regex pathRegex_;
        HandlerPtr handler_;
        RouteHandlerObj(HttpRequest::Method method, std::regex pathRegex, HandlerPtr handler);
    };

    std::unordered_map<RouteKey, HandlerPtr, RouteKeyHash> handlers_;
    std::unordered_map<RouteKey, HandlerCallback, RouteKeyHash> callbacks_;
    std::vector<RouteHandlerObj> regexHandlers_;
    std::vector<RouteCallbackObj> regexCallbacks_;
};

}
}

#endif
