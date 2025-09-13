#ifndef LOGFLUSH_H
#define LOGFLUSH_H

#include "Level.hpp"
#include "Util.hpp"
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
        ConsoleFlush(std::string file_name = " ", size_t size = 1)
        {

        }

        ~ConsoleFlush() = default;

        void flush(const std::string& formatted_log)
        {
            std::cerr << formatted_log << std::endl;
        }
    };

    // 文件日志输出器
    class FileFlush: public Flush
    {   // 将日志信息输出到文件
    public:
        FileFlush(std::string file_path, size_t size = 1)
        {
            
        }  
        
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
        RollFileFlush(std::string file_path, size_t size)
        {
            // 获取文件名字
            std::pair<std::string, std::string> file_item = mylog::Util::File::Path(file_path);
            if(file_item.second == "")
            {
                printf("log file is not invalid");
                exit(-1);
            }

            std::string file_path_root = file_item.first;
            std::string file_name = file_item.second;

            int pos = file_name.rfind(".");
            if(file_name.substr(pos+1) != "log")
            {
                printf("log file is not invalid");
                exit(-1);
            }


            // file.open()创建文件的前提是前级的目录都存在
            mylog::Util::File::CreateDirectory(file_path_root);

            
            std::string file_name_without_suffix = file_name.substr(0, pos);

            std::string new_file_path = file_path;

            int index = 1;
    
            for(;;)
            {
                int64_t current_log_size = mylog::Util::File::FileSize(new_file_path);
                if(-1 == current_log_size || size >= current_log_size)  // 说明文件不存在 或者空间充足
                {
                    file_.open(new_file_path, std::ios::app);
                    if(file_.is_open())
                        return;
                    else
                        exit(-1);
                }
                else  // 空间不足     
                {
                    new_file_path = "";
                    new_file_path += file_path_root;
                    new_file_path += file_name_without_suffix;
                    new_file_path += "(" + std::to_string(index++) + ")";
                    new_file_path += ".log";
                }
            }
            
        }

        ~RollFileFlush() 
        {
            if(file_.is_open())
                file_.close(); 
        }

        void flush(const std::string& formatted_log)
        {
            file_ << formatted_log << std::endl;
        }

    private:
        std::ofstream file_;
    };


    // 日志记录器
    class Logger
    {
    public:
        Logger(mylog::Flush* flush):
            formatter_(std::make_unique<LoggerMessage>()),
            flush_(flush),
            level_(mylog::LogLevel::DEBUG){}

        void log(mylog::LogLevel level, const std::string& message)
        {
            // 如果小于输出的最低级别就不输出
            if(level >= level_)
            {
                // 先格式化
                std::string formatted_log = formatter_->format(level, message);
                // 输出
                flush_->flush(formatted_log);
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
        LogLevel level_;        // 输入日志的最低级别
        mylog::Flush* flush_;   // 日志输出器
        std::unique_ptr<LoggerMessage> formatter_;
    };

}


#endif