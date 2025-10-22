#ifndef ASYNCWORKER_H
#define ASYNCWORKER_H
/*
    AsyncWorker对象控制的双缓冲区-消费者缓冲区和生产者缓冲区
    当消费者缓冲区有数据上——阻塞
    当消费者缓冲区上没有数据，而生产者缓冲区上有数据——交换缓冲区

*/

#include <iostream>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <functional>

#include "AsyncBuffer.hpp"
#include "Flush.hpp"
#include "Message.hpp"
#include "MyLog.hpp"
#include "LogSystemConfig.hpp"

namespace mylog
{
    enum class ExpandMode
    {
        SOFTEXPANSION = 0,  // 日志大小大于缓冲区整个大小时，增加两倍该日志大小的容量
        HARDEXPANSION       // 日志大小大于缓冲区剩余大小时，增加两倍该日志大小的容量
    };

    class AsyncWorker
    {
    private:
        using LOG_FUNC = std::function<void(std::string)>;

        std::mutex Mutex_;

        std::condition_variable cond_productor_;  // 用于生产者的同步通信
        std::condition_variable cond_consumer_;   // 用于消费者的同步通信

        std::condition_variable cond_writable_;   // 用于生产者缓冲区不足时对写入操作的控制

        std::condition_variable cond_exit_;         // 用于等待生产者消费者都退出后再继续析构

        std::shared_ptr<AsyncBuffer> buffer_productor_;
        std::shared_ptr<AsyncBuffer> buffer_consumer_;

        std::atomic_bool label_consumer_ready_;  // 给生产者判断消费者是否处在空闲状态
        std::atomic_bool label_data_ready_;      // 给生产者判断数据是否传入

        int current_effective_expansion_times;   // 扩容后维持的次数
        int effective_expansion_times;

        LOG_FUNC log_func_;
        ExpandMode expand_mode_;

        bool ExitLabel_;

        bool ExitProductorLabel_;
        bool ExitConsumerLabel_;

        // 测试功能
        std::chrono::_V2::steady_clock::time_point start_time_;  
        std::atomic_int cnt;
        std::atomic_int pdc;


    public:
        AsyncWorker(LOG_FUNC log_func):
            buffer_productor_(std::make_shared<AsyncBuffer>()),
            buffer_consumer_(std::make_shared<AsyncBuffer>()),
            log_func_(log_func),

            label_consumer_ready_(true),
            label_data_ready_(false),
            current_effective_expansion_times(-1),

            start_time_(std::chrono::steady_clock::now()),

            expand_mode_(ExpandMode::HARDEXPANSION)

        {
            ExitLabel_ = false;
            ExitProductorLabel_ = false;
            ExitConsumerLabel_ = false;

            effective_expansion_times = mylog::Config::GetInstance().GetEffectiveExpansionTimes();

            effective_expansion_times = 5;

            pdc = 0;
            cnt = 0;
        }

        ~AsyncWorker()
        {
            std::unique_lock<std::mutex> lock(Mutex_);
            // std::cerr << "~AsyncWorker()" << std::endl;
            // 终止生产者和消费者线程
            ExitLabel_ = true;
            // 通知生产者和消费者
            cond_productor_.notify_all();
            cond_consumer_.notify_all();

            // 生产者和消费者中最后一个结束的时候的通知会让这里从等待到阻塞
            cond_exit_.wait(lock, [this]()->bool { return ExitProductorLabel_ && ExitConsumerLabel_;});  
        }

        // 启动
        void start()
        {   
            std::thread productorThread(std::bind(&AsyncWorker::productorTask, this));
            std::thread consumerThread(std::bind(&AsyncWorker::consumerTask, this));

            productorThread.detach();
            consumerThread.detach();
        }

