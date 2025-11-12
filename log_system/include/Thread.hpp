#ifndef THREAD_H
#define THREAD_H

#include <thread>
#include <mutex>
#include <future>
#include <condition_variable>
#include <unordered_map>
#include <functional>
#include <queue>

namespace mylog
{
    // 线程类
    class Thread
    {
    private:
        using t_uint = unsigned int;
        using ThreadFunc = std::function<void(int, Thread*)>;
        using WaitFunc = std::function<void()>;
        
        t_uint threadId_;     // 线程号
        ThreadFunc threadfunc_;      // 线程函数
        WaitFunc waitfunc_;          // 唤醒函数
        static t_uint generateId_;      

        std::atomic_bool clientactive_;  

    public:
        Thread(ThreadFunc threadfunc, WaitFunc waitfunc)
        {
            threadfunc_ = threadfunc;
            waitfunc_ = waitfunc;

            threadId_ = generateId_++;
            clientactive_ = true;
        }

        void start()
        {
            std::thread t(threadfunc_, threadId_, this);
            t.detach();
        }

        // 获取线程id
        int getId() const
        {
            return threadId_;
        }        

        void Deactivate(){ clientactive_ = false; waitfunc_(); }  // client退出后，唤醒线程

        bool ClientActiveStatus() const { return clientactive_; }
    };
    unsigned int Thread::generateId_ = 1;
}

#endif