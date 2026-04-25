#include "../../../include/middleware/cors/CorsMiddleware.h"

#include <algorithm>
#include <sstream>

namespace http_v2
{
namespace middleware
{

CorsMiddleware::CorsMiddleware(const CorsConfig& config): config_(config)
{
}

void CorsMiddleware::before(HttpRequest& request)
{
    if(request.method() == HttpRequest::kOptions)
    {
        HttpResponse response;
        handlePreflightRequest(request, response);
        throw response;
    }
}

void CorsMiddleware::after(HttpResponse& response)
{
    if(config_.allowedOrigins.empty()) return;

    if(std::find(config_.allowedOrigins.begin(), config_.allowedOrigins.end(), "*") != config_.allowedOrigins.end())
    {
        addCorsHeaders(response, "*");
    }
    else
    {
        addCorsHeaders(response, config_.allowedOrigins[0]);
    }
}

std::string CorsMiddleware::join(const std::vector<std::string>& strings, const std::string& delimiter)
{
    std::ostringstream result;
    for(size_t i = 0; i < strings.size(); ++i)
    {
        if(i > 0) result << delimiter;
        result << strings[i];
    }
    return result.str();
}

bool CorsMiddleware::isOriginAllowed(const std::string& origin) const
{
    return config_.allowedOrigins.empty() ||
        std::find(config_.allowedOrigins.begin(), config_.allowedOrigins.end(), "*") != config_.allowedOrigins.end() ||
        std::find(config_.allowedOrigins.begin(), config_.allowedOrigins.end(), origin) != config_.allowedOrigins.end();
}

void CorsMiddleware::handlePreflightRequest(const HttpRequest& request, HttpResponse& response)
{
    const std::string origin = request.getHeader("Origin");
    if(!isOriginAllowed(origin))
    {
        response.setStatusLine(request.getVersion(), HttpResponse::k403Forbidden, "Forbidden");
        return;
    }

    addCorsHeaders(response, origin.empty() ? "*" : origin);
    response.setStatusLine(request.getVersion(), HttpResponse::k204NoContent, "No Content");
    response.setCloseConnection(false);
}

void CorsMiddleware::addCorsHeaders(HttpResponse& response, const std::string& origin)
{
    response.addHeader("Access-Control-Allow-Origin", origin);
    if(config_.allowCredentials)
    {
        response.addHeader("Access-Control-Allow-Credentials", "true");
    }
    if(!config_.allowedMethods.empty())
    {
        response.addHeader("Access-Control-Allow-Methods", join(config_.allowedMethods, ", "));
    }
    if(!config_.allowedHeaders.empty())
    {
        response.addHeader("Access-Control-Allow-Headers", join(config_.allowedHeaders, ", "));
    }
    response.addHeader("Access-Control-Max-Age", std::to_string(config_.maxAge));
}

}
}
