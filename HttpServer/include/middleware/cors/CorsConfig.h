#ifndef CORSCONFIG_H
#define CORSCONFIG_H

#include <string>
#include <vector>

namespace http
{

namespace middleware
{

/*
    用于存储和传递 CORS 配置的数据结构，让 CORS 中间件的配置更加灵活和可维护
*/

struct CorsConfig
{
    std::vector<std::string> allowedOrigins;
    std::vector<std::string> allowedMethods;
    std::vector<std::string> allowedHeaders;

    bool allowCredentials = false;
    int maxAge = 3600;

    static CorsConfig defaultConfig()
    {
        CorsConfig config;
        config.allowedOrigins = {"*"};
        config.allowedMethods = {"GET", "POST", "PUT", "DELETE", "OPTION"};
        config.allowedHeaders = {"Content-Type", "Authorization"};

        return config;
    }
};

}

}

#endif