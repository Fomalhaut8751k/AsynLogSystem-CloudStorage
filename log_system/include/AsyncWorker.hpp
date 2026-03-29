#ifndef ASYNCWORKER_H
#define ASYNCWORKER_H
/*
    AsyncWorker对象控制的双缓冲区-消费者缓冲区和生产者缓冲区
    当消费者缓冲区有数据上——阻塞
    当消费者缓冲区上没有数据，而生产者缓冲区上有数据——交换缓冲区

*/

#include <iostream>
#include <mutex>
#include <shared_mutex>
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

#define LOG_BUFFER_MAX_SIZE  (64 * 1024 * 1024)   // 64MB 
#define SPACE_AVAILABLE 4294967296  // 4G

enum class WaitStatus {
    DISKINSUFFICIENT,   // 磁盘空间不足
    BUFFERABAILABLE,    // 缓冲区大小充足
    BLUE
};

enum class Status{
    BUFFERABAILABLE,
    EXPAND
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
        std::condition_variable_any cond_writabel_v2_; 

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

        std::shared_mutex SharedMutex_;
        std::mutex InfoMutex_;

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
                cond_productor_.wait(lock, [&]()->bool{  // 只要满足消费者已就绪，且用户发来消息，才会唤醒生产者
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
                    std::unique_lock<std::mutex> lock_swap(Mutex_User);
                    // std::unique_lock write(SharedMutex_);

                    auto tmp_buffer_controler = buffer_productor_;
                    buffer_productor_ = buffer_consumer_;
                    buffer_consumer_ = tmp_buffer_controler;
                    
                    label_consumer_ready_ = false;
                    label_data_ready_ = false;

                    // 此时消费者的buffer就有了数据
                    cond_consumer_.notify_all();
                    // 完成交换后，生产者就有了空间，就可以发通知，告知可以写入
                    cond_writable_.notify_all();
                    // cond_writabel_v2_.notify_all();

                }
                else{  // 如果醒来发现buffer没有消息，就重新循环，此时消费者没有被生产者notify_all()还是等待，不会抢互斥锁
                    label_data_ready_ = false;
                }
                // 交换后对扩容状态的处理
                // #######################################################################################################
                if(current_effective_expansion_times == 0) {
                    // 表示异步工作者认为已经长时间没有使用到扩容的空间，应该释放掉
                    std::unique_lock<std::mutex> lock_swap(Mutex_User);
                    // std::unique_lock writelock(SharedMutex_);
                    buffer_productor_->scaleDown();  
                    buffer_consumer_->scaleDown();  // 由于生产者判断了交换前的情况，因此消费者接手它的空间后不用担心缩容对数据的影响
                    current_effective_expansion_times = -1;
                }
                // #######################################################################################################
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

        void readFromUser1(std::string message, unsigned int buffer_length){
            if(ProhibitSummbitLabel_) return;   // 如果禁止提交，已经进来排队的就等处理，如果刚进来的就劝退
            
            if(buffer_length > LOG_BUFFER_MAX_SIZE){  // 如果日志大小大于缓冲区最大可以扩容的长度，就丢弃
                log_func_default(
                    std::string("Received a very large log, which has been discarded: ") + \
                    std::to_string(buffer_length) + " > " + std::to_string(LOG_BUFFER_MAX_SIZE)
                );
                return; 
            }

            enum Status status;
            const char* buffer = message.c_str();
            user_current_count_ += 1;

            while(1){
                // -------------------------------------------------------------------------------------------------------
                {   std::shared_lock readlock(SharedMutex_);   // 读锁，允许同时有多个读操作
                    
                    if(info_available < SPACE_AVAILABLE){   // 如果磁盘空间不足，就需要暂停写入，等开发者手动清理内存，而不是直接丢弃
                        log_func_default("Disk warning: The current available space is insufficient. Please free up space");
                        std::this_thread::sleep_for(std::chrono::seconds(5));  // 先休眠5秒，然后更新DiskInfo（这里在读锁，因此不会影响其他线程）
                        {
                            std::lock_guard<std::mutex> infolock(InfoMutex_);
                            info = DiskSpaceChecker::get_disk_info("/");  
                        }
                        info_available = info.available_bytes;   // 这是个原子类型
                        continue;  // 继续循环
                    } 
                    if(buffer_productor_->getAvailable() <= buffer_length){   // 如果磁盘空间足够，但缓冲区的大小不够
                        if(buffer_productor_->getSize() >= LOG_BUFFER_MAX_SIZE){   // 可以扩容
                            status = Status::EXPAND;
                        }else{  // 不可以扩容，也就是当前缓冲区的大小已经扩容到上限了
                            cond_writabel_v2_.wait(readlock);
                            continue;
                        }
                    }
                    else{  // 磁盘空间足够，且缓冲区大小足够
                        status = Status::BUFFERABAILABLE;
                    }
                }
                // -------------------------------------------------------------------------------------------------------
                // #######################################################################################################
                {   std::unique_lock writelock(SharedMutex_);   // 写锁，只允许有一个写操作，并且不能读
                    if(status == Status::EXPAND){  
                        if(buffer_productor_->getSize() < LOG_BUFFER_MAX_SIZE  // 锁加双重判断
                                && buffer_productor_->getAvailable() <= buffer_length){
                            buffer_productor_->scaleUp(buffer_productor_->getSize(), 0);  // 扩容生产者
                            buffer_consumer_->scaleUp(buffer_productor_->getSize(), 1);   // 扩容消费者
                            current_effective_expansion_times = effective_expansion_times;
                        }
                    }  // 扩容之后，可以直接写了，但是一次扩容不一定就够了
                    // if(status == Status::BUFFERABAILABLE){
                    if(buffer_productor_->getAvailable() > buffer_length){   // 锁加双重判断
                        status = Status::BUFFERABAILABLE;   // 作为判断标准，因为有可能是扩容后才可以写的
                        buffer_productor_->write(buffer, buffer_length);   // 把日志信息写入生产者的buffer中
                        break;
                    }else continue;  
                }
                // #######################################################################################################
            } 
            /*
                如果磁盘空间不足，就【continue】，等待有人清理磁盘空间
                如果磁盘空间足够，但是不能扩容了，就【break】;
                如果磁盘空间足够，且能扩容，就扩容
                如果磁盘空间足够，且不需要扩容就能写入数据，就写入

                走到这里，要么数据已经写入，要么数据太大且无法扩容
            */
            if(status == Status::BUFFERABAILABLE){
                label_data_ready_ = true;
                cond_productor_.notify_all();
            }
            user_current_count_ -= 1;
            if(ProhibitSummbitLabel_ && user_current_count_ == 0){
                cond_exit_.notify_all();
            }
        }

        void readFromUser2(std::string message, unsigned int buffer_length) {
            // 快速检查：禁止提交
            if (ProhibitSummbitLabel_.load(std::memory_order_acquire)) {
                return;
            }
            
            // 快速检查：日志太大
            if (buffer_length > LOG_BUFFER_MAX_SIZE) {
                log_func_default(
                    std::string("Received a very large log, which has been discarded: ") + 
                    std::to_string(buffer_length) + " > " + std::to_string(LOG_BUFFER_MAX_SIZE)
                );
                return;
            }
            
            const char* buffer = message.c_str();
            user_current_count_ += 1;
            
            // 阶段1：快速路径 - 尝试无锁写入（使用原子操作）
            bool written = false;
            
            // 先快速检查磁盘空间（原子读取）
            if (info_available.load(std::memory_order_acquire) >= SPACE_AVAILABLE) {
                // 尝试使用读锁快速检查缓冲区
                {
                    std::shared_lock read_lock(SharedMutex_, std::try_to_lock);
                    if (read_lock.owns_lock()) {
                        size_t available = buffer_productor_->getAvailable();
                        if (available > buffer_length) {
                            // 需要升级为写锁，使用 try_lock 避免死锁
                            read_lock.unlock();
                            
                            std::unique_lock write_lock(SharedMutex_, std::try_to_lock);
                            if (write_lock.owns_lock()) {
                                // 双重检查
                                if (info_available.load(std::memory_order_acquire) >= SPACE_AVAILABLE &&
                                    buffer_productor_->getAvailable() > buffer_length) {
                                    buffer_productor_->write(buffer, buffer_length);
                                    written = true;
                                    label_data_ready_.store(true, std::memory_order_release);
                                    cond_productor_.notify_all();
                                }
                            }
                        }
                    }
                }
            }
            
            // 快速路径成功
            if (written) {
                user_current_count_ -= 1;
                if (ProhibitSummbitLabel_.load() && user_current_count_ == 0) {
                    cond_exit_.notify_all();
                }
                return;
            }
            
            // 阶段2：慢速路径 - 使用条件变量和互斥锁
            // 这里使用独立的互斥锁，避免与快速路径的读写锁冲突
            std::unique_lock<std::mutex> lock(Mutex_User);
            
            enum class SlowPathStatus {
                NEED_WAIT,
                SPACE_READY,
                DISK_FULL,
                NEED_EXPAND
            };
            
            while (true) {
                // 检查条件
                SlowPathStatus current_status = SlowPathStatus::NEED_WAIT;
                
                cond_writable_.wait(lock, [&]() -> bool {
                    // 1. 检查磁盘空间
                    uint64_t current_available = info_available.load(std::memory_order_acquire);
                    if (current_available < SPACE_AVAILABLE) {
                        log_func_default("Disk warning: The current available space is insufficient. Please free up space");
                        current_status = SlowPathStatus::DISK_FULL;
                        return true;
                    }
                    
                    // 2. 检查缓冲区空间
                    size_t available = buffer_productor_->getAvailable();
                    if (available > buffer_length) {
                        current_status = SlowPathStatus::SPACE_READY;
                        return true;
                    }
                    
                    // 3. 尝试扩容
                    if (buffer_productor_->getSize() < LOG_BUFFER_MAX_SIZE) {
                        current_status = SlowPathStatus::NEED_EXPAND;
                        return true;
                    }
                    
                    // 无法满足，继续等待
                    return false;
                });
                
                // 根据状态处理
                switch (current_status) {
                    case SlowPathStatus::SPACE_READY: {
                        // 写入数据
                        buffer_productor_->write(buffer, buffer_length);
                        goto write_success;
                    }
                    
                    case SlowPathStatus::DISK_FULL: {
                        // 磁盘空间不足，休眠后重试
                        lock.unlock();
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                        
                        // 更新磁盘信息
                        {
                            std::lock_guard<std::mutex> info_lock(InfoMutex_);
                            info = DiskSpaceChecker::get_disk_info("/");
                            info_available.store(info.available_bytes, std::memory_order_release);
                        }
                        
                        lock.lock();
                        continue;
                    }
                    
                    case SlowPathStatus::NEED_EXPAND: {
                        // 需要扩容
                        // 注意：扩容操作需要写锁保护
                        lock.unlock();  // 先释放 Mutex_User，避免死锁
                        
                        {
                            std::unique_lock write_lock(SharedMutex_);
                            
                            // 双重检查
                            if (buffer_productor_->getSize() < LOG_BUFFER_MAX_SIZE &&
                                buffer_productor_->getAvailable() <= buffer_length) {
                                
                                // 循环扩容直到满足需求
                                while (buffer_productor_->getSize() < LOG_BUFFER_MAX_SIZE &&
                                    buffer_productor_->getAvailable() <= buffer_length) {
                                    size_t current_size = buffer_productor_->getSize();
                                    buffer_productor_->scaleUp(current_size, 0);
                                    buffer_consumer_->scaleUp(buffer_productor_->getSize(), 1);
                                    current_effective_expansion_times = effective_expansion_times;
                                }
                            }
                        }
                        
                        lock.lock();
                        
                        // 扩容后再次检查
                        if (buffer_productor_->getAvailable() > buffer_length) {
                            buffer_productor_->write(buffer, buffer_length);
                            goto write_success;
                        }
                        continue;
                    }
                    
                    default:
                        continue;
                }
            }
            
        write_success:
            // 写入成功后的处理
            label_data_ready_.store(true, std::memory_order_release);
            cond_productor_.notify_all();
            
            user_current_count_ -= 1;
            if (ProhibitSummbitLabel_.load(std::memory_order_acquire) && 
                user_current_count_.load() == 0) {
                cond_exit_.notify_all();
            }
        }
    };

    using AsyncWorkerPtr = std::shared_ptr<AsyncWorker>;
}
#endif