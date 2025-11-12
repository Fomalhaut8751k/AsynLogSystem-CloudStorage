#ifndef LOGSYSTEMCONFIG_H
#define LOGSYSTEMCONFIG_H

#include "LogSystemUtils.hpp"
#include "Level.hpp"

#include <iostream>
#include <sys/stat.h>
#include <fstream>
#include <cstring>

using json = nlohmann::json;

namespace mylog
{
    class Config
    {
    private:
        Config(){};
        Config(const Config&) = delete;
        Config& operator=(const Config&) = delete;

        // 线程池参数
        unsigned int init_thread_size_;
        unsigned int thread_size_threshhold_;
        unsigned int logque_max_threshhold_;
        std::string backlog_ip_addr_; 
        unsigned int backlog_port_;

        // 异步日志系统参数
        unsigned int init_buffer_size_;

        // 异步日志工作者参数
        unsigned int effective_expansion_times_;

    public:
        static Config& GetInstance()
        {
            static Config config;
            return config;
        }

        std::pair<std::string, mylog::LogLevel> ReadConfig(const char* config_load_path = "../log_system/include/LogSystem.conf")
        {
            // fs::path current_path = fs::current_path();
            // std::cout << "当前工作目录: " << current_path << std::endl;

            // Storage.conf中的信息以json串的形式存储
            std::ifstream file_;
            struct stat st;
            // 如果配置文件不存在
            if(stat(config_load_path, &st) != 0)
            { 
                return {"the LogSystem.conf is not existed", mylog::LogLevel::ERROR};
            }
            file_.open(config_load_path, std::ios::ate);
            if(!file_.is_open()) 
            { 
                return {"the LogSystem.conf is not open", mylog::LogLevel::ERROR};
            }

            auto size = file_.tellg();        // 文件长度
            std::string content(size, '\0');  // 用于存储
            file_.seekg(0);                   // 从起点开始
            file_.read(&content[0], size);    // 把size大小的内容写入

            json config_js = json::parse(content);   // 读取到json串中

            init_thread_size_ = config_js["init_threadsize"];                      // 线程池初始线程数量
            thread_size_threshhold_ = config_js["thread_size_threshhold"];         // 线程池最大线程数量
            logque_max_threshhold_ = config_js["logque_max_threshhold"];           // 日志队列上限数量
            init_buffer_size_ = config_js["init_buffer_size"];                     // 初始缓冲区大小
            effective_expansion_times_ = config_js["effective_expansion_times"];   // 有效扩容计数
            backlog_ip_addr_ = config_js["backlog_ip_addr"];                       // 远程服务器的ip地址
            backlog_port_ = config_js["backlog_port"];                             // 远程服务器的端口号

            // 把初始化配置发送给异步日志系统
            std::string config_log = "";
            config_log += "Initialize logsystem configuration:";
            config_log += "\nTHREADPOOL:";
            config_log += ("\n  init_thread_size: " + std::to_string(init_thread_size_));
            config_log += ("\n  thread_size_threshhold: " + std::to_string(thread_size_threshhold_));
            config_log += ("\n  logque_max_threshhold: " + std::to_string(logque_max_threshhold_));
            config_log += ("\n  backlog_ip_addr: " + backlog_ip_addr_);
            config_log += ("\n  backlog_port: " + std::to_string(backlog_port_));
            config_log += "\nASYNCBUFFER: ";
            config_log += ("\n  init_buffer_size: " + std::to_string(init_buffer_size_));
            config_log += "\nASYNCWORKER: ";
            config_log += ("\n  effective_expansion_times: " + std::to_string(effective_expansion_times_));

            file_.close();

            return {config_log, mylog::LogLevel::INFO};
        }

        unsigned int GetInitThreadSize() const { return init_thread_size_; }
        unsigned int GetThreadSizeThreshhold() const { return thread_size_threshhold_; }
        unsigned int GetLogQueMaxThreshhold() const { return logque_max_threshhold_; }

        unsigned int GetInitBufferSize() const { return init_buffer_size_; }

        unsigned int GetEffectiveExpansionTimes() const { return effective_expansion_times_; }

        std::string GetBackLogIpAddr() const { return backlog_ip_addr_; }
        unsigned int GetBackLogPort() const { return backlog_port_; }

    };
}

#endif