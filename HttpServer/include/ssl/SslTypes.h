#ifndef SSLTYPES_H
#define SSLTYPES_H

namespace ssl
{

// SSL/TLS协议
enum class SSLVersion
{
    TLS_1_0,
    TLS_1_1,
    TLS_1_2,
    TLS_1_3
};

// SSL错误类型
enum class SSLError
{
    NONE,
    WANT_READ,
    WANT_WRITE,
    SYSCALL,
    SSL,
    UNKNOWN
};

// SSL状态
enum class SSLState
{
    HANDSHAKE,
    ESTABLISHED,
    SHUTDOWN,
    ERROR
};  


}


#endif