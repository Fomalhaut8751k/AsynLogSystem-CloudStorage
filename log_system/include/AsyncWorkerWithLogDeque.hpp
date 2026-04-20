#ifndef ASYNC_WORKER_WITH_LOG_DEQUE_H
#define ASYNC_WORKER_WITH_LOG_DEQUE_H

#include <iostream>
#include <thread>
#include <functional>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "LogSystemConfig.hpp"
#include "utils/utils2.hpp"
#include "Flush.hpp"
#include "Message.hpp"
#include "MyLog.hpp"

#define CONSOLEWARN 0

namespace mylog{
    class AsyncWorkerWithLogDeque{
    private:
        using LOG_FUNC = std::function<void(std::string)>;
        using LOG_FUNC_DEFAULT = std::function<void(const std::string&string)>;  // 用于在控制台输出一些紧急日志
        
        // 日志控制队列，基于deque版
        std::shared_ptr<LogQueueByDeque> logQueue_;

        // 日志输出器，提供一个默认的控制台模式日志器
        LOG_FUNC logFunc_;
        LOG_FUNC_DEFAULT logFuncDefault_;

        // 记录磁盘磁盘管理状态
        DiskSpaceChecker::DiskInfo diskInfo_;
        std::atomic_uint64_t diskInfoAvailable_;  // 记录在变量中，避免频繁使用系统调用

        // 控制日志的取出
        std::mutex muteX_;
        std::condition_variable condV_; 

        // 管理异步工作者析构工作
        bool exitLabel_;
        Semaphore exitSemaphore;

        std::atomic_int cnt;

        // 从日志队列中获取日志并输出到指定位置
        void LogQueueBackward(){
            exitSemaphore.acquire();  // 先获取唯一的资源
            std::string logBuffer = "";
            for(;;){
                std::unique_lock<std::mutex> lock(muteX_);
                condV_.wait_for(lock, std::chrono::milliseconds(1000));
                if(exitLabel_) break;
                logQueue_->deleteFromTail(logBuffer);
                logBuffer.pop_back(); 
                logFunc_(logBuffer);  // 输出到指定为止当中
                logBuffer.clear();
                cnt++;
            }
            std::cerr << cnt << std::endl;
            exitSemaphore.release();  // 释放唯一的资源，析构函数才能继续执行
        }

    public:
        AsyncWorkerWithLogDeque(LOG_FUNC logfunc, LOG_FUNC_DEFAULT logfuncdefault):
            logQueue_(std::make_shared<LogQueueByDeque>()), 
            logFunc_(logfunc),
            logFuncDefault_(logfuncdefault),
            exitLabel_(false), 
            exitSemaphore(1), 
            cnt(0){
        
        }

        ~AsyncWorkerWithLogDeque(){
            exitLabel_ = true;
            condV_.notify_one();
            exitSemaphore.acquire();
        }

        void start(){
            std::thread Th(&AsyncWorkerWithLogDeque::LogQueueBackward, this);
            Th.detach();
        }

        void LogQueueForward(std::string message){
            std::uint32_t allLogSize = logQueue_->insertFromHead(message, 0);
            diskInfoAvailable_ -= (message.length() + 1);
            if(allLogSize > 5 * LOG_BUFFER_MAX_SIZE / 6){
                condV_.notify_one();
            }
        }
    };   
}

#endif
