#ifndef ASYNCLOGGER_H
#define ASYNCLOGGER_H
/*
    AsyncLogger 把组织好的日志放到（生产者的）缓存区上
    "所有的操作将在这里被调用"

    肯定有个函数，接受组织好的日志作为参数
*/
#include "Worker.hpp"
#include "AsyncWorker.hpp"
#include "AsyncWorkerWithLogQueue.hpp"
#include "AsyncWorkerWithLogDeque.hpp"

#include "LogSystemUtils.hpp"
#include "Flush.hpp"
#include "backlog/ClientBackupLog.hpp"
#include "ThreadPool.hpp"

#define LOGGERTYPE 2   // 0, 1, 2, 3

// 抽象的产品(异步日志器)类
class AbstractAsyncLogger
{
protected:
    mylog::ThreadPool* threadpool_;                // 线程池指针，用于调用submitLog  
    mylog::LogLevel level_;                       // 输入日志的最低级别

    std::string logger_name_;                     // 日志器的名称
    std::shared_ptr<mylog::Flush> flush_;         // 日志输出器     
    std::shared_ptr<mylog::Flush> flush_default_;

    // 应该先创建Flush，再创建AsyncWorker，这样才能先析构AsyncWorker，再析构Flush

#if LOGGERTYPE == 0
    std::shared_ptr<mylog::Worker> worker_;
#elif LOGGERTYPE == 1
    std::shared_ptr<mylog::AsyncWorker> worker_;
#elif LOGGERTYPE == 2
    std::shared_ptr<mylog::AsyncWorkerWithLogQueue> worker_;
#elif LOGGERTYPE == 3
    std::shared_ptr<mylog::AsyncWorkerWithLogDeque> worker_;
#endif

    std::unique_ptr<mylog::LoggerMessage> formatter_;

public:
    AbstractAsyncLogger(): threadpool_(nullptr),formatter_(std::make_unique<mylog::LoggerMessage>()){}
    virtual ~AbstractAsyncLogger() = default;

    virtual void setLevel(mylog::LogLevel) = 0;
    virtual void setAsyncWorker() = 0;

    // 设置异步日志器的名称，可以通过名称从管理者那调用对应的日志器使用
    void setLoggerName(std::string logger_name){ logger_name_ = logger_name; }

    // 设置不同类型(ConsoleFlush, FileFlush, RollFileFlush)的日志提交方式
    template<typename FlushType>
    void setLogFlush(const std::string& logpath, size_t size) 
    { 
        flush_ = std::make_shared<FlushType>(logpath, size); 
        flush_default_ = std::make_shared<mylog::ConsoleFlush>();
    }

    // 接受线程池的指针，用于调用submitLog推送日志到服务器
    void setThreadPool(mylog::ThreadPool* thread_pool)
    {
        threadpool_ = thread_pool;
    }

    // 提交日志
    void log(std::string formatted_message)
    {   
        if(flush_){
            flush_->flush(formatted_message);
        }
        else{
            std::cerr << "flush_ is nullptr" << std::endl;
        }
    }

    // 将消息处理为Debug类型的日志并发送
    void Debug(const std::string& unformatted_message, const char* FILE = __FILE__, int LINE = __LINE__)  // 调试信息
    {
        if(unformatted_message.length() > 64 * 1024 * 1024){
            std::string formatted_message = formatter_->format("A massive log was examined and filtered", mylog::LogLevel::WARN);
            flush_default_->flush(formatted_message);
            return;
        }
        std::string formatted_message = formatter_->format(unformatted_message, mylog::LogLevel::DEBUG);  // message.hpp
        const int formatted_message_length = formatted_message.length();

        // 如果消息等级过低，就不写入缓冲区
        if(level_ > mylog::LogLevel::DEBUG)
        {
            return;
        }
        char buf[256];
        snprintf(buf, sizeof(buf), ", __FILE__: %s, __LINE__: %d", FILE, LINE);  // 添加相应的调试信息
        std::string final_message = formatted_message + buf;

        // 把最终信息写到worker_的buffer当中
        // worker_->readFromUser(final_message, final_message.length());
        worker_->LogQueueForward(final_message);
        
    }

