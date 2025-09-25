#ifndef SERVERBACKUPLOG_H
#define SERVERBACKUPLOG_H

// 创建一个服务器，在另一台“主机”登录
#include <stdio.h>
#include <event.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <event2/listener.h>
#include <iostream>
#include <fstream>



// 将接受到的数据写到生产的buffer中
void read_log_callback(struct bufferevent *, void*);

// 事件
void event_callback(struct bufferevent *, short, void *);

// 给到listener的回调函数，当有客户端连接时就会触发该回调函数
void listener_call_back(struct evconnlistener *, evutil_socket_t, struct sockaddr *, int, void*);


class Server
{
private:
    struct event_base* base_;
    struct sockaddr_in server_addr_;
    struct evconnlistener* listener_;

    std::string logfilepath_;  // 存放日志的地址
    static std::ofstream file_;        // 日志文件

public:
    Server(const std::string addr = "127.0.0.1", unsigned int port = 8000)
    {
        logfilepath_ = "./logfile.log";
        file_.open(logfilepath_, std::ios::app);
        if(!file_.is_open())
        {
            std::cerr << "open log file failed!" << std::endl;
            exit(-1);
        }

        memset(&server_addr_, 0, sizeof(server_addr_));
        server_addr_.sin_family = AF_INET;
        server_addr_.sin_addr.s_addr = inet_addr("127.0.0.1");
        server_addr_.sin_port = htons(8000);

        base_ = event_base_new();
        if(NULL == base_)
        {
            std::cerr << "event_base_new error" << std::endl;
            exit(-1);
        }

        // 创建socket, 绑定，监听，接受连接
        struct evconnlistener* listener_ = evconnlistener_new_bind(
            base_,                                      // 事件集合
            listener_call_back,                         // 当有连接被调用时的函数
            (void*)(base_),                             // 回调函数参数
            LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,  // 释放监听对象关闭socket | 端口重复使用
            10,                                         // 监听队列长度
            (struct sockaddr*)&server_addr_,             // 绑定的信息
            sizeof(server_addr_)                         // 绑定信息的长度
        );
        if(NULL == listener_)
        {
            printf("evconnlistener_new_bind error\n");
            exit(1);
        }
    }

    ~Server()
    {
        // 释放文件资源
        if(file_.is_open())
            file_.close();
        
        // 释放两个对象
        evconnlistener_free(listener_);
        event_base_free(base_);
    }
    
    // 启动事件循环
    void start()
    {
        // 监听集合中的事件
        event_base_dispatch(base_);    // 线程会阻塞在这里
    }

    static void save(char buffer[])
    {
        file_ << buffer << std::endl;
    }

};

std::ofstream Server::file_;

// 将接受到的数据写到生产的buffer中
void read_log_callback(struct bufferevent *bev, void* args)
{
    int fd = bufferevent_getfd(bev);  // 获取发送源的文件描述符
    
    char buffer[1024] = {'\0'};
    size_t ret = bufferevent_read(bev, &buffer, sizeof(buffer));
    if(-1 == ret)
    {
        printf("bufferevent_read error!\n");
        exit(1);
    }
    // ####### 处理数据 #######
    std::cerr << buffer << std::endl;
    Server::save(buffer);
    // #######################
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
     
    struct event_base* base = (struct event_base*)args;

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
        NULL                 // 回调函数的参数
    );

    // 使能事件类型
    bufferevent_enable(bev, EV_READ);  // 使能读
}

#endif