#ifndef LOGFLUSH_H
#define LOGFLUSH_H

#include "Level.hpp"
#include "Message.hpp"
#include <iostream>
#include <string>
#include <fstream>

// 日志输出器
namespace mylog
{
    class Flush
    {   // 抽象基类，定义类日志输出的接口
    public:
        virtual void flush(const std::string& formatted_log) = 0;
        virtual ~Flush() = default;
    };

    // 控制台日志输出器
    class ConsoleFlush: public Flush
    {   // 将日志信息输出到控制台
    public:
        void flush(const std::string& formatted_log)
        {
            std::cerr << formatted_log << std::endl;
        }
    };

    // 文件日志输出器
    class FileFlush: public Flush
    {   // 将日志信息输出到文件
    public:
        FileFlush(std::string file_name): file_(file_name, std::ios::app){}  // 以追加的方式写入  
        
        ~FileFlush() 
        {
            if(file_.is_open())
                file_.close(); 
        };

        void flush(const std::string& formatted_log)
        {
            file_ << formatted_log << std::endl;
        }

    private:
        std::ofstream file_;
    };

    // 滚动日志输出器
    class RollFileFlush: public Flush
    {
        // 将日志输出到文件，并按照大小生产日志
    public:
        RollFileFlush(std::string file_name){};
    };


    // 日志记录器
    class Logger
    {
    public:
        Logger(const std::string file_name = "app.log"):
            formatter_(std::make_unique<Formatter>()),
            console_Flush_(std::make_unique<ConsoleFlush>()),
            file_Flush_(std::make_unique<FileFlush>(file_name)){}

        void log(LogLevel level, const std::string& message)
        {
            // 如果小于输出的最低级别就不输出
            if(level >= level_)
            {
                // 先格式化
                std::string formatted_log = formatter_->format(level, message);
                // 输出到控制台
                console_Flush_->flush(formatted_log);
                // 输出到日志文件
                file_Flush_->flush(formatted_log);
            }
        }

        void Debug(const std::string& message)  // 调试信息
        {
            log(LogLevel::DEBUG, message);
        }

        void Info(const std::string& message)  // 普通信息
        {
            log(LogLevel::INFO, message);
        }

        void Warn(const std::string& message)  // 警告信息
        {
            log(LogLevel::WARN, message);
        }

        void Error(const std::string& message)  // 错误信息
        {
            log(LogLevel::ERROR, message);
        }

        void Fatal(const std::string& message)  // 致命信息
        {
            log(LogLevel::FATAL, message);
        }

    private:
        LogLevel level_;  // 输入日志的最低级别
        std::unique_ptr<Formatter> formatter_;
        std::unique_ptr<Flush> console_Flush_;
        std::unique_ptr<Flush> file_Flush_;
    };

}


#endif