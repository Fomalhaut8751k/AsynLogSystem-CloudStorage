#include "../../include/ssl/SslConnection.h"
#include "../../include/ssl/SslTypes.h"

#include <openssl/err.h>
#include "mymuduo/Alogger.h"
#include <functional>

namespace ssl
{

static BIO_METHOD* createCustomBioMethod()
{
    BIO_METHOD* method = BIO_meth_new(BIO_TYPE_MEM, "custom");
    BIO_meth_set_write(method, SslConnection::bioWrite);
    BIO_meth_set_read(method, SslConnection::bioRead);
    BIO_meth_set_ctrl(method, SslConnection::bioCtrl);
    return method;
}

SslConnection::SslConnection(const TcpConnectionPtr& conn, SslContext* ctx):
    ssl_(nullptr),
    conn_(conn),
    ctx_(ctx),  // 创建好的ssl上下文
    state_(SSLState::HANDSHAKE),
    readBio_(nullptr),
    writeBio_(nullptr),
    messageCallback_(nullptr)
{
    // 创建 SSL 对象
    ssl_ = SSL_new(ctx_->getNativeHandle());
    /*
        这里的ctx_时SslContext类，这个类中包含了一个SSL_CTX* ctx_的成员变量
        调用ctx_->getNativeHandle()就是返回SslContext对象中SSL_CTX*类型的成员变量
    */
    if(!ssl_)
    {
        logger_->ERROR(std::string("Failed to create SSL object: ") + ERR_error_string(ERR_get_error(), nullptr));
        return;
    }
    
    // 创建BIO
    readBio_ = BIO_new(BIO_s_mem());
    writeBio_ = BIO_new(BIO_s_mem());

    if(!readBio_ || !writeBio_)
    {
        logger_->ERROR("Failed to create BIO objects");
        SSL_free(ssl_);
        ssl_ = nullptr;
        return;
    }

    SSL_set_bio(ssl_, readBio_, writeBio_);
    SSL_set_accept_state(ssl_);  // 设置为服务器模式

    // 设置SSL选项
    SSL_set_mode(ssl_, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE);

    // 设置连接回调
    conn_->setMessageCallback(std::bind(&SslConnection::onRead, this, std::placeholders::_1,
        std::placeholders::_2, std::placeholders::_3)
    );
}

SslConnection::~SslConnection()
{
    if(ssl_)
    {
        SSL_free(ssl_);  // 会同时释放 BIO
    }
}

void SslConnection::startHandshake()
{
    SSL_set_accept_state(ssl_);
    handleHandshake();
}

void SslConnection::send(const void* data, size_t len)
{
    if(state_ != SSLState::ESTABLISHED)
    {
        logger_->ERROR("Cannot send data before SSL handshake is complete");
        return;
    }

    /*
        先把明文数据data, 交给 OpenSSL 的 SSL/TLS 引擎, SSL 引擎进行加密, 
        并将加密后的数据写入内部的 BIO 缓冲区。

        从 BIO 读取加密数据，读到缓冲区buf中，再通过TcpConnection进行发送
    */

    int written = SSL_write(ssl_, data, len);
    if(written <= 0)
    {
        int err = SSL_get_error(ssl_, written);
        logger_->ERROR(std::string("SSL_write failed: ") + ERR_error_string(err, nullptr));
        return;
    }

    char buf[4096];
    int pending;
    // bio缓冲区还有数据
    while((pending = BIO_pending(writeBio_)) > 0)
    {
        // 从bio中读取数据到buf当中
        int bytes = BIO_read(writeBio_, buf, std::min(pending, static_cast<int>(sizeof(buf))));
        if(bytes > 0)
        {   // 将buf中的数据通过TcpConnection发送出去
            conn_->send(buf);
        }
    }
}

void SslConnection::onRead(const TcpConnectionPtr& conn, BufferPtr buf, TimeStamp time)
{
    /*
        将接收到的网络数据（加密的握手消息）写入 readBio_
        调用 handleHandshake() 处理 SSL/TLS 握手过程
    */
    if(state_ == SSLState::HANDSHAKE)
    {
        // 把buf中的数据写入到bio中
        BIO_write(readBio_, buf->peek(), buf->readableBytes());
        buf->retrieve(buf->readableBytes());
        handleHandshake();
        return;
        /*
            如果还在(还未)Handshake(), 那么就先把数据写到bio但是先不处理？
        */
    }
    /*
        传给TcpServer的MessageCallback的回调函数中应该
        包含将readBio_
    */
    else if(state_ == SSLState::ESTABLISHED)
    {
        // 解密数据
        char decryptedData[4096];
        int ret = SSL_read(ssl_, decryptedData, sizeof(decryptedData));
        if(ret > 0)
        {
            // 创建新的Buffer存储解密后的数据
            Buffer decryptedBuffer;
            decryptedBuffer.append(decryptedData, ret);

            // 调用上层回调处理解密后的数据
            if(messageCallback_)
            {
                messageCallback_(conn_, &decryptedBuffer, time);
            }
        }
    }
}


// SSL BIO 操作回调
int SslConnection::bioWrite(BIO* bio, const char* data, int len)
{
    SslConnection* conn = static_cast<SslConnection*>(BIO_get_data(bio));
    if(!conn)
    {
        return -1;
    }
    conn->conn_->send(data);
    return len;
}

int SslConnection::bioRead(BIO* bio, char* data, int len)
{
    SslConnection* conn = static_cast<SslConnection*>(BIO_get_data(bio));
    if(!conn)
    {
        return -1;
    }
    size_t readable = conn->readBuffer_.readableBytes();
    if(readable == 0)
    {
        return -1; // 无数据可读
    }

    size_t toRead = std::min(static_cast<size_t>(len), readable);
    memcpy(data, conn->readBuffer_.peek(), toRead);
    conn->readBuffer_.retrieve(toRead);
    return toRead;
}

long SslConnection::bioCtrl(BIO* bio, int cmd, long num, void* ptr)
{
    switch(cmd)
    {
        case BIO_CTRL_FLUSH:
            return 1;
        default:
            return 0;
    }
}


void SslConnection::handleHandshake()
{
    /*
        就直接调用内置的函数
    */
    int ret = SSL_do_handshake(ssl_); 
    /*
        这个函数就包括了：
        服务端发送自己的证书(公钥)，客户端创建会话秘钥，通过
        公钥进行加密并发送给服务端，服务端通过私钥进行解密的过程。
    */

    if(ret == 1)  // 握手成功
    {   // 从SSLState::HANDSHAKE 变成 SSLState::ESTABLISHED 
        state_ = SSLState::ESTABLISHED;
        logger_->INFO("SSL handshake completed successfully");
        logger_->INFO(std::string("Using cipher: ") + SSL_get_cipher(ssl_));
        logger_->INFO(std::string("Protocol version: ") + SSL_get_version(ssl_));

        // 握手完成后，确保设置了正确的回调
        if(!messageCallback_)
        {
            logger_->WARN("No message callback set after SSL handshake");
        }
        return;
    }

    int err = SSL_get_error(ssl_, ret);
    switch(err)
    {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            // 正常的握手过程，需要继续
            break;
        default:{
            // 获取详细的错误信息
            char errBuf[256];
            unsigned long errCode = ERR_get_error();
            ERR_error_string_n(errCode, errBuf, sizeof(errBuf));
            logger_->ERROR(std::string("SSL handshake failed: ") + errBuf);
            conn_->shutdown();  // 关闭连接
            break;
        }
    }   
}

void SslConnection::onEncrypted(const char* data, size_t len)
{
    writeBuffer_.append(data, len);
    conn_->send(&writeBuffer_);
}

void SslConnection::onDecrypted(const char* data, size_t len)
{
    decryptedBuffer_.append(data, len);
}

SSLError SslConnection::getLastError(int ret)
{
    int err = SSL_get_error(ssl_, ret);
    switch (err)
    {
    case SSL_ERROR_NONE:
        return SSLError::NONE;
    case SSL_ERROR_WANT_READ:
        return SSLError::WANT_READ;
    case SSL_ERROR_WANT_WRITE:
        return SSLError::WANT_WRITE;
    case SSL_ERROR_SYSCALL:
        return SSLError::SYSCALL;
    case SSL_ERROR_SSL:
        return SSLError::SSL;
    default:
        return SSLError::UNKNOWN;
    }
}

void SslConnection::handleError(SSLError error)
{
    switch(error)
    {
        case SSLError::WANT_READ:
        case SSLError::WANT_WRITE:
            // 需要等待更多数据或者写缓冲区可用
            break;
        case SSLError::SSL:
        case SSLError::SYSCALL:
        case SSLError::UNKNOWN:
            logger_->ERROR(std::string("SSL error occurred: ") + ERR_error_string(ERR_get_error(), nullptr));
            state_ = SSLState::ERROR;
            conn_->shutdown();
            break;
        default:
            break;
    }

}



}