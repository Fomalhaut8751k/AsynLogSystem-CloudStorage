#ifndef SSLCONTEXT_H
#define SSLCONTEXT_H

#include "SslConfig.h"
#include <openssl/ssl.h>
#include <memory>
#include "mymuduo/noncopyable.h"

namespace ssl
{

class SslContext: noncopyable
{
public:
    explicit SslContext(const SslConfig& config);
    ~SslContext();

    bool initialize();
    SSL_CTX* getNativeHandle() { return ctx_; }

private:
    bool loadCerticates();
    bool setupProtocol();
    void setupSessionCache();
    static void handleSslError(const char* msg);

    SSL_CTX* ctx_;  // SSL上下文
    SslConfig config_;  // SSL配置
};

}


#endif