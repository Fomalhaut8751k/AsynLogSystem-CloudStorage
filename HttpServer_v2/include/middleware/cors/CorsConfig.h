#ifndef HTTPSERVER_V2_CORSCONFIG_H
#define HTTPSERVER_V2_CORSCONFIG_H

#include <string>
#include <vector>

namespace http_v2
{
namespace middleware
{

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
        config.allowedMethods = {"GET", "POST", "PUT", "DELETE", "OPTIONS"};
        config.allowedHeaders = {"Content-Type, Authorization, FileName, StorageType, FileSize"};
        return config;
    }
};

}
}

#endif
