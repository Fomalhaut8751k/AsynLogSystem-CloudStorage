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
#include "LogSystemConfig.hpp"
#include "Thread.hpp"


namespace mylog
{

    class ThreadPool
    {
    private:
        using tp_uint = unsigned int;
        using tp_atomic_uint = std::atomic_uint16_t;
        using tp_log = std::string;
        // using Task = std::function<void()>;

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        tp_uint initThreadSize_;    // 初始线程大小
        tp_atomic_uint curThreadSize_;     // 当前线程大小（成功连接上服务器的）
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

        void waitFunc()
        {
            notEmpty_.notify_all();
        }

        void threadFunc(tp_uint threadid, Thread* thread_)
        {
            // 创建客户端
            std::unique_ptr<Client> client_ =
                std::make_unique<Client>(serverAddr_, serverPort_, threadid, thread_);
            // 启动客户端并连接服务器, 其中包含了event_base_dispatch(base);

            if(client_->start())
            {
                curThreadSize_++;
            }
            else
            {
                client_->stop();
                threads_.erase(threadid);
                // curThreadSize_--;
                return;
            }

            // 日志信息
            tp_log log = "";

            while(ThreadPoolRunning_ || logSize_)
            {
                {   // 先获取锁
                    std::unique_lock<std::mutex> lock(logQueMtx_);  
                    // 尝试获取任务
                    notEmpty_.wait(lock, [&]()->bool{   // 如果client断开连接，就会被直接唤醒。
                        return !client_->GetConnectedStatus() || logSize_ > 0 || ThreadPoolRunning_ == false; 
                    }); 
                    // 如果是因为有日志但是client已经断开连接而来到这里                        
                    if(!client_->GetConnectedStatus())
                    {   // 关闭Client并通知其他线程也关闭
                        client_->stop(); 
                        threads_.erase(threadid);
                        notEmpty_.notify_all();
                        curThreadSize_--;
                        return;
                    }
                    // 确认taskQue_确实不为空
                    if(logSize_ > 0)
                    {
                        log = logQue_.front(); 
                        logQue_.pop();
                        logSize_--;
                    }
                    else  // 唤醒条件要么有日志，要么线程池关闭了，但是先判断有日志，保证下达关闭指令后先处理完日志
                    { 
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
            
            curThreadSize_--;
            client_->stop();  // 清理base, bv，关闭事件循环
            threads_.erase(threadid);
            Exit_.notify_all();  // 通知析构函数那里，我这个线程已经清理完成了
        }

    public:    
        ThreadPool()
        { 
            // 为 Libevent 启用 POSIX 线程（pthreads）支持
            evthread_use_pthreads();  
        }

        ~ThreadPool()   
        {
            // 线程池析构的时候要把所有事件循环关闭
            std::unique_lock<std::mutex> lock(logQueMtx_);
            ThreadPoolRunning_ = false;
            // 唤醒所有线程，当然不一定所有线程都在睡觉
            notEmpty_.notify_all();  
            // 等待所有线程关闭，可能在这里会被通知4次
            Exit_.wait(lock, [&]()->bool { return curThreadSize_ == 0;}); 
        }

        void reloadnotifyall()
        {
            notEmpty_.notify_all();
        }

        void setup()
        {
            serverAddr_ = mylog::Config::GetInstance().GetBackLogIpAddr();
            serverPort_ = mylog::Config::GetInstance().GetBackLogPort();

            initThreadSize_ = mylog::Config::GetInstance().GetInitThreadSize();
            threadSizeThreshHold_ = mylog::Config::GetInstance().GetThreadSizeThreshhold();

            logQueMaxThreshHold_ = mylog::Config::GetInstance().GetLogQueMaxThreshhold();

            // std::cerr << serverAddr_ << " " << serverPort_ << " " << initThreadSize_ << " " << threadSizeThreshHold_ << " " << logQueMaxThreshHold_ << std::endl;

            curThreadSize_ = 0;
            logSize_ = 0;

            ThreadPoolRunning_ = true;  // 表示线程池正在运行中
        }

        std::pair<std::string, mylog::LogLevel> startup()
        {
            std::vector<int> threadids_;

            // std::cerr << "threads_: " << threads_.size() << std::endl;
            // std::cerr << "curThreadSize_: " << curThreadSize_ << std::endl; 

            // 先创建足够数量的线程
            for(int i = 0; i < initThreadSize_; i++)
            {
                std::unique_ptr<Thread> thread_ = std::make_unique<Thread>(
                    std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1, std::placeholders::_2),
                    std::bind(&ThreadPool::waitFunc, this)
                );

                int threadid_ = thread_.get()->getId();
                threads_.emplace(threadid_, std::move(thread_));  
                threadids_.emplace_back(threadid_);
            }

            std::cerr << threads_.size() << std::endl;
            for(int& threadid: threadids_)
            {
                threads_[threadid].get()->start();   // 启动线程,Thread自己会设置为分离线程  unordered_map
            }

            // 休眠20ms保证所有线程都返回并根据自己情况修改curThreadSize_的值
            std::this_thread::sleep_for(std::chrono::milliseconds(20));

            if(curThreadSize_ == 0)  // 没有连接上远程服务器
            {
                return {"No thread connected to the remote server, please check the startup status of the remote server", mylog::LogLevel::WARN};
            }
            else
            {
                return {"There are " + std::to_string(curThreadSize_) + " threads connecting to the remote server here", mylog::LogLevel::INFO};
            }
        }

        void submitLog(const std::string log_message)
        {
            std::unique_lock<std::mutex> lock(logQueMtx_);
            // 如果当前队列已经满了,就等待队列为空，或者超时，如果是因为队列空唤醒的，则可以执行接下来的提交任务操作，否则视为提交失败
            if(!notFull_.wait_for(lock, 
                                    std::chrono::seconds(10),           // 超时
                                    [this]()->bool{ 
                                        return logSize_ < logQueMaxThreshHold_;
                                    }) 
            ) 
            {   // 如果是超时返回的，视为提交失败
                std::cerr << "task queue is full, submit task fail." << std::endl;
            }
            // 如果是因为taskQue_有空闲位置而被唤醒，则添加到任务队列，并通知线程
            logQue_.emplace(log_message);
            logSize_++;
            notEmpty_.notify_all();        
        }

        // 判断处在线程函数中的线程的个数(运行中但不一定是正常运行，因为他的Client可能挂了)
        bool Connected() const { std::cerr << curThreadSize_ << std::endl; return curThreadSize_ > 0;}
   
        // 返回处在线程函数中的线程并且正常运行的线程的个数
        int ClientActiveNumber() const
        {
            int number = 0;
            for(auto& item: threads_)
            {
                if(item.second->ClientActiveStatus())
                {
                    number++;
                }
            }
            return number;
        }
    };
}

#endif