    // 将消息处理为Info类型的日志并发送
    void Info(const std::string& unformatted_message)  // 普通信息
    {
        if(unformatted_message.length() > 64 * 1024 * 1024){
            std::string formatted_message = formatter_->format("A massive log was examined and filtered", mylog::LogLevel::WARN);
            flush_default_->flush(formatted_message);
            return;
        }
        // 如果消息等级过低，就不写入缓冲区
        if(level_ > mylog::LogLevel::INFO) return;
        // 减少临时对象的创建
        worker_->LogQueueForward(formatter_->format(unformatted_message, mylog::LogLevel::INFO));
    }

    // 将消息处理为Warn类型的日志并发送
    void Warn(const std::string& unformatted_message)  // 警告信息
    {
        if(unformatted_message.length() > 64 * 1024 * 1024){
            std::string formatted_message = formatter_->format("A massive log was examined and filtered", mylog::LogLevel::WARN);
            flush_default_->flush(formatted_message);
            return;
        }
        std::string formatted_message = formatter_->format(unformatted_message, mylog::LogLevel::WARN);  // message.hpp
        unsigned int formatted_message_length = formatted_message.length();

        // 如果消息等级过低，就不写入缓冲区
        if(level_ > mylog::LogLevel::WARN)
        {
            return;
        }

        // 把信息写到worker_的buffer当中
        // worker_->readFromUser(formatted_message, formatted_message_length);
        worker_->LogQueueForward(formatted_message);
    }

    // 将消息处理为Error类型的日志并发送
    void Error(const std::string& unformatted_message)  // 错误信息
    {
        if(unformatted_message.length() > 64 * 1024 * 1024){
            std::string formatted_message = formatter_->format("A massive log was examined and filtered", mylog::LogLevel::WARN);
            flush_default_->flush(formatted_message);
            return;
        }
        std::string formatted_message = formatter_->format(unformatted_message, mylog::LogLevel::ERROR);  // message.hpp
        unsigned int formatted_message_length = formatted_message.length();

        // 如果消息等级过低，就不写入缓冲区
        if(level_ > mylog::LogLevel::ERROR)
        {
            return;
        }

        if(threadpool_)
        {
            threadpool_->submitLog(formatted_message);
        }

        // 把信息写到worker_的buffer当中
        // worker_->readFromUser(formatted_message, formatted_message_length);
        worker_->LogQueueForward(formatted_message);
    }

    // 将消息处理为Fatal类型的日志并发送
    void Fatal(const std::string& unformatted_message)  // 致命信息
    {
        if(unformatted_message.length() > 64 * 1024 * 1024){
            std::string formatted_message = formatter_->format("A massive log was examined and filtered", mylog::LogLevel::WARN);
            flush_default_->flush(formatted_message);
            return;
        }
        std::string formatted_message = formatter_->format(unformatted_message, mylog::LogLevel::FATAL);  // message.hpp
        unsigned int formatted_message_length = formatted_message.length();

        // 如果消息等级过低，就不写入缓冲区
        if(level_ > mylog::LogLevel::FATAL)
        {
            return;
        }

        if(threadpool_)
        {
            threadpool_->submitLog(formatted_message);
        }
        // 把信息写到worker_的buffer当中
        // worker_->readFromUser(formatted_message, formatted_message_length);
        worker_->LogQueueForward(formatted_message);
    }

    void WarnDefault(const std::string& unformatted_message)  // 警告信息
    {
        // 先格式化日志信息，用于反馈关键信息，直接输出到控制台
        std::string formatted_message = formatter_->format(unformatted_message, mylog::LogLevel::WARN);  // message.hpp
        std::cerr << formatted_message << std::endl;
    }

