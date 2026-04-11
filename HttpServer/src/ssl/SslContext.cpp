#include "../../include/ssl/SslContext.h"
#include "mymuduo/Alogger.h"
#include <openssl/err.h>

namespace ssl
{

SslContext::SslContext(const SslConfig& config):
    ctx_(nullptr),
    config_(config)
{

}

SslContext::~SslContext()
{
    if(ctx_)
    {
        SSL_CTX_free(ctx_);
    }
}

bool SslContext::initialize()
{
    // 初始化 openssl    加载SSL错误字符串表                     加载加密算法错误字符串
    OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);

    // 创建上下文
    const SSL_METHOD* method = TLS_server_method();  // 创建一个服务器端的 SSL/TLS 方法对象，用于配置 SSL_CTX
    ctx_ = SSL_CTX_new(method);  // 根据指定的 SSL_METHOD 创建 SSL 上下文（SSL_CTX）
    if(!ctx_)
    {
        handleSslError("Failed to create SSL context");
        return false;
    }

    // 设置SSL选项
    long options = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 
                            | SSL_OP_NO_COMPRESSION | SSL_OP_CIPHER_SERVER_PREFERENCE;
    SSL_CTX_set_options(ctx_, options);

    // 加载证书，加载和验证私钥，加载证书链
    if(!loadCerticates())
    {
        return false;
    }

    // 设置协议版本
    if(!setupProtocol())
    {
        return false;
    }

    // 设置会话缓存
    setupSessionCache();

    logger_->INFO("SSL context initialized successfully");
    return true;
}

bool SslContext::loadCerticates()
{   
    /*
        证书：只包含网站自身的身份信息
        证书链：包含从网站证书到根证书的完整信任路径
    */

    // 加载证书
    if(SSL_CTX_use_certificate_file(ctx_, config_.getCertificateFile().c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        handleSslError("Failed to load server certicate");
        return false;
    }

    // 加载私钥
    if(SSL_CTX_use_PrivateKey_file(ctx_, config_.getPrivateKeyFile().c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        handleSslError("Failed to load private key");
        return false;
    }

    // 验证私钥
    if(!SSL_CTX_check_private_key(ctx_))
    {
        handleSslError("Private key dose not match the certificate");
        return false;
    }

    // 加载证书链
    if(!config_.getCertificateChainFile().empty())
    {
        if(SSL_CTX_use_certificate_chain_file(ctx_, config_.getCertificateChainFile().c_str()) <= 0)
        {
            handleSslError("Failed to load certificate chain");
            return false;
        }
    }

    return true;
}

bool SslContext::setupProtocol()
{
    // 设置 SSL/TLS 协议版本
    long options = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
    switch(config_.getProtocolVersion())
    {   
        case SSLVersion::TLS_1_0:
            options |= SSL_OP_NO_TLSv1;
            break;
        case SSLVersion::TLS_1_1:
            options |= SSL_OP_NO_TLSv1_1;
            break;
        case SSLVersion::TLS_1_2:
            options |= SSL_OP_NO_TLSv1_2;
            break;
        case SSLVersion::TLS_1_3:
            options |= SSL_OP_NO_TLSv1_3;
            break;
    }
    SSL_CTX_set_options(ctx_, options);

    // 设置加密套件
    if(!config_.getCipherList().empty())
    {
        if(SSL_CTX_set_cipher_list(ctx_, config_.getCipherList().c_str()) <= 0)
        {
            handleSslError("Failed to set cipher list");
            return false;
        }
    }
    return true;
}

void SslContext::setupSessionCache()
{
    SSL_CTX_set_session_cache_mode(ctx_, SSL_SESS_CACHE_SERVER);
    SSL_CTX_sess_set_cache_size(ctx_, config_.getSessionCacheSize());
    SSL_CTX_set_timeout(ctx_, config_.getSessionTimeout());
}

void SslContext::handleSslError(const char* msg)
{
    char buf[256];
    ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
    logger_->ERROR(std::string(msg) + buf);
}

}