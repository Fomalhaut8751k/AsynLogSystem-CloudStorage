#ifndef ASYNCLOGGER_H
#define ASYNCLOGGER_H

#include <stdio.h>
#include <event.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <event2/listener.h>

/*
    AsyncLogger 把组织好的日志放到（生产者的）缓存区上
    "所有的操作将在这里被调用"

    肯定有个函数，接受组织好的日志作为参数
*/
#include "AsyncWorker.hpp"
#include "Util.hpp"
#include "LogFlush.hpp"

struct ListenerArgs
{
    ListenerArgs(struct event_base* base, mylog::AsyncWorker* asyncworker):
        base_(base), asyncworker_(asyncworker){}
    
    struct event_base* base_;
    mylog::AsyncWorker* asyncworker_;
};


// 将接受到的数据写到生产的buffer中
void read_log_callback(struct bufferevent *bev, void* args)
{
    int fd = bufferevent_getfd(bev);  // 获取发送源的文件描述符

    mylog::AsyncWorker* worker = (mylog::AsyncWorker*)args;
    
    char buffer[1024] = {'\0'};
    size_t ret = bufferevent_read(bev, &buffer, sizeof(buffer));
    if(-1 == ret)
    {
        printf("bufferevent_read error!\n");
        exit(1);
    }
    else
    {   // 此时数据在buffer里，把它通过worker发送到生产者的buffer中
        unsigned int length = std::string(buffer).length();
        worker->readFromUser(buffer, length-1);  // 来自客户端的消息的末尾会有'\n'，把它舍去
    } 
}


// 事件
void event_callback(struct bufferevent *bev, short what, void *args)
{
    int fd = bufferevent_getfd(bev);
    if(what & BEV_EVENT_EOF)
    {
        printf("客户端%d下线\n", fd); 
        bufferevent_free(bev);     // 释放bufferevent对象
    }
    else
    {
        printf("未知错误\n");
    }
}


// 给到listener的回调函数，当有客户端连接时就会触发该回调函数
void listener_call_back(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *addr, int socklen, void* args)
{
    printf("接收%d的连接\n", fd);
    ListenerArgs* listener_args = (ListenerArgs *)args;

    // 解包
    struct event_base* base = listener_args->base_;
    mylog::AsyncWorker* asyncworker_ = listener_args->asyncworker_;

    // 针对已经存在的socket创建bufferevent对象
    struct bufferevent* bev = bufferevent_socket_new(
        base,                       // 事件集合（从主函数传递来的）
        fd,                         // 代表TCP连接
        BEV_OPT_CLOSE_ON_FREE       // 如果释放bufferevent对象则关闭连接   
    );
    if(NULL == bev)
    {
        printf("bufferevent_socket_new error!\n");
        exit(1);
    }

    // 给bufferevent设置回调函数
    bufferevent_setcb(
        bev,                 // bufferevent对象
        read_log_callback,   // 读事件的回调函数
        NULL,                // 写事件的回调函数（不用）
        event_callback,      // 其他事件回调函数
        (void*) asyncworker_ // 回调函数的参数
    );

    // 使能事件类型
    bufferevent_enable(bev, EV_READ);  // 使能读
}


// 抽象的产品(异步日志器)类
class AbstractAsyncLogger
{
protected:
    std::shared_ptr<mylog::AsyncWorker> worker_;
    std::shared_ptr<mylog::Logger> logger_;
    
    struct event_base* base_;
    struct evconnlistener* listener_;

    std::string log_path_;     // log文件的路径
    size_t size_;              // 文件最多存放的大小
    std::string logger_name_;  // 日志器的名称
    // mylog::Flush flush_;        // 日志输出器

public:
    virtual void setLogger() = 0;
    virtual void setAsyncWorker() = 0;
    virtual void setEvent() = 0;

    void setLoggerName(std::string logger_name){ logger_name_ = logger_name; }
    void setLogPath(std::string log_path){ log_path_ = log_path; }
    void setLogSize(size_t size){ size_ = size; }

    std::string getLoggerName() const { return logger_name_; }
    mylog::Logger* getLogger() const { return logger_.get(); }
};


// 抽象的日志器建造类
class AbstractLoggerBuilder
{
protected:
    std::shared_ptr<AbstractAsyncLogger> async_logger_;  // 抽象基类指针

public:
    virtual void BuildLoggerName(const std::string& loggername) = 0;
    // virtual void BuildLoggerFlush()
    virtual std::shared_ptr<AbstractAsyncLogger> Build() = 0;
};


namespace mylog
{
    using AbstractAsyncLoggerPtr = std::shared_ptr<AbstractAsyncLogger>;

    // 具体的产品(异步日志器)类
    class AsyncLogger: public AbstractAsyncLogger
    {
    private:

    public:
        using ptr = std::shared_ptr<AsyncLogger>;

        void setLogger()
        {
            logger_ = std::make_shared<mylog::Logger>();
        }

        void setAsyncWorker()
        {
            worker_ = std::make_shared<mylog::AsyncWorker>(logger_.get());
            worker_->start();
        }

        void setEvent()  
        {     
            // 包含了event_base_dispatch(base_); 
            base_ = event_base_new();
            if(NULL == base_)
            {
                std::printf("event_base_new error\n");
                exit(-1);
            }

            struct sockaddr_in server_addr;
            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
            server_addr.sin_port = htons(8000);

            ListenerArgs listenerargs(base_, worker_.get());

            // 创建socket, 绑定，监听，接受连接
            listener_ = evconnlistener_new_bind(
                base_,                                      // 事件集合
                listener_call_back,                         // 当有连接被调用时的函数
                (void*)(&listenerargs),                     // 回调函数参数
                LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,  // 释放监听对象关闭socket | 端口重复使用
                10,                                         // 监听队列长度
                (struct sockaddr*)&server_addr,             // 绑定的信息
                sizeof(server_addr)                         // 绑定信息的长度
            );
            if(NULL == listener_)
            {
                printf("evconnlistener_new_bind error\n");
                exit(1);
            }

            // 监听集合中的事件
            event_base_dispatch(base_);  

            // 释放两个对象
            evconnlistener_free(listener_);
            event_base_free(base_);
      
        }
    };

    // 具体的日志器建造者类
    class LoggerBuilder: public AbstractLoggerBuilder
    {
    public:
        LoggerBuilder()
        {
            async_logger_ = std::dynamic_pointer_cast<mylog::AsyncLogger>(std::make_shared<mylog::AsyncLogger>());
        }

        ~LoggerBuilder() = default;

        void BuildLoggerName(const std::string& loggername)
        {
            async_logger_->setLoggerName(loggername);
        }

        template<typename T>  // T: mylog::Flush flush
        void BuildLoggerFlush(const std::string& logpath, size_t size)
        {
            async_logger_->setLogPath(logpath);
            async_logger_->setLogSize(size);
        }

        AbstractAsyncLoggerPtr Build()
        {
            async_logger_->setLogger();
            async_logger_->setAsyncWorker();
            async_logger_->setEvent(); 

            return async_logger_;
        }
    };
}

#endif