#ifndef HTTPSERVER_V2_SSLCONNECTION_H
#define HTTPSERVER_V2_SSLCONNECTION_H

namespace http_v2
{
namespace ssl
{

class SslConnection
{
public:
    bool isHandshakeCompleted() const { return false; }
};

}
}

#endif
