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
#include "LogFlush.hpp"

#include "MyLog.hpp"

namespace mylog
{
    class AsyncWorker
    {
    private:
        std::mutex Mutex_;

        std::condition_variable cond_productor_;  // 用于生产者的同步通信
        std::condition_variable cond_consumer_;   // 用于消费者的同步通信

        std::shared_ptr<AsyncBuffer> buffer_productor_;
        std::shared_ptr<AsyncBuffer> buffer_consumer_;

        std::atomic_bool label_consumer_ready_;  // 给生产者判断消费者是否处在空闲状态
        std::atomic_bool label_data_ready_;      // 给生产者判断数据是否传入

        mylog::Logger* logger_;  // 日志记录器


    public:
        AsyncWorker(mylog::Logger* logger):
            buffer_productor_(std::make_shared<AsyncBuffer>()),
            buffer_consumer_(std::make_shared<AsyncBuffer>()),

            label_consumer_ready_(true),
            label_data_ready_(true)
        {
            logger_ = logger;
        }

        ~AsyncWorker() = default;

        // 启动
        void start()
        {   
            std::thread productorThread(std::bind(&AsyncWorker::productorTask, this));
            std::cerr << "productor is ready!" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));

            std::thread consumerThread(std::bind(&AsyncWorker::consumerTask, this));
            std::cerr << "consumer is ready!" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // std::thread userThread(std::bind(&AsyncWorker::userTask, this));
            // std::cerr << "user is ready!" << std::endl;

            productorThread.detach();
            consumerThread.detach();
            // userThread.join();
        }

        void productorTask()  // 生产者线程
        {
            for(;;)
            {
                std::unique_lock<std::mutex> lock(Mutex_);
                // 只要满足消费者已就绪，且用户发来消息，才会唤醒生产者
                cond_productor_.wait(lock, [&]()->bool{
                        return label_consumer_ready_ && label_data_ready_;
                    }
                );
                if(!buffer_productor_->read().empty())
                {
                    std::cerr << "Producer has received information!" << std::endl;
                    // 交换生产者和消费者的变量
                    auto tmp_buffer_controler = buffer_productor_;
                    buffer_productor_ = buffer_consumer_;
                    buffer_consumer_ = tmp_buffer_controler;
                    
                    label_data_ready_ = false;
                
                    // 此时消费者的buffer就有了数据
                    cond_consumer_.notify_all();
                }
                else  // 如果醒来发现buffer没有消息，就重新循环，此时
                    // 消费者没有被生产者notify_all()还是等待，不会抢互斥锁
                {

                }
            }
            
        }

        void consumerTask()   // 消费者线程
        {
            // std::this_thread::sleep_for(std::chrono::seconds(12));
            for(;;)
            {
                std::unique_lock<std::mutex> lock(Mutex_);
                label_consumer_ready_ = true;
                cond_productor_.notify_all();  // 通知生产者现在消费者空闲状态
                // 只要生产者一声令下，消费者就干活
                cond_consumer_.wait(lock);

                for(std::string message_formatted: buffer_consumer_->read())
                {
                    logger_->Info(message_formatted);
                }

                buffer_consumer_->clear();
                label_consumer_ready_ = false;
            }
        }

        // 对外提供一个写入的接口
        void readFromUser(char buffer[], unsigned int buffer_length)
        {
            buffer_productor_->write(buffer, buffer_length);
            std::cerr << "read from user" << std::endl;
            label_data_ready_ = true;
            cond_productor_.notify_all();
        }
    };

    using AsyncWorkerPtr = std::shared_ptr<AsyncWorker>;
}
#endif