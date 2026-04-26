#ifndef ASYNCWORKERWITHLOGQUEUE_H
#define ASYNCWORKERWITHLOGQUEUE_H
/*
    AsyncWorkerWithLogQueue对象控制LogQueue来实现日志
    的异步写入和取出，使用链表来存储日志，
    既不需要两个缓冲区，也避免了扩容操作
*/

#include <iostream>
#include <thread>
#include <functional>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "utils/utils2.hpp"
#include "Flush.hpp"
#include "Message.hpp"
#include "MyLog.hpp"
#include "LogSystemConfig.hpp"

#define CSWARN 0

namespace mylog{
    class AsyncWorkerWithLogQueue{
    private:
        using LOG_FUNC = std::function<void(std::string)>;
        using LOG_FUNC_DEFAULT = std::function<void(const std::string&string)>;  // 用于在控制台输出一些紧急日志

        // 日志控制队列
        // std::shared_ptr<LogQueue> logQueue_;  
        std::shared_ptr<LogQueueWithPool> logQueue_;
        
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
        void LogQueueBackwordNoBlock(){
            exitSemaphore.acquire();  // 先获取唯一的资源
            // std::string logBuffer;
            std::stringstream ss;
            for(;;){
                if(logQueue_->empty() && exitLabel_){
                    break;
                }else if(logQueue_->empty()){
                    continue;
                }
                // logQueue_->deleteFromTail(logBuffer); 
                logQueue_->deleteFromTail(ss);  // 日志将存储在logBuffer当中
                std::string logBuffer = ss.str();
                logBuffer.pop_back();  // 忽略最后一个换行符
                logFunc_(logBuffer);  // 输出到指定为止当中
                ss.str("");   // 清空内容
                ss.clear();  // 清空缓冲区
                cnt++;
            }
            std::cerr << cnt << std::endl;
            exitSemaphore.release();  // 释放唯一的资源，析构函数才能继续执行
        }

        // 从日志队列中获取日志并输出到指定位置
        void LogQueueBackwordBlock(){
            exitSemaphore.acquire();  // 先获取唯一的资源
            std::string logBuffer;
            for(;;){
                std::unique_lock<std::mutex> lock(muteX_);
                condV_.wait_for(lock, std::chrono::milliseconds(1000));
                if(logQueue_->empty() && exitLabel_){
                    break;
                }else if(logQueue_->empty()){
                    continue;
                }
                logQueue_->deleteFromTail(logBuffer); 
                logBuffer.pop_back(); 
                logFunc_(logBuffer);  // 输出到指定为止当中
                logBuffer.clear();
                cnt++;
            }
            std::cerr << cnt << std::endl;
            exitSemaphore.release();  // 释放唯一的资源，析构函数才能继续执行
        }

        void LogQueueBackwordBlockv1(){
            exitSemaphore.acquire();  // 先获取唯一的资源
            std::stringstream ss;
            for(;;){
                std::unique_lock<std::mutex> lock(muteX_);
                condV_.wait_for(lock, std::chrono::milliseconds(1000));
                if(logQueue_->empty() && exitLabel_){
                    break;
                }else if(logQueue_->empty()){
                    continue;
                }
                logQueue_->deleteFromTail(ss);  // 日志将存储在logBuffer当中
                std::string logBuffer = ss.str();
                logBuffer.pop_back(); 
                logFunc_(logBuffer);  // 输出到指定为止当中
                ss.str("");   // 清空内容
                ss.clear();  // 清空缓冲区
                cnt++;
            }
            std::cerr << cnt << std::endl;
            exitSemaphore.release();  // 释放唯一的资源，析构函数才能继续执行
        }

        void LogQueueBackwordBlockv2(){
            exitSemaphore.acquire();  // 先获取唯一的资源
            std::string logBuffer;
            for(;;){
                std::unique_lock<std::mutex> lock(muteX_);
                condV_.wait_for(lock, std::chrono::milliseconds(1000));
                if(logQueue_->empty() && exitLabel_){
                    break;
                }else if(logQueue_->empty()){
                    continue;
                }
                logQueue_->deleteFromTail(logBuffer, 0); 
                logBuffer.pop_back(); 
                logFunc_(logBuffer);  // 输出到指定为止当中
                logBuffer.clear();
                cnt++;
            }
            std::cerr << cnt << std::endl;
            exitSemaphore.release();  // 释放唯一的资源，析构函数才能继续执行
        }

    public:
        AsyncWorkerWithLogQueue(LOG_FUNC logfunc, LOG_FUNC_DEFAULT logfuncdefault):
            // logQueue_(std::make_shared<LogQueue>()),
            logQueue_(std::make_shared<LogQueueWithPool>()),
            logFunc_(logfunc),
            logFuncDefault_(logfuncdefault),
            exitLabel_(false),
            exitSemaphore(1){
            cnt = 0;
            diskInfo_ = DiskSpaceChecker::get_disk_info("/");
            diskInfoAvailable_ = diskInfo_.available_bytes;
        }

        ~AsyncWorkerWithLogQueue(){
            exitLabel_ = true;
            condV_.notify_one();
            exitSemaphore.acquire();
        }

        void start(){
            // std::thread Th(std::bind(&AsyncWorkerWithLogQueue::LogQueueBackwordNoBlock, this));
            std::thread Th(std::bind(&AsyncWorkerWithLogQueue::LogQueueBackwordBlockv2, this));  // v2对字符串的处理进一步优化
            Th.detach();
        }

        // 将日志写入日志队列当中
        void LogQueueForward(const std::string& message){  
            if(exitLabel_){  
                // 已经禁止提交了，就直接返回
                return;
            }
            const std::uint64_t messageSize = message.length() + 1;
            if(diskInfoAvailable_ < messageSize + SPACE_ERROR_THRESHOLD){
#if CSWARN      // 磁盘空间到达预警阈值，选择直接丢弃日志
                logFuncDefault_("Disk warning: The current available space is insufficient. Please free up space");
#endif
                return;
            }
            if(0 == logQueue_->insertFromHead(message, 0, LOG_BUFFER_MAX_SIZE)){       
#if CSWARN      // 因为链表达到了存储阈值，当前日志没有被写入
                logFuncDefault_("The buffer has been expanded to its maximum capacity, and log submission will be blocked");
#endif
                return;
            }
            diskInfoAvailable_ -= messageSize;
            if(logQueue_->getLogSize() > 5 * LOG_BUFFER_MAX_SIZE / 6){
                condV_.notify_one();
            }
        }

        void LogQueueForward(std::string&& message){
            if(exitLabel_){  
                // 已经禁止提交了，就直接返回
                return;
            }
            const std::uint64_t messageSize = message.length() + 1;
            if(diskInfoAvailable_ < messageSize + SPACE_ERROR_THRESHOLD){
#if CSWARN      // 磁盘空间到达预警阈值，选择直接丢弃日志
                logFuncDefault_("Disk warning: The current available space is insufficient. Please free up space");
#endif
                return;
            }
            if(0 == logQueue_->insertFromHead(std::move(message), 0, LOG_BUFFER_MAX_SIZE)){       
#if CSWARN      // 因为链表达到了存储阈值，当前日志没有被写入
                logFuncDefault_("The buffer has been expanded to its maximum capacity, and log submission will be blocked");
#endif
                return;
            }
            diskInfoAvailable_ -= messageSize;
            if(logQueue_->getLogSize() > 5 * LOG_BUFFER_MAX_SIZE / 6){
                condV_.notify_one();
            }
        }
    };
}
 
#endif
