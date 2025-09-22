#ifndef CLIENTBACKUPLOG_H
#define CLIENTBACKUPLOG_H


#include <stdio.h>
#include <event.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/thread.h>
#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>

class Client;


// 读取服务器回复的回调函数
void read_callback(struct bufferevent* bev, void* ctx);
// 事件回调函数
void event_callback(struct bufferevent* bev, short events, void* ctx);
// 启动事件循环
void event_loop_start(struct event_base* base, std::packaged_task<bool()>* returnEventLoopExit);

bool returnLabel(bool label)
{
    return label;
}

class Client
{
private:
    struct event_base* base_;
    struct sockaddr_in server_addr_;
    struct bufferevent* bev_;
    struct event* ev_;

    // 模拟日志信息缓冲区
    std::string log_message_;
    std::mutex Mutex_;
    std::condition_variable cv_;

    // 对应的线程池的线程号
    unsigned int threadid_;

    // 连接成功符号
    bool Connecting_;
    std::shared_ptr<std::packaged_task<bool(bool)>> returnConnectLabel;
    std::shared_ptr<std::packaged_task<bool()>> returnEventLoopExit;

public:
    Client(const std::string addr, unsigned int port, unsigned int threadid)
    {
        log_message_ = "";
        Connecting_ = false;
        threadid_ = threadid;

        // 为 Libevent 启用 POSIX 线程（pthreads）支持
        evthread_use_pthreads();
        base_ = event_base_new();
        if(NULL == base_)
        {
            std::cerr << "event_base_new error" << std::endl;
            exit(-1);
        }
        
        memset(&server_addr_, 0, sizeof(server_addr_));
        server_addr_.sin_family = AF_INET;
        server_addr_.sin_addr.s_addr = inet_addr(addr.c_str());
        server_addr_.sin_port = htons(port);

        bev_ = bufferevent_socket_new(base_, -1, BEV_OPT_CLOSE_ON_FREE);
        if (NULL == bev_) {
            std::cerr << "bufferevent_socket_new error!" << std::endl;
            event_base_free(base_);
            exit(-1);
        }

    }

    ~Client()
    {
        // std::cout << "the client has exited!" << std::endl;
    }

    bool start()
    {
        // 判断服务器是否连接成功
        returnConnectLabel = std::make_shared<std::packaged_task<bool(bool)>>([](bool label) -> bool { return label; });
        // 判断事件循环是否结束
        returnEventLoopExit = std::make_shared<std::packaged_task<bool()>>([]() -> bool {  return true; });
       
        bufferevent_setcb(bev_, read_callback, NULL, event_callback, (void*)returnConnectLabel.get());
        bufferevent_enable(bev_, EV_READ | EV_WRITE);

        // 运行事件循环
        std::thread t1(event_loop_start, base_, returnEventLoopExit.get());
        t1.detach();

        std::this_thread::sleep_for(std::chrono::seconds(3));

        // 尝试连接服务器
        int ret = bufferevent_socket_connect(bev_, (struct sockaddr*)&server_addr_, sizeof(server_addr_));
        if(ret < 0)
        {
            return false;
        }

        // 判断服务器是否连接成功 
        ret = returnConnectLabel->get_future().get();
        if(ret == false)
        {
            return false;
        } 
        
        Connecting_ = true;
        return true;
    }

    void log_write(const std::string log_message)
    {
        bufferevent_write(bev_, log_message.c_str(), log_message.length());
    }

    void stop()
    {
        // 退出事件循环  @return 0 if successful, or -1 if an error occurred
        if(event_base_loopexit(base_, nullptr) == -1)
        {
            std::cerr << "event loop exit failed!" << std::endl;
        }

        // 等待事件循环结束
        returnEventLoopExit->get_future().get();

        bufferevent_free(bev_);
        event_base_free(base_);
    }
};


// 读取服务器回复的回调函数
void read_callback(struct bufferevent* bev, void* ctx) {
    struct evbuffer* input_ = bufferevent_get_input(bev);
    size_t len = evbuffer_get_length(input_);
    
    if (len > 0) {
        char buffer[1024];
        evbuffer_remove(input_, buffer, len);
        buffer[len] = '\0';
        
        std::cout << "服务器回复: " << buffer;
        std::cout.flush();
    }
}

// 事件回调函数
void event_callback(struct bufferevent* bev, short events, void* ctx) 
{
    auto returnConnectLabel = static_cast<std::packaged_task<bool(bool)>*>(ctx);
    bool label = false;
    
    if (events & BEV_EVENT_CONNECTED) {
        // std::cout << "成功连接到服务器 " << std::endl;
        label = true;
    } 
    else if (events & BEV_EVENT_ERROR) {
        // std::cerr << "连接错误: " << evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()) << std::endl;
    } 
    else if (events & BEV_EVENT_EOF) {
        // std::cout << "服务器断开连接" << std::endl;
    }
    // 把连接服务器是否成功的标志返回
    (*returnConnectLabel)(label);
}

// 启动事件循环
void event_loop_start(struct event_base* base, std::packaged_task<bool()>* returnEventLoopExit)
{
    // std::cerr << "event loop start" << std::endl;
    int ret = event_base_dispatch(base);
    // std::cerr << "event loop exit" << std::endl;
    (*returnEventLoopExit)();
}

#endif