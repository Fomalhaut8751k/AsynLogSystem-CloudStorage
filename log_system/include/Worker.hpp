#ifndef WORKER_H
#define WORKER_H

#include <iostream>
#include <functional>
#include <string>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "utils/utils2.hpp"
#include "Flush.hpp"
#include "Message.hpp"
#include "MyLog.hpp"
#include "LogSystemConfig.hpp"

#define CSWARN 0

namespace mylog{
    class Worker{
    private:
        using LOG_FUNC = std::function<void(std::string)>;
        using LOG_FUNC_DEFAULT = std::function<void(const std::string&string)>;  // 用于在控制台输出一些紧急日志

        // 日志输出器，提供一个默认的控制台模式日志器
        LOG_FUNC logFunc_;
        LOG_FUNC_DEFAULT logFuncDefault_;

        // 记录磁盘磁盘管理状态
        DiskSpaceChecker::DiskInfo diskInfo_;
        std::atomic_uint64_t diskInfoAvailable_;  // 记录在变量中，避免频繁使用系统调用

        // 管理异步工作者析构工作
        bool exitLabel_;
        Semaphore exitSemaphore;

        // 写入和读取的控制
        std::mutex muteX_;
        std::condition_variable condV_;

        // 缓冲区
        std::string buffer_;

        std::uint32_t bufferMaxSize_;

        std::atomic_int cnt;

        // void LogQueueBackward(){
        //     exitSemaphore.acquire();  // 先获取唯一的资源
        //     std::unique_lock<std::mutex> lock(muteX_);
        //     while(!exitLabel_){
        //         condV_.wait_for(lock, std::chrono::milliseconds(1000));
        //         if(buffer_.size() < 1) return;
        //         buffer_.pop_back();  // 去掉最后的'\n'
        //         logFunc_(buffer_);
        //         buffer_.clear();
        //     }
        //     exitSemaphore.release();  // 释放唯一的资源，析构函数才能继续执行
        // }

    public:
        Worker(LOG_FUNC logfunc, LOG_FUNC_DEFAULT logfuncdefault):
            logFunc_(logfunc),
            logFuncDefault_(logfuncdefault),
            exitLabel_(false),
            exitSemaphore(1){
            
            bufferMaxSize_ = 512 * 1024 * 1024;
            buffer_.reserve(bufferMaxSize_);
        }

        ~Worker(){
            std::cout << cnt << std::endl;
            std::cout << "~Worker()" << std::endl;
            exitLabel_ = true;
            if(buffer_.size() > 1){
                buffer_.pop_back();  // 去掉最后的'\n'
                logFunc_(buffer_);
                buffer_.clear();
            }
        }

        void start(){

        }
        
        void LogQueueForward(std::string& message){
            std::lock_guard<std::mutex> lock(muteX_);
            if(exitLabel_) return;
            if(buffer_.size() + message.length() >= bufferMaxSize_ - 1){
                return;
            }
            buffer_.append(message);
            buffer_.append("\n");
            if(buffer_.size() > 5 * bufferMaxSize_ / 6){
                cnt++;
                buffer_.pop_back();  // 去掉最后的'\n'
                logFunc_(buffer_);
                buffer_.clear();
            }
            /*
                这种设计显然不合理，因为通过在某个用户调用函数时判断是否要日志输出，
                就会导致其他的用户只是提交了日志就跑路了，而偏偏有几个冤大头提交完
                日志后要等一大堆日志都处理完后才能走。
            */
        }
    };

}

#endif