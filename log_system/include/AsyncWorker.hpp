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
#include "utils/df.hpp"

#define LOG_BUFFER_INIT_SIZE   (4 * 1024 * 1024)   // 4MB
#define LOG_BUFFER_MAX_SIZE  (64 * 1024 * 1024)   // 64MB 
#define SPACE_AVAILABLE 4294967296  // 4G

enum class WaitStatus {
    DISKINSUFFICIENT,   // 磁盘空间不足
    BUFFERABAILABLE,    // 缓冲区大小充足
    BLUE
};

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
        using LOG_FUNC_DEFAULT = std::function<void(const std::string&string)>;

        std::mutex Mutex_;
        std::mutex Mutex_User;

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
        LOG_FUNC_DEFAULT log_func_default;  // 备用的输出模式，将日志不通过缓冲区直接输出到控制台

        ExpandMode expand_mode_;

        std::atomic_bool ProhibitSummbitLabel_;
        bool ExitLabel_;

        bool ExitProductorLabel_;
        bool ExitConsumerLabel_;

        // 设置一个计数，只有要用户调用提交函数就+1，提交完走人后就-1
        std::atomic_int user_current_count_;   

        // 记录磁盘磁盘管理状态
        DiskSpaceChecker::DiskInfo info;
        std::atomic_uint64_t info_available;  // 记录在变量中，避免频繁使用系统调用

    public:
        AsyncWorker(LOG_FUNC log_func, LOG_FUNC_DEFAULT log_func_default):
            buffer_productor_(std::make_shared<AsyncBuffer>()),
            buffer_consumer_(std::make_shared<AsyncBuffer>()),

            log_func_(log_func),
            log_func_default(log_func_default),

            label_consumer_ready_(true),
            label_data_ready_(false),
            current_effective_expansion_times(-1),

            user_current_count_(0),

            // expand_mode_(ExpandMode::HARDEXPANSION)
            expand_mode_(ExpandMode::SOFTEXPANSION)
        {
            ExitLabel_ = false;
            ExitProductorLabel_ = false;
            ExitConsumerLabel_ = false;
            ProhibitSummbitLabel_ = false;

            effective_expansion_times = mylog::Config::GetInstance().GetEffectiveExpansionTimes();

            effective_expansion_times = 5;

            info = DiskSpaceChecker::get_disk_info("/");
            info_available = info.available_bytes;
        }

        ~AsyncWorker()
        {
            {   // 这是保证剩下用户全部提交的，最后一个用户提交后就会通知
                std::unique_lock<std::mutex> lock(Mutex_User);
                // 第一阶段，关闭用户提交日志窗口，等待剩余日志处理完成
                ProhibitSummbitLabel_ = true;
                cond_exit_.wait(lock, [&]()->bool { return user_current_count_ == 0; } );
            }
            
            {   // 这是保证生产者和消费者处理完所有日志的
                std::unique_lock<std::mutex> lock(Mutex_);
                // 第二阶段，通知生产者和消费者，等待消费者和生产者关闭
                ExitLabel_ = true;
                cond_productor_.notify_all();
                cond_consumer_.notify_all();
                cond_exit_.wait(lock, [this]()->bool { 
                    return ExitProductorLabel_ && ExitConsumerLabel_;
                });  // 生产者和消费者中最后一个结束的时候的通知会让这里从等待到阻塞
            }
            // std::cout << "~AsyncWorker()" << std::endl;
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
            while(1){
                std::unique_lock<std::mutex> lock(Mutex_);
                // 只要满足消费者已就绪，且用户发来消息，才会唤醒生产者
                cond_productor_.wait(lock, [&]()->bool{
                        return label_consumer_ready_ && label_data_ready_ || ExitLabel_;
                    }
                );
                if(buffer_productor_->getEmpty() && ExitLabel_){  // 确保退出的时候缓冲区内没有数据
                    ExitProductorLabel_ = true;
                    cond_exit_.notify_all();
                    return;
                }

                // 交换前对扩容计数的处理，此时可以判断扩容的部分是否被使用到
                if(current_effective_expansion_times > 0){   // 当前是扩容状态
                    // 如果扩容的部分没有被使用到
                    if(buffer_productor_->getIdleExpansionSpace()) {   
                        current_effective_expansion_times--;  // 计数减一
                    }
                    else { // 如果使用到了
                        current_effective_expansion_times = effective_expansion_times;  // 刷新计数
                    }
                }

                // 交换缓冲区
                if(!buffer_productor_->getEmpty()){

                    // 更新的可用空间,减少量等于此次缓冲区中数据的大小
                    info_available -= (buffer_productor_->getSize() - buffer_productor_->getAvailable());

                    // 这里使用的是Mutex_User也就是用户判断空间是否可以写入数据的时候
                    // 但是并没有和用户写入数据的操作互斥，因此写和交换可能同时发生。
                    std::unique_lock<std::mutex> lock_swap(Mutex_User);
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
                else{  // 如果醒来发现buffer没有消息，就重新循环，此时消费者没有被生产者notify_all()还是等待，不会抢互斥锁
                    label_data_ready_ = false;
                }

                // 交换后对扩容状态的处理
                if(current_effective_expansion_times == 0) {
                    // 表示异步工作者认为已经长时间没有使用到扩容的空间，应该释放掉
                    std::unique_lock<std::mutex> lock_swap(Mutex_User);
                    buffer_productor_->scaleDown();  
                    buffer_consumer_->scaleDown();  // 由于生产者判断了交换前的情况，因此消费者接手它的空间后不用担心缩容对数据的影响
                    current_effective_expansion_times = -1;
                    // std::cerr << "扩容空间长时间未使用，已回到: " << buffer_productor_->getSize() << std::endl;
                }
            }
        }

        void consumerTask()   // 消费者线程
        { 
            // 消费者唯一的出口就是从等待哪里出去
            while(1)
            {
                std::string message_formatted;
                {
                    std::unique_lock<std::mutex> lock(Mutex_);
                    cond_productor_.notify_all();  // 通知生产者现在消费者空闲状态
                    // 只要生产者一声令下，消费者就干活
                    cond_consumer_.wait(lock, [&]()->bool { return ExitLabel_ || !label_consumer_ready_;});
                    if(buffer_consumer_->getEmpty() && ExitLabel_ && ExitProductorLabel_){
                        ExitConsumerLabel_ = true;
                        cond_exit_.notify_all();
                        return;
                    }
                    label_consumer_ready_ = false;
                    if(!buffer_consumer_) continue;
                    // message_formatted_vector = buffer_consumer_->read();
                    if(!buffer_consumer_->getEmpty()){
                        message_formatted = buffer_consumer_->read(0);
                    }
                    buffer_consumer_->clear();
                    label_consumer_ready_ = true;
                }
                log_func_(message_formatted);  // 一次性将所有日志输出
            }
        }

        // 对外提供一个写入的接口
        void readFromUser(std::string message, unsigned int buffer_length)
        {
            // 如果禁止提交，已经进来排队的就等处理，如果刚进来的就劝退
            if(ProhibitSummbitLabel_) { return; }

            // 如果日志大小大于缓冲区最大可以扩容的长度，就丢弃
            if(buffer_length > LOG_BUFFER_MAX_SIZE){  
                log_func_default(
                    std::string("Received a very large log, which has been discarded: ") + \
                    std::to_string(buffer_length) + " > " + std::to_string(LOG_BUFFER_MAX_SIZE)
                );
                return; 
            }

            const char* buffer = message.c_str();
            user_current_count_ += 1;

            {
                std::unique_lock<std::mutex> lock(Mutex_User);  
                // 如果生产者的空间不足以写入，就释放锁等待，生产者的缓冲区有空间会通知
                enum WaitStatus status;
                unsigned int buffer_size = 0;
                while(1){
                    cond_writable_.wait(
                        lock, [&]()->bool{ 
                            // 如果磁盘空间不足，就需要暂停写入，等开发者手动清理内存，而不是直接丢弃
                            if(info_available < SPACE_AVAILABLE){  
                                log_func_default("Disk warning: The current available space is insufficient. Please free up space");
                                status = WaitStatus::DISKINSUFFICIENT; 
                                return true;
                            }
                            // 如果磁盘空间足够，且缓冲区的大小足够，就直接返回
                            if(buffer_productor_->getAvailable() > buffer_length){  
                                status = WaitStatus::BUFFERABAILABLE;
                                return true;
                            }
                            // 如果不够，并且已经达到缓冲区扩容的最大容量上限了
                            else{ 
                                if(buffer_productor_->getSize() >= LOG_BUFFER_MAX_SIZE){  
                                    // log_func_default("The buffer has been expanded to its maximum capacity, and log submission will be blocked");
                                    return false;
                                }
                                // 否则就扩容，扩容生产者和消费者的缓冲区, 到原来的两倍
                                while(buffer_productor_->getSize() < LOG_BUFFER_MAX_SIZE && buffer_productor_->getAvailable() <= buffer_length){  
                                    buffer_productor_->scaleUp(buffer_productor_->getSize(), 0);
                                    buffer_consumer_->scaleUp(buffer_productor_->getSize(), 1);  // 扩容的时候也会扩消费者缓冲区，但是和消费者线程不互斥
                                    current_effective_expansion_times = effective_expansion_times;
                                }
                                if(buffer_productor_->getAvailable() > buffer_length){
                                    status = WaitStatus::BUFFERABAILABLE;
                                    return true;
                                }
                                else{
                                    return false;
                                }
                            }
                            return true;
                        }
                    );
                    if(status == WaitStatus::BUFFERABAILABLE){
                        buffer_productor_->write(buffer, buffer_length);   // 把日志信息写入生产者的buffer中
                        break;
                    }
                    else if(status == WaitStatus::DISKINSUFFICIENT){
                        std::this_thread::sleep_for(std::chrono::seconds(5));  // 先休眠5秒，然后更新DiskInfo
                        info = DiskSpaceChecker::get_disk_info("/");
                        info_available = info.available_bytes; 
                    }
                }
                
            }
            label_data_ready_ = true;  // 设置标志
            cond_productor_.notify_all();   // 并通知生产者可以来处理日志信息了

            user_current_count_ -= 1;
            // 如果是最后一个用户，就提醒析构函数
            if(ProhibitSummbitLabel_ && user_current_count_ == 0)
            {
                cond_exit_.notify_all();
            }
        }
    };

    using AsyncWorkerPtr = std::shared_ptr<AsyncWorker>;
}
#endif