#ifndef HTTPSERVER_V2_SSLCONFIG_H
#define HTTPSERVER_V2_SSLCONFIG_H

#include <string>

namespace http_v2
{
namespace ssl
{

class SslConfig
{
public:
    std::string certFile;
    std::string keyFile;
    std::string caFile;
    bool verifyPeer{false};
};

}
}

#endif
