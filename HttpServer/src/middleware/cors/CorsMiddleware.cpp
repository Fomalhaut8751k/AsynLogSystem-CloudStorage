#include "../../../include/middleware/cors/CorsMiddleware.h"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <string>
#include "mymuduo/Alogger.h"

extern ALogger* logger_;

namespace http
{

namespace middleware
{

CorsMiddleware::CorsMiddleware(const CorsConfig& config):
    // config_(std::move(config)) 无法调用移动构造函数，和下面的效果一样
    config_(config) 
{
    
}

void CorsMiddleware::before(HttpRequest& request)
{
    /*
        在请求进入业务逻辑之前进行拦截和处理
    */
    logger_->DEBUG("CorsMiddleware::before - Processing request");

    if(request.method() == HttpRequest::Method::kOptions)  // http请求方法
    {
        logger_->INFO("Processing CORS preflight request");
        HttpResponse response;
        handlePreflightRequest(request, response);  // 处理预检请求
        throw response;
    }
}

void CorsMiddleware::after(HttpResponse& response)
{
    logger_->DEBUG("CorsMiddleware::after - Processing request");

    // 直接添加CORS头，简化处理逻辑
    if(!config_.allowedOrigins.empty())
    {
        // 如果允许所有源
        if(std::find(config_.allowedOrigins.begin(), config_.allowedOrigins.end(), "*")
                        != config_.allowedOrigins.end())
        {
            addCorsHeaders(response, "*");
        }
        else
        {
            // 添加第一个允许的源
            addCorsHeaders(response, config_.allowedOrigins[0]);
        }    
    }
}

// 一个工具函数，将字符串数组连接成单个字符串
std::string CorsMiddleware::join(const std::vector<std::string>& strings, const std::string& delimiter)
{
    std::ostringstream result;
    for(size_t i = 0; i < strings.size(); ++i)
    {
        if(i > 0)
        {
            result << delimiter;
        }
        result << strings[i];
    }
    return result.str();
}

bool CorsMiddleware::isOriginAllowed(const std::string& origin) const
{
    /* config_.allowedOrigins中能找到“*”和origin 

        GET /api/data HTTP/1.1
        Host: api.example.com
        Origin: https://www.myapp.com   表示发起请求的网页地址
        User-Agent: Mozilla/5.0

        ”*” 乃CORS通配符，表示“允许所有来源”
    
        如果allowedOrigins中有“*”，表示允许所有来源，就放行
        如果allowedOrigins中有origin，也放行
        如果allowedOrigins中什么都没有，表示允许所有来源，放行
    */ 
    
    return config_.allowedOrigins.empty() || 
            std::find(config_.allowedOrigins.begin(), config_.allowedOrigins.end(), "*") != config_.allowedOrigins.end() ||
            std::find(config_.allowedOrigins.begin(), config_.allowedOrigins.end(), origin) != config_.allowedOrigins.end();
}

void CorsMiddleware::handlePreflightRequest(const HttpRequest& request, HttpResponse& response)
{
    const std::string &origin = request.getHeader("Origin");

    if(!isOriginAllowed(origin))  // 检查是否是允许的来源
    {
        logger_->WARN("Origin not allowed: " + origin);
        response.setStatusCode(HttpResponse::k403Forbidden);  // 禁止访问
        return;
    }

    addCorsHeaders(response, origin);
    response.setStatusCode(HttpResponse::k204NoContent);
    logger_->INFO("Preflight request processes successfully");
}

// 为 HTTP 响应添加 CORS（跨源资源共享）相关的响应头，允许跨域请求
void CorsMiddleware::addCorsHeaders(HttpResponse& response, const std::string& origin)
{
    try
    {   
        /*  Access-Control-Allow-Origin
            CORS 的核心响应头
            表示允许来自这个 origin 的网页访问资源
        */
        response.addHeader("Access-Control-Allow-Origin", origin); 

        // 允许携带凭证
        if(config_.allowCredentials)
        {
            response.addHeader("Access-Control-Allow-Credentials", "true");
        }

        // 允许的方法
        if(!config_.allowedMethods.empty())
        {
            response.addHeader("Access-Control-Allow-Methods", join(config_.allowedMethods, ", "));
        }

        // 允许的请求头
        if(!config_.allowedHeaders.empty())
        {
            response.addHeader("Access-Control-Allow-Headers", join(config_.allowedHeaders, ", "));
        }

        // 预检请求缓存时间
        response.addHeader("Access-Control-Max-Age", std::to_string(config_.maxAge));

        logger_->DEBUG("CORS headers added successfully");
    }
    catch(const std::exception& e)
    {
        logger_->ERROR(std::string("Error adding CORS headers: ") + e.what());
    }
    
}


}

}