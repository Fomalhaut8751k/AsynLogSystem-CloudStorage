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

// #include "../ThreadPool.hpp"
#include "../Thread.hpp"

class Client
{
private:
    struct event_base* base_;
    struct sockaddr_in server_addr_;
    struct bufferevent* bev_;

    std::string addr_;
    unsigned int port_;

    // 模拟日志信息缓冲区
    std::string log_message_;
    std::mutex Mutex_;
    std::condition_variable cv_;

    // 对应的线程池的线程号
    unsigned int threadid_;

    // 连接成功符号
    bool Connecting_;

    mylog::Thread* thread_;

public:
    // static std::atomic_int init_evthread_;

    Client(const std::string addr, unsigned int port, unsigned int threadid, mylog::Thread* thread_ = nullptr)
    {
        log_message_ = "";
        Connecting_ = false;
        threadid_ = threadid;
        port_ = port;
        addr_ = addr;

        this->thread_ = thread_;
    }

    ~Client() = default;

    // 获取线程与远程服务器的连接状态
    bool GetConnectedStatus() const { return Connecting_; }

    bool start()
    {
        base_ = event_base_new();
        if(NULL == base_)
        {
            std::cerr << "event_base_new error" << std::endl;
            exit(-1);
        }
        
        memset(&server_addr_, 0, sizeof(server_addr_));
        server_addr_.sin_family = AF_INET;
        server_addr_.sin_addr.s_addr = inet_addr(addr_.c_str());
        server_addr_.sin_port = htons(port_);


        bev_ = bufferevent_socket_new(base_, -1, BEV_OPT_CLOSE_ON_FREE);
        if (NULL == bev_) {
            std::cerr << "bufferevent_socket_new error!" << std::endl;
            event_base_free(base_);
            exit(-1);
        }
        
        bufferevent_setcb(bev_, read_callback, NULL, NULL, NULL);
        int ret = bufferevent_enable(bev_, EV_READ | EV_WRITE);
        if(ret < 0)
        {
            return false;
        }

        // 尝试连接服务器
        ret = bufferevent_socket_connect(bev_, (struct sockaddr*)&server_addr_, sizeof(server_addr_));
        if(ret < 0)  // ret = 0 不代表连接成功
        {
            return false;
        }

        std::thread loop([&]()->void{ 
            ret = event_base_dispatch(base_);
            // 如果远程服务器关机了，连接断了，事件循环就会退出
            // std::cerr << "pdcHelloWorld" << std::endl;
            thread_->Deactivate();
            Connecting_ = false;
        });

        loop.detach(); 

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        if(ret == 0)  // ret没有被更新还是0，说明事件循环正常启动
        {
            Connecting_ = true;
            return true;
        }
        
        return false;
    }

    int log_write(const std::string log_message)
    {
        int ret = bufferevent_write(bev_, log_message.c_str(), log_message.length());
        return ret;  // 数据成功放入了bev_的发送缓冲区，并非发送成功服务器接受到了
    }

    void stop()
    {
        // 退出事件循环  @return 0 if successful, or -1 if an error occurred
        if(event_base_loopexit(base_, nullptr) == -1)
        {
            std::cerr << "event loop exit failed!" << std::endl;
        }

        bufferevent_free(bev_);
        event_base_free(base_);
    }

    // 读取服务器回复的回调函数
    static void read_callback(struct bufferevent* bev, void* ctx) {
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
    };

#endif