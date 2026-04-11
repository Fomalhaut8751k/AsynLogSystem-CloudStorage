# SSL

`HTTP`和`HTTPS`是两种通信协议，`HTTP`以明文的方式发送内容且没有提供任何形式的数据加密功能。想比起来`HTTPS`在`HTTP`的基础上引入了`SSL`协议，通过证书验证服务器身份以确保通过过程中数据得到合理的保护和加密处理.....

## 1. SSL上下文(SSL_CTX)

SSL上下文(`STL_CTX`)是OpenSSL中用于管理SSL/TLS协议的核心结构，他负责存储SSL配置，证书，密钥等信息，并用于创建SSL连接。创建SSL上下文是实现HTTPS的关键步骤。

```cpp
SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());  // 创建
SSL_CTX_free(ctx);  // 释放

SSL_CTX_use_certificate_file(ctx_, config_.getCertificateFile().c_str(), SSL_FILETYPE_PEM)   // 加载证书
SSL_CTX_use_PrivateKey_file(ctx_, config_.getPrivateKeyFile().c_str(), SSL_FILETYPE_PEM)   // 加载私钥
SSL_CTX_use_certificate_chain_file(ctx_, config_.getCertificateChainFile().c_str())  // 加载证书链

SSL_CTX_check_private_key(ctx_)   // 验证私钥
```

由于证书 = 公钥 + 身份信息 + 签名，因此看似没有加载公钥，其实是有的。

**加密套件：**

加密套件是 SSL/TLS 连接中协商使用的"加密算法套餐"，它定义了如何实现密钥交换、身份认证、数据加密和完整性验证



### 2. SSL配置(SslConfig)

用于存储SSL配置，包括证书和密钥文件路径

```cpp
std::string certFile_;    // 证书文件
std::string keyFile_;     // 私钥文件
std::string chainFile_;   // 证书链文件
SSLVersion version_;      // 协议版本
std::string cipherList_;  // 加密套件
bool verifyClient_;       // 是否验证客户端
int verifyDepth_;         // 验证深度
int sessionTimeout_;      // 会话超时时间
long sessionCacheSize_;   // 会话缓存大小
```

### 3. SslContext

封装了`STL_CTX`类和`SslConfig`类，将配置信息加载到`SslConfig`中，在`STL_CTX`创建和设置时通`SslConfig`对外封装的接口获取相应的信息(证书文件，私钥等)

### 4. BIO缓冲区

Basic Input/Output（基本输入输出）是 OpenSSL 的抽象 I/O 层，提供了统一的接口来处理各种数据源和数据目的地。

```cpp
readBio_ = BIO_new(BIO_s_mem());  // 创建BIO
writeBio_ = BIO_new(BIO_s_mem());

SSL_set_bio(ssl_, readBio_, writeBio_);  // 设置

int BIO_pending(BIO* bio);   // 检查是否有数据可读
int BIO_wpending(BIO* bio);

int BIO_read(BIO* bio, void* buf, int len);  // 读取数据，从bio读到buf中
int BIO_write(BIO* bio, const void* buf, int len);  // 写入数据，从buf写入bio中
```

### 5. SSL连接(SSL)

`SSL `类是 `OpenSSL` 中表示单个 `SSL/TLS` 连接的核心对象，每个连接都有自己独立的 `SSL` 对象。

```cpp
ssl_ = SSL_new(ctx_);  // 创建SSL, ctx_为创建好的SSL_CTX*
SSL_free(ssl_);  // 释放SSL

SSL_write(ssl_, data, len)  // 将要发送的明文数据(起始地址为data，长度为len)加密后写到writeBIO_中
SSL_read(ssl_, decryptedData, sizeof(decryptedData));  // 将readBio_中的加密数据解密后放到decryptedData(一个长度为4096的char数组)中
```

- 补充：`BIO_read()`, `BIO_write()`, `SSL_read()`, `SSL_write()`的关系

    - 发送：

        先调用`SSL_write()`将起始地址为`data`，长度为`len`的明文数据进行**加密**，然后发送到`writeBio_`。

        然后调用`BIO_read()`将`writeBio_`的加密数据读到`buf`当中。

        最后通过`muduo::TcpConnection`的`send()`方法把`buf`上的加密数据发送出去。

    - 接收：

        接收到的数据一开始存放在`buf`中。

        通过调用`BIO_write()`将`buf`中的加密数据读到`readBio_`当中。

        再通过`SSL_read()`将加密数据进行解密，并写到起始地址为`decryptedData`,长度为`sizeof(decryptedData)`的数组当中。

    总之，发送和接收过程就是
    ```cpp
    /*
        发送：
                  SSL_write()    【bio】    BIO_read()    【buf】  conn_->send()
        明文数据 ---------------> 加密数据 ---------------> 加密数据 --------------->

        接收：       
                   SSL_read()    【bio】    BIO_write()   【buf】
        明文数据 <--------------- 加密数据 <--------------- 加密数据
    /*    
    ```


### 6. `SSLConnections`和`TcpConnection`

在`muduo`网络库的`TcpServer`中，`TcpConnection`保存在了：
```cpp
// using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
std::unordered_map<std::string, TcpConnectionPtr> connections_;   // 保存所有连接
```
这里的键(`std::string`)是连接`Connection`的名字

在本项目的`HttpServer`中，`SSlConnection`保存在了:
```cpp
std::map<TcpConnectionPtr, std::unique_ptr<ssl::SslConnection>> sslConns_;
```
可以认为就是和`muduo`网络库`TcpServer`中的表一一对应。

在`SSLConnection`中封装了这么一个成员变量。在`SSLConnection`的构造函数中，需要传入一个创建好的`TcpConnection`用于初始化，给到这个成员变量。需要用这个`conn_`进行数据发送等操作。
```cpp
TcpConnectionPtr conn_;        // TCP连接
...
conn_->send(buf);              // 使用conn_向对端发送数据
conn_->shutdown();             // 关闭TcpConnection
```
同时，`SSLConnection`会为它的`conn_`提供相应的消息回调函数，即当`TcpConnection`监听到对端有消息发来时将会触发的回调函数。
