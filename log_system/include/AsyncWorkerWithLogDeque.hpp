#ifndef ASYNC_WORKER_WITH_LOG_DEQUE_H
#define ASYNC_WORKER_WITH_LOG_DEQUE_H

#include <iostream>
#include <thread>
#include <functional>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <deque>

#include "LogSystemConfig.hpp"
#include "utils/utils2.hpp"
#include "Flush.hpp"
#include "Message.hpp"
#include "MyLog.hpp"

#define CONSOLEWARN 0

namespace mylog{
    class AsyncWorkerWithLogDeque{
    private:
        using LOG_FUNC = std::function<void(const std::string&)>;
        using LOG_FUNC_DEFAULT = std::function<void(const std::string&string)>;  // 用于在控制台输出一些紧急日志
        
        // 日志控制队列，生产者只负责入队；消费者批量 swap 后在锁外拼接。
        std::deque<std::string> logQueue_;
        std::uint64_t logSize_;
        std::mutex queueMutex_;

        // 日志输出器，提供一个默认的控制台模式日志器
        LOG_FUNC logFunc_;
        LOG_FUNC_DEFAULT logFuncDefault_;

        // 记录磁盘磁盘管理状态
        DiskSpaceChecker::DiskInfo diskInfo_;
        std::atomic_uint64_t diskInfoAvailable_;  // 记录在变量中，避免频繁使用系统调用

        // 控制日志的取出
        std::condition_variable condV_; 

        // 管理异步工作者析构工作
        bool exitLabel_;
        Semaphore exitSemaphore;

        std::atomic_int cnt;

        // 从日志队列中获取日志并输出到指定位置
        void LogQueueBackward(){
            exitSemaphore.acquire();  // 先获取唯一的资源
            std::deque<std::string> localQueue;
            std::string logBuffer;
            std::uint64_t localSize = 0;
            for(;;){
                {
                    std::unique_lock<std::mutex> lock(queueMutex_);
                    condV_.wait_for(lock, std::chrono::milliseconds(1000), [this](){
                        return exitLabel_ || !logQueue_.empty();
                    });

                    if(logQueue_.empty() && exitLabel_){
                        break;
                    }
                    if(logQueue_.empty()){
                        continue;
                    }

                    localQueue.swap(logQueue_);
                    localSize = logSize_;
                    logSize_ = 0;
                }

                logBuffer.clear();
                logBuffer.reserve(localSize);
                for(const auto& message: localQueue){
                    logBuffer.append(message);
                    logBuffer.push_back('\n');
                }
                if(!logBuffer.empty()){
                    logBuffer.pop_back();
                }
                logFunc_(logBuffer);  // 输出到指定为止当中
                localQueue.clear();
                cnt++;
            }
            std::cerr << cnt << std::endl;
            exitSemaphore.release();  // 释放唯一的资源，析构函数才能继续执行
        }

    public:
        AsyncWorkerWithLogDeque(LOG_FUNC logfunc, LOG_FUNC_DEFAULT logfuncdefault):
            logSize_(0),
            logFunc_(logfunc),
            logFuncDefault_(logfuncdefault),
            exitLabel_(false), 
            exitSemaphore(1), 
            cnt(0){
            diskInfo_ = DiskSpaceChecker::get_disk_info("/");
            diskInfoAvailable_ = diskInfo_.available_bytes;
        }

        ~AsyncWorkerWithLogDeque(){
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                exitLabel_ = true;
            }
            condV_.notify_one();
            exitSemaphore.acquire();
        }

        void start(){
            std::thread Th(&AsyncWorkerWithLogDeque::LogQueueBackward, this);
            Th.detach();
        }

        void LogQueueForward(const std::string& message){
            std::string copy(message);
            LogQueueForward(std::move(copy));
        }

        void LogQueueForward(std::string&& message){
            const std::uint64_t messageSize = message.size() + 1;
            if(diskInfoAvailable_ < messageSize + SPACE_ERROR_THRESHOLD){
#if CONSOLEWARN
                logFuncDefault_("Disk warning: The current available space is insufficient. Please free up space");
#endif
                return;
            }

            std::uint64_t allLogSize = 0;
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                if(exitLabel_){
                    return;
                }
                if(logSize_ + messageSize > LOG_BUFFER_MAX_SIZE){
#if CONSOLEWARN
                    logFuncDefault_("The buffer has been expanded to its maximum capacity, and log submission will be blocked");
#endif
                    return;
                }
                logQueue_.emplace_back(std::move(message));
                logSize_ += messageSize;
                allLogSize = logSize_;
            }

            diskInfoAvailable_ -= messageSize;
            if(allLogSize > 5 * LOG_BUFFER_MAX_SIZE / 6){
                condV_.notify_one();
            }
        }
    };   
}

#endif
