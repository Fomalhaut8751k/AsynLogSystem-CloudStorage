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

// 抽象的产品(异步日志器)类
class AbstractAsyncLogger
{
protected:
    std::shared_ptr<mylog::AsyncWorker> worker_;
    std::unique_ptr<mylog::LoggerMessage> formatter_;

    mylog::LogLevel level_;                       // 输入日志的最低级别

    std::string logger_name_;                     // 日志器的名称
    std::shared_ptr<mylog::Flush> flush_;         // 日志输出器       

public:
    virtual void setLevel(mylog::LogLevel) = 0;
    virtual void setAsyncWorker() = 0;

    void setLoggerName(std::string logger_name){ logger_name_ = logger_name; }

    template<typename FlushType>
    void setLogFlush(const std::string& logpath, size_t size) 
    { 
        flush_ = std::make_shared<FlushType>(logpath, size); 
        std::cerr << "typename: " << typeid(FlushType).name() << std::endl;
    }

    void log(std::string formatted_message)
    {
        flush_->flush(formatted_message);
    }

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

        // 把信息写到worker_的buffer当中
        worker_->readFromUser(formatted_message, formatted_message_length);
    }

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

        // 把信息写到worker_的buffer当中
        worker_->readFromUser(formatted_message, formatted_message_length);
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

        AbstractAsyncLoggerPtr Build(mylog::LogLevel level = mylog::LogLevel::INFO)
        {
            async_logger_->setLevel(level);
            async_logger_->setAsyncWorker();

            return async_logger_;
        }
    };
}

#endif