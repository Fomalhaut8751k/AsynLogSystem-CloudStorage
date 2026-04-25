#ifndef HTTPSERVER_V2_SSLCONTEXT_H
#define HTTPSERVER_V2_SSLCONTEXT_H

#include "SslConfig.h"

namespace http_v2
{
namespace ssl
{

class SslContext
{
public:
    explicit SslContext(const SslConfig& config): config_(config) {}
    bool initialize() { return false; }

private:
    SslConfig config_;
};

}
}

#endif
