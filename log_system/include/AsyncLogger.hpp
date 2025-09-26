#ifndef ASYNCLOGGER_H
#define ASYNCLOGGER_H
/*
    AsyncLogger 把组织好的日志放到（生产者的）缓存区上
    "所有的操作将在这里被调用"

    肯定有个函数，接受组织好的日志作为参数
*/
#include "AsyncWorker.hpp"
#include "Util.hpp"
#include "Flush.hpp"
#include "backlog/ClientBackupLog.hpp"
#include "ThreadPool.hpp"

// 抽象的产品(异步日志器)类
class AbstractAsyncLogger
{
protected:
    std::shared_ptr<mylog::AsyncWorker> worker_;
    std::unique_ptr<mylog::LoggerMessage> formatter_;

    mylog::LogLevel level_;                       // 输入日志的最低级别

    std::string logger_name_;                     // 日志器的名称
    std::shared_ptr<mylog::Flush> flush_;         // 日志输出器     

    mylog::ThreadPool* threadpool_;                // 线程池指针，用于调用submitLog  

public:
    AbstractAsyncLogger():threadpool_(nullptr){}
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
        // std::cerr << "typename: " << typeid(FlushType).name() << std::endl;
    }

    // 接受线程池的指针，用于调用submitLog推送日志到服务器
    void setThreadPool(mylog::ThreadPool* thread_pool)
    {
        threadpool_ = thread_pool;
    }

    // 提交日志
    void log(std::string formatted_message)
    {
        flush_->flush(formatted_message);
    }

    // 将消息处理为Debug类型的日志并发送
    void Debug(const std::string& unformatted_message)  // 调试信息
    {
        // 先格式化日志信息
        std::string formatted_message = formatter_->format(unformatted_message, mylog::LogLevel::DEBUG);  // message.hpp
        unsigned int formatted_message_length = formatted_message.length();

        // 如果消息等级过低，就不写入缓冲区
        if(level_ > mylog::LogLevel::DEBUG)
        {
            return;
        }

        // 把信息写到worker_的buffer当中
        worker_->readFromUser(formatted_message, formatted_message_length);
        
    }

    // 将消息处理为Info类型的日志并发送
    void Info(const std::string& unformatted_message)  // 普通信息
    {
        // 先格式化日志信息
        std::string formatted_message = formatter_->format(unformatted_message, mylog::LogLevel::INFO);  // message.hpp
        unsigned int formatted_message_length = formatted_message.length();

        // 如果消息等级过低，就不写入缓冲区
        if(level_ > mylog::LogLevel::INFO)
        {
            return;
        }

        // 把信息写到worker_的buffer当中
        worker_->readFromUser(formatted_message, formatted_message_length);
    }

    // 将消息处理为Warn类型的日志并发送
    void Warn(const std::string& unformatted_message)  // 警告信息
    {
        // 先格式化日志信息
        std::string formatted_message = formatter_->format(unformatted_message, mylog::LogLevel::WARN);  // message.hpp
        unsigned int formatted_message_length = formatted_message.length();

        // 如果消息等级过低，就不写入缓冲区
        if(level_ > mylog::LogLevel::WARN)
        {
            return;
        }

        // 把信息写到worker_的buffer当中
        worker_->readFromUser(formatted_message, formatted_message_length);
    }

    // 将消息处理为Error类型的日志并发送
    void Error(const std::string& unformatted_message)  // 错误信息
    {
        // 先格式化日志信息
        std::string formatted_message = formatter_->format(unformatted_message, mylog::LogLevel::ERROR);  // message.hpp
        unsigned int formatted_message_length = formatted_message.length();

        // 如果消息等级过低，就不写入缓冲区
        if(level_ > mylog::LogLevel::ERROR)
        {
            return;
        }

        // 把error级别的日志发送到服务器中,确保先连接上了
        if(threadpool_->Connected())
        {
            // std::cerr << "=> 日志消息 " << formatted_message << "将被发送至服务器" << std::endl;
            threadpool_->submitLog(formatted_message);
        }  

        // 把信息写到worker_的buffer当中
        worker_->readFromUser(formatted_message, formatted_message_length);
    }

    // 将消息处理为Fatal类型的日志并发送
    void Fatal(const std::string& unformatted_message)  // 致命信息
    {
        // 先格式化日志信息
        std::string formatted_message = formatter_->format(unformatted_message, mylog::LogLevel::FATAL);  // message.hpp
        unsigned int formatted_message_length = formatted_message.length();

        // 如果消息等级过低，就不写入缓冲区
        if(level_ > mylog::LogLevel::FATAL)
        {
            return;
        }

        // 把fatal级别的日志发送到服务器中,确保先连接上了
        if(threadpool_->Connected())
        {
            // std::cerr << "=> 日志消息 " << formatted_message << "将被发送至服务器" << std::endl;
            threadpool_->submitLog(formatted_message);
        }  

        // 把信息写到worker_的buffer当中
        worker_->readFromUser(formatted_message, formatted_message_length);
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
    using AbstractAsyncLoggerPtr = std::shared_ptr<AbstractAsyncLogger>;

    // 具体的产品(异步日志器)类
    class AsyncLogger: public AbstractAsyncLogger
    {
    private:

    public:
        using ptr = std::shared_ptr<AsyncLogger>;

        AsyncLogger():AbstractAsyncLogger(){}
        ~AsyncLogger()
        {
            
        }

        void setLevel(mylog::LogLevel level)
        {
            level_ = level;
        }

        void setAsyncWorker()
        {
            // 让worker的消费者线程可以将日志发送到指定为止
            worker_ = std::make_shared<mylog::AsyncWorker>(std::bind(&AsyncLogger::log, this, std::placeholders::_1));
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

        AbstractAsyncLoggerPtr Build(mylog::LogLevel level = mylog::LogLevel::INFO)
        {
            async_logger_->setLevel(level);
            async_logger_->setAsyncWorker();

            return async_logger_;
        }
    };
}

#endif