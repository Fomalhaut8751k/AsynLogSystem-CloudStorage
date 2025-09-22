#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <thread>
#include <mutex>
#include <future>
#include <condition_variable>
#include <unordered_map>
#include <functional>
#include <queue>
#include "backlog/ClientBackupLog.hpp"

#define INIT_THREADSIZE 4
#define THREAD_SIZE_THRESHHOLD 8
#define LOGQUE_MAX_THRESHHOLD 4

namespace mylog
{
    // 线程类
    class Thread
    {
    private:
        using t_uint = unsigned int;
        using ThreadFunc = std::function<void(int)>;
        
        t_uint threadId_;     // 线程号
        ThreadFunc threadfunc_;      // 线程函数
        static t_uint generateId_;        // 

    public:
        Thread(ThreadFunc threadfunc)
        {
            threadfunc_ = threadfunc;
            threadId_ = generateId_++;
        }

        void start()
        {
            std::thread t(threadfunc_, threadId_);
            t.detach();
        }

        // 获取线程id
        int getId() const
        {
            return threadId_;
        }
        
    };
    unsigned int Thread::generateId_ = 1;

    // 线程池类（设计为单例模式）
    class ThreadPool
    {
    private:
        using tp_uint = unsigned int;
        using tp_log = std::string;
        // using Task = std::function<void()>;

        ThreadPool(const std::string addr = "127.0.0.1", 
                    unsigned int port = 8000)
        {
            setup(addr, port);
            ThreadPoolRunning_ = true;  // 表示线程池正在运行中
        }

        ~ThreadPool()   
        {
            // std::cerr << "~ThreadPool()" << std::endl;
            // 线程池析构的时候要把所有事件循环关闭
            std::unique_lock<std::mutex> lock(logQueMtx_);
            ThreadPoolRunning_ = false;
            // 唤醒所有线程，当然不一定所有线程都在睡觉
            notEmpty_.notify_all();  
            // 等待所有线程关闭，可能在这里会被通知4次
            Exit_.wait(lock, [&]()->bool { return curThreadSize_ == 0;}); 

            // std::cout << "~ThreadPool() finish" << std::endl;
        }

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        tp_uint initThreadSize_;    // 初始线程大小
        tp_uint curThreadSize_;     // 当前线程大小（成功连接上服务器的）
        tp_uint threadSizeThreshHold_;   // 线程上限大小
        std::unordered_map<int, std::unique_ptr<Thread>> threads_;   // 线程列表

        tp_uint logSize_;                // 日志数量
        tp_uint logQueMaxThreshHold_;    // 日志数量上限大小
        std::queue<tp_log> logQue_;      // 日志队列

        std::mutex logQueMtx_;               // 控制队列进出的互斥操作
        std::condition_variable notEmpty_;    // 任务队列非空，线程应该及时取走任务
        std::condition_variable notFull_;     // 任务队列非满，提交的任务可以被接受
        std::condition_variable Exit_;        // 用于线程池析构

        // 服务器信息
        std::string serverAddr_;
        unsigned int serverPort_;

        // 线程池启动情况
        std::atomic_bool ThreadPoolRunning_;

        void threadFunc(tp_uint threadid)
        {
            // 创建客户端
            std::unique_ptr<Client> client_ =
                std::make_unique<Client>(serverAddr_, serverPort_, threadid);
            // 启动客户端并连接服务器, 其中包含了event_base_dispatch(base);
            if(client_->start())
            {
                // std::cout << "=> thread id: " << std::this_thread::get_id() \
                //         << " connected server successful!" << std::endl;
                curThreadSize_++;
            }
            else
            {
                // std::cout << "=> thread id: " << std::this_thread::get_id() \
                //         << " connected server failed!" << std::endl;
                client_->stop();
                return;
            }

            // 日志信息
            tp_log log = "";

            while(ThreadPoolRunning_ || logSize_)
            {
                {   // 先获取锁
                    std::unique_lock<std::mutex> lock(logQueMtx_);  
                    // 尝试获取任务
                    notEmpty_.wait(lock, [&]()->bool{ return logSize_ > 0 || ThreadPoolRunning_ == false; });   // 等待日志队列中有任务被提交，就会notify唤醒
                    // 确认taskQue_确实不为空
                    if(logSize_ > 0)
                    {
                        log = logQue_.front();  // std::function<void()>;
                        logQue_.pop();
                        logSize_--;
                    }
                    else  // 唤醒条件要么有日志，要么线程池关闭了，但是先判断有日志
                    { 
                        curThreadSize_--;
                        break;
                    }
                    
                    // 如果发现logQue_中还有队列，就通知其他人来消费
                    if(logSize_ > 0)
                    {
                        notEmpty_.notify_all();
                    }     
                }

                // 这个如果放在锁里面，就发挥不了线程池的特点了
                if(log.length() > 0)
                {
                    client_->log_write(log);
                    log = "";
                }
            }

            client_->stop();  // 清理base, bv，关闭事件循环
            Exit_.notify_all();  // 通知析构函数那里，我这个线程已经清理完成了
            
            // std::cout << "【线程池第" << threadid << "个线程关闭】\n" << std::endl;  
        }

    public:
        static ThreadPool& GetInstance()
        {
            static ThreadPool threadpool;
            return threadpool;
        }

        void setup(std::string server_addr, 
                unsigned int server_port,
                tp_uint initThreadSize = INIT_THREADSIZE,
                tp_uint threadSizeThreshHold =  THREAD_SIZE_THRESHHOLD
        )
        {
            serverAddr_ = server_addr;
            serverPort_ = server_port;

            initThreadSize_ = initThreadSize;
            curThreadSize_ = 0;
            threadSizeThreshHold_ = threadSizeThreshHold;

            logSize_ = 0;
            logQueMaxThreshHold_ = LOGQUE_MAX_THRESHHOLD;
        }

        void startup()
        {
            // 先创建足够数量的线程
            for(int i = 0; i < initThreadSize_; i++)
            {
                std::unique_ptr<Thread> thread_ = std::make_unique<Thread>(
                    std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1)
                );
                threads_.emplace(thread_.get()->getId(), std::move(thread_));  
            }

            for(int index = 1; index <= initThreadSize_; index++)
            {
                threads_[index].get()->start();   // 启动线程,Thread自己会设置为分离线程
                // std::cerr << "【线程池第" << index << "个线程就绪】\n" << std::endl;
            }
        }

        void submitLog(const std::string log_message)
        {
            std::unique_lock<std::mutex> lock(logQueMtx_);
            // 如果当前队列已经满了,就等待队列为空，或者超时，如果是因为队列空唤醒的，则可以执行接下来的提交任务操作，否则视为提交失败
            if(!notFull_.wait_for(lock, 
                                    std::chrono::seconds(10),           // 超时
                                    [this]()->bool{ return logSize_ < logQueMaxThreshHold_;}) 
            ) 
            {   // 如果是超时返回的，视为提交失败
                std::cerr << "task queue is full, submit task fail." << std::endl;
            }
            // 如果是因为taskQue_有空闲位置而被唤醒，则添加到任务队列，并通知线程
            logQue_.emplace(log_message);
            logSize_++;
            notEmpty_.notify_all();        
        }

    };
}

#endif