#ifndef HTTPSERVER_V2_CORSMIDDLEWARE_H
#define HTTPSERVER_V2_CORSMIDDLEWARE_H

#include "../Middleware.h"
#include "CorsConfig.h"

namespace http_v2
{
namespace middleware
{

class CorsMiddleware: public Middleware
{
public:
    explicit CorsMiddleware(const CorsConfig& config = CorsConfig::defaultConfig());

    void before(HttpRequest& request) override;
    void after(HttpResponse& response) override;

private:
    std::string join(const std::vector<std::string>& strings, const std::string& delimiter);
    bool isOriginAllowed(const std::string& origin) const;
    void handlePreflightRequest(const HttpRequest& request, HttpResponse& response);
    void addCorsHeaders(HttpResponse& response, const std::string& origin);

    CorsConfig config_;
};

}
}

#endif
