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
    std::shared_ptr<mylog::Logger> logger_;
    
    struct event_base* base_;
    struct evconnlistener* listener_;

    std::string logger_name_;                     // 日志器的名称
    std::shared_ptr<mylog::Flush> flush_;         // 日志输出器       

public:
    virtual void setLogger() = 0;
    virtual void setAsyncWorker() = 0;

    void setLoggerName(std::string logger_name){ logger_name_ = logger_name; }

    template<typename FlushType>
    void setLogFlush(const std::string& logpath, size_t size) 
    { 
        flush_ = std::make_shared<FlushType>(logpath, size); 
        std::cerr << "typename: " << typeid(FlushType).name() << std::endl;
    }

    std::string getLoggerName() const { return logger_name_; }
    mylog::Logger* getLogger() const { return logger_.get(); }
};


// 抽象的日志器建造类
class AbstractLoggerBuilder
{
protected:
    std::shared_ptr<AbstractAsyncLogger> async_logger_;  // 抽象基类指针

public:
    virtual void BuildLoggerName(const std::string& loggername) = 0;
    // virtual void BuildLoggerFlush()
    virtual std::shared_ptr<AbstractAsyncLogger> Build() = 0;
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

        void setLogger()
        {
            logger_ = std::make_shared<mylog::Logger>(flush_.get());
        }

        void setAsyncWorker()
        {
            worker_ = std::make_shared<mylog::AsyncWorker>(logger_.get());
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

        AbstractAsyncLoggerPtr Build()
        {
            async_logger_->setLogger();
            async_logger_->setAsyncWorker();

            return async_logger_;
        }
    };
}

#endif