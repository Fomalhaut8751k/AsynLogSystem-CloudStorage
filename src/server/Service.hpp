#ifndef SERVICE_H
#define SERVICE_H

#include "Config.hpp"
#include "Utils.hpp"

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
#include <iostream>
#include <memory>
#include <thread>

namespace mylog
{
    class StorageServer
    {
    private:
        struct event_base* base_;
        struct bufferevent* bev_;
        struct evconnlistener* listener_;
        struct sockaddr_in server_addr_;

        std::string server_ip_addr_;
        unsigned int server_port_;

        // 下载目录前缀，深度存储目录，浅度存储目录
        std::string download_prefix_;
        std::string deep_storage_dir_;
        std::string low_storage_dir_;

        // 深度存储的文件类型
        unsigned int bundle_format_;
        // 已存储的文件信息
        std::string storage_info_;

    public:
        StorageServer()
        {

        }   

        // 启动事件循环
        static void event_loop_start(struct event_base* base)
        {
            event_base_dispatch(base);
        }

        // 读取服务器回复的回调函数
        static void read_callback(struct bufferevent* bev, void* ctx)
        {
            
        }

        // 事件回调函数
        static void event_callback(struct bufferevent* bev, short events, void* ctx) 
        {
            
        }

        // 加载配置项
        std::pair<std::string, mylog::LogLevel> InitializeConfiguration()
        {
            std::pair<std::string, mylog::LogLevel> log_message = mylog::Config::GetInstance().ReadConfig();
            // 加载配置失败
            if(log_message.second == mylog::LogLevel::ERROR)
            {
                return log_message;
            }
            
            // 加载配置成功
            server_ip_addr_ = mylog::Config::GetInstance().GetServerIp();
            server_port_ = mylog::Config::GetInstance().GetServerPort();
            
            download_prefix_ = mylog::Config::GetInstance().GetDownLoadPrefix();
            deep_storage_dir_ = mylog::Config::GetInstance().GetDeepStorageDir();
            low_storage_dir_ = mylog::Config::GetInstance().GetLowStorageDir();

            bundle_format_ = mylog::Config::GetInstance().GetBundleFormat();
            storage_info_ = mylog::Config::GetInstance().GetStorageInfo();

            return log_message;
        }
    
        // 启动服务器
        std::pair<std::string, mylog::LogLevel> InitializeService()
        {
            // 创建event_base
            evthread_use_pthreads();
            base_ = event_base_new();
            if(NULL == base_)
            {
                return {"event_base_new error", mylog::LogLevel::ERROR};   
            }

            // 设置服务器参数
            memset(&server_addr_, 0, sizeof(server_addr_));
            server_addr_.sin_family = AF_INET;
            server_addr_.sin_addr.s_addr = inet_addr(server_ip_addr_.c_str());
            server_addr_.sin_port = htons(server_port_);

            // 创建bufferevent
            bev_ = bufferevent_socket_new(base_, -1, BEV_OPT_CLOSE_ON_FREE);
            if (NULL == bev_) 
            {
                event_base_free(base_);
                return {"bufferevent_socket_new error!", mylog::LogLevel::ERROR};
            }

            // 设置回调函数
            bufferevent_setcb(bev_, StorageServer::read_callback, NULL, StorageServer::event_callback, NULL);
            if(bufferevent_enable(bev_, EV_READ | EV_WRITE) < 0)
            {
                bufferevent_free(bev_);
                event_base_free(base_);
                return {"bufferevent_enable error!", mylog::LogLevel::ERROR};
            }

            // 启动事件循环
            std::thread event_loop_thread(StorageServer::event_loop_start, base_);
            event_loop_thread.detach();

            // 尝试连接服务器
            if(bufferevent_socket_connect(bev_, (struct sockaddr*)&server_addr_, sizeof(server_addr_)) < 0)
            {
                bufferevent_free(bev_);
                event_base_free(base_);
                return {"bufferevent_socket_connect error!", mylog::LogLevel::ERROR};
            }

            return {"server initialization completed", mylog::LogLevel::INFO};
        }
    };
}




#endif