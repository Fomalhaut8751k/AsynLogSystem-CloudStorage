#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <thread>
#include <mutex>
#include <future>
#include <condition_variable>
#include <unordered_map>
#include <functional>
#include <queue>

#define INIT_THREADSIZE 4
#define THREAD_SIZE_THRESHHOLD 8
#define TASKQUE_MAX_THRESHHOLD 4

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
    using Task = std::function<void()>;

    ThreadPool()
    {
        setup();
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    tp_uint initThreadSize_;    // 初始线程大小
    tp_uint curThreadSize_;     // 当前线程大小
    tp_uint threadSizeThreshHold_;   // 线程上限大小
    std::unordered_map<int, std::unique_ptr<Thread>> threads_;   // 线程列表

    tp_uint taskSize_;             // 任务数量
    tp_uint taskQueMaxThreshHold_; // 任务上限大小
    std::queue<Task> taskQue_;    // 任务队列

    std::mutex taskQueMtx_;               // 控制队列进出的互斥操作
    std::condition_variable notEmpty_;    // 任务队列非空，线程应该及时取走任务
    std::condition_variable notFull_;     // 任务队列非满，提交的任务可以被接受

    void threadFunc(tp_uint threadid)
    {
        Task task;
        for(;;)
        {
            {
                // 先获取锁
                std::unique_lock<std::mutex> lock(taskQueMtx_);
                
                // 尝试获取任务
                notEmpty_.wait(lock, [&]()->bool{return taskSize_ > 0;});   // 等待任务队列中有任务被提交，就会notify唤醒
                // 确认taskQue_确实不为空
                task = taskQue_.front();  // std::function<void()>;
                taskQue_.pop();
                taskSize_--;

                // 如果发现taskQue_中还有队列，就通知其他人来消费
                if(taskSize_ > 0)
                {
                    notEmpty_.notify_all();
                }     
            }

            if(task != nullptr)
            {
                task();   // 执行任务  std::function<void()>;
            }

        }
    }

public:
    static ThreadPool& GetInstance()
    {
        static ThreadPool threadpool;
        return threadpool;
    }

    void setup(tp_uint initThreadSize = INIT_THREADSIZE,
               tp_uint threadSizeThreshHold =  THREAD_SIZE_THRESHHOLD
    )
    {
        initThreadSize_ = initThreadSize;
        curThreadSize_ = 0;
        threadSizeThreshHold_ = threadSizeThreshHold;

        taskSize_ = 0;
        taskQueMaxThreshHold_ = TASKQUE_MAX_THRESHHOLD;
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
        }
    }

    template<typename Func, typename... Args>
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
    {
        // 打包任务，放入任务队列里面
		using RType = decltype(func(args...));   // 获取任务的返回值
        auto task = std::make_shared<std::packaged_task<RType()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		std::future<RType> result = task->get_future();

        // 先检查任务队列中是否满了，满的话就等待，等待一秒发现还是满的，就提交失败
        {
            std::unique_lock<std::mutex> lock(taskQueMtx_);
            // 如果当前队列已经满了,就等待队列为空，或者超时，如果是因为队列空唤醒的，则可以执行接下来的提交任务操作，否则视为提交失败
            if(!notFull_.wait_for(lock, 
                                  std::chrono::seconds(1),           // 超时
                                  [this]()->bool{ return taskSize_ < taskQueMaxThreshHold_;}) 
            ) 
            {   // 如果是超时返回的，视为提交失败
                std::cerr << "task queue is full, submit task fail." << std::endl;
                // 创建一个简单的任务来获取返回值给到用户，表示任务提交失败
                auto task = std::make_shared<std::packaged_task<RType()>>([]()->RType { return RType(); });
                // 执行任务
                (*task)();
                return task->get_future();
            }
            // 如果是因为taskQue_有空闲位置而被唤醒，则添加到任务队列，并通知线程
            taskQue_.emplace([task]() {(*task)();});
            taskSize_++;
            notEmpty_.notify_all();
            return result;
        }
        
    }

};
#endif