    void Log(std::pair<std::string, mylog::LogLevel> log_message)
    {
        std::string unformatted_message = log_message.first;
        mylog::LogLevel log_level = log_message.second;

        if(log_level == mylog::LogLevel::DEBUG) { Debug(unformatted_message); }
        else if(log_level == mylog::LogLevel::INFO) { Info(unformatted_message); }
        else if(log_level == mylog::LogLevel::WARN) { Warn(unformatted_message); }
        else if(log_level == mylog::LogLevel::ERROR) { Error(unformatted_message); }
        else if(log_level == mylog::LogLevel::FATAL) { Fatal(unformatted_message); }
        else {}
    }

    std::string getLoggerName() const { return logger_name_; }
};


// 抽象的日志器建造类
class AbstractLoggerBuilder
{
protected:
    std::shared_ptr<AbstractAsyncLogger> async_logger_;  // 抽象基类指针

public:
    virtual void BuildLoggerName(const std::string& loggername) = 0;
    // virtual void BuildLoggerFlush()
    virtual std::shared_ptr<AbstractAsyncLogger> Build(mylog::LogLevel) = 0;
    virtual void BuildLoggerThreadPool(mylog::ThreadPool*) = 0;
};


namespace mylog
{
    // 具体的产品(异步日志器)类
    class AsyncLogger: public AbstractAsyncLogger
    {
    private:

    public:
        using ptr = std::shared_ptr<AbstractAsyncLogger>;
        using n_ptr = AbstractAsyncLogger*;

        AsyncLogger():AbstractAsyncLogger(){}
        ~AsyncLogger()
        {
            // std::cerr << "~AsyncLogger()" << std::endl;
        }

        void setLevel(mylog::LogLevel level)
        {
            level_ = level;
        }

        void setAsyncWorker()
        {
#if LOGGERTYPE == 0
            worker_ = std::make_shared<mylog::Worker>(
                std::bind(&AsyncLogger::log, this, std::placeholders::_1),
                std::bind(&AsyncLogger::WarnDefault, this, std::placeholders::_1)
            );
#elif LOGGERTYPE == 1
            让worker的消费者线程可以将日志发送到指定为止
            worker_ = std::make_shared<mylog::AsyncWorker>(
                std::bind(&AsyncLogger::log, this, std::placeholders::_1),
                std::bind(&AsyncLogger::WarnDefault, this, std::placeholders::_1)
            );
#elif LOGGERTYPE == 2
            worker_ = std::make_shared<mylog::AsyncWorkerWithLogQueue>(
                std::bind(&AsyncLogger::log, this, std::placeholders::_1),
                std::bind(&AsyncLogger::WarnDefault, this, std::placeholders::_1)
            );
#elif LOGGERTYPE == 3
            worker_ = std::make_shared<mylog::AsyncWorkerWithLogDeque>(
                std::bind(&AsyncLogger::log, this, std::placeholders::_1),
                std::bind(&AsyncLogger::WarnDefault, this, std::placeholders::_1)
            );
#endif         
            worker_->start();
        }
    };

    // 具体的日志器建造者类
    class LoggerBuilder: public AbstractLoggerBuilder
    {
    public:
        LoggerBuilder()
        {
            async_logger_ = std::dynamic_pointer_cast<mylog::AsyncLogger>(std::make_shared<mylog::AsyncLogger>());
        }

        ~LoggerBuilder() = default;

        void BuildLoggerName(const std::string& loggername)
        {
            async_logger_->setLoggerName(loggername);
        }

        template<typename FlushType>  // T: mylog::Flush flush
        void BuildLoggerFlush(const std::string& logpath, size_t size)
        {
            async_logger_->setLogFlush<FlushType>(logpath, size);
        }

        void BuildLoggerThreadPool(mylog::ThreadPool* thread_pool_)
        {
            async_logger_->setThreadPool(thread_pool_);
        }

        AsyncLogger::ptr Build(mylog::LogLevel level = mylog::LogLevel::INFO)
        {
            async_logger_->setLevel(level);
            async_logger_->setAsyncWorker();

            return async_logger_;
        }
    };
}

#endif