        void productorTask()  // 生产者线程
        {
            while(!ExitLabel_)
            {
                std::unique_lock<std::mutex> lock(Mutex_);
                // 只要满足消费者已就绪，且用户发来消息，才会唤醒生产者
                cond_productor_.wait(lock, [&]()->bool{
                        return label_consumer_ready_ && label_data_ready_ || ExitLabel_;
                    }
                );

                // 从生产者开始处理第一条日志开始:
                if(pdc == 0) 
                {
                    start_time_ = std::chrono::steady_clock::now();
                    pdc = 1;
                }
                

                if(ExitLabel_)
                {
                    ExitProductorLabel_ = true;
                    cond_exit_.notify_all();
                    return;
                }

                // std::cerr << "即将发生交换, effect_expansion_times: " << effective_expansion_times << std::endl;

                // 交换前对扩容计数的处理，此时可以判断扩容的部分是否被使用到
                if(current_effective_expansion_times > 0)   // 当前是扩容状态
                {   // 如果扩容的部分没有被使用到
                    if(buffer_productor_->getIdleExpansionSpace()) 
                    {   // 计数减一
                        current_effective_expansion_times--;
                    }
                    else  // 如果使用到了
                    {   // 刷新计数
                        current_effective_expansion_times = effective_expansion_times;
                    }
                }

                if(!buffer_productor_->getEmpty())
                {
                    auto tmp_buffer_controler = buffer_productor_;
                    buffer_productor_ = buffer_consumer_;
                    buffer_consumer_ = tmp_buffer_controler;
                    
                    label_consumer_ready_ = false;
                    label_data_ready_ = false;

                    // 此时消费者的buffer就有了数据
                    cond_consumer_.notify_all();
                    // 完成交换后，生产者就有了空间，就可以发通知，告知可以写入
                    cond_writable_.notify_all();
                }
                else  // 如果醒来发现buffer没有消息，就重新循环，此时
                    // 消费者没有被生产者notify_all()还是等待，不会抢互斥锁
                {
                    label_data_ready_ = false;
                }

                // 交换后对扩容状态的处理
                if(current_effective_expansion_times == 0)
                {   // 表示异步工作者认为已经长时间没有使用到扩容的空间，应该释放掉
                    buffer_productor_->scaleDown();  
                    buffer_consumer_->scaleDown();  // 由于生产者判断了交换前的情况，因此消费者接手它的空间后不用担心缩容对数据的影响
                    current_effective_expansion_times = - 1;
                    // std::cerr << "扩容空间长时间未使用，已回到: " << buffer_productor_->getSize() << std::endl;
                }
            }
            
        }

        void consumerTask()   // 消费者线程
        { 
            while(!ExitLabel_)
            {
                std::unique_lock<std::mutex> lock(Mutex_);
                // label_consumer_ready_ = true;
                cond_productor_.notify_all();  // 通知生产者现在消费者空闲状态
                // 只要生产者一声令下，消费者就干活
                cond_consumer_.wait(lock, [&]()->bool { return ExitLabel_ || !label_consumer_ready_;});
                if(ExitLabel_)
                {
                    ExitConsumerLabel_ = true;
                    cond_exit_.notify_all();
                    return;
                }

                label_consumer_ready_ = false;
                
                for(std::string message_formatted: buffer_consumer_->read())
                {
                    // 把日志消息发送到指定的位置
                    cnt++;
                    log_func_(message_formatted);  
                }

                // 如果处理完后发现事件大于1秒了就输出
                if(pdc == 1 && std::chrono::steady_clock::now() - start_time_ > std::chrono::seconds(1))
                {
                    std::cerr << "1秒内处理了: " << cnt << " 条日志" << std::endl;
                    pdc = 2;
                }
                // std::cerr << cnt << std::endl;

                buffer_consumer_->clear();
                // 逻辑上的调整,移动到此处
                label_consumer_ready_ = true;
            }
        }

        // 对外提供一个写入的接口
        void readFromUser(std::string message, unsigned int buffer_length)
        {
            const char* buffer = message.c_str();
            // std::cerr << "当前日志大小: " << buffer_length << std::endl;
            std::unique_lock<std::mutex> lock(Mutex_);
            {
                // 如果生产者的空间不足以写入，就释放锁等待，生产者的缓冲区有空间会通知
                cond_writable_.wait(lock, [&]()->bool{ 
                    // 软扩容，日志大小大于缓冲区整个大小时，增加两倍该日志大小的容量
                    if(expand_mode_ == ExpandMode::SOFTEXPANSION)
                    {   
                        if(buffer_productor_->getSize() <= buffer_length)
                        {   // 扩容生产者和消费者的缓冲区
                            buffer_productor_->scaleUp(2 * buffer_length);
                            buffer_consumer_->scaleUp(2 * buffer_length);
                            current_effective_expansion_times = effective_expansion_times;
                            // std::cerr << "扩容到: " << buffer_productor_->getSize() << std::endl;
                            return true;
                        }
                        else
                        {
                            return buffer_productor_->getAvailable() > buffer_length;
                        }
                    }
                    // 硬扩容，日志大小大于缓冲区剩余大小时，增加两倍该日志大小的容量
                    else if(expand_mode_ == ExpandMode::HARDEXPANSION)
                    {
                        if(buffer_productor_->getAvailable() <= buffer_length)
                        {   // 扩容生产者和消费者的缓冲区
                            buffer_productor_->scaleUp(2 * buffer_length);
                            buffer_consumer_->scaleUp(2 * buffer_length);
                            current_effective_expansion_times = effective_expansion_times;
                            // std::cerr << "扩容到: " << buffer_productor_->getSize() << std::endl;
                            return true;
                        }
                        else
                        {
                            // return buffer_productor_->getAvailable() > buffer_length;
                            return true;
                        }
                    }
                    else
                    {
                        perror("Invalid expansion method");
                    }
                    return true;    
                });
                buffer_productor_->write(buffer, buffer_length);   // 把日志信息写入生产者的buffer中

                label_data_ready_ = true;  // 设置标志
                cond_productor_.notify_all();   // 并通知生产者可以来处理日志信息了
            } 
        }
    };

    using AsyncWorkerPtr = std::shared_ptr<AsyncWorker>;
}
#endif