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
            file_path_ = file_path;
            file_.open(file_path, std::ios::app);
        }  
        
        ~FileFlush() 
        {
            if(file_.is_open())
                file_.close(); 
        };

        void flush(const std::string& formatted_log)
        {
            std::cerr << "有日志被写入: " << file_path_ << "中" << std::endl;
            file_ << formatted_log << std::endl;
        }

    private:
        std::ofstream file_;
        std::string file_path_;
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
            size_ = size;

            // 设置初始的滚动日志文件路径
            init_file_path_root_ = file_path_root;
            init_file_name_without_suffix_ = file_name_without_suffix;

            int index = 1;
    
            for(;;)
            {
                int64_t current_log_size = mylog::Util::File::FileSize(new_file_path);
                if(-1 == current_log_size || size >= current_log_size)  // 说明文件不存在 或者空间充足
                {
                    file_.open(new_file_path, std::ios::app);
                    if(file_.is_open())
                    {
                        current_file_path_ = new_file_path;
                        index_ = index + 1;  // 如果使用过程中写满了，就从这index_开始创建
                        return;
                    } 
                    else
                    {
                        exit(-1);
                    }
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
            /*
                写入前文件的大小 + formatted_log.length() + 1 = 写入后文件的大小
            */
            // 先判断文件是否大小足够：
            // std::cerr << "当前文件: " << current_file_path_ << \
            //             ": (" << mylog::Util::File::FileSize(current_file_path_) << "/" << size_ << ")" << std::endl;
            size_t current_log_size = mylog::Util::File::FileSize(current_file_path_);
            size_t log_size = formatted_log.length() + 1;

            // 视为可以写入
            if(current_log_size + log_size <= size_)  
            {
                std::cerr << "有日志被写入: " << current_file_path_ << "中" << std::endl;
                file_ << formatted_log << std::endl;
            }
            // 空间不足，需要创建新的.log
            else
            {
                file_.close();  // 关闭原来的文件
                std::string new_file_name_without_suffix = init_file_name_without_suffix_ + \
                            "(" + std::to_string(index_++) + ")";
                std::string new_file_path = init_file_path_root_ + new_file_name_without_suffix + ".log"; 


                file_.open(new_file_path, std::ios::app);
                if(!file_.is_open())
                {
                    printf("roll file falied");
                    exit(-1);
                }
                // 当然新的空日志文件限定的大小比如200也写不下太大的字符串
                // 但依然写入，不过下一次就不会写着这里
                current_file_path_ = new_file_path;
                std::cerr << "有日志被写入: " << current_file_path_ << "中" << std::endl;
                file_ << formatted_log << std::endl;
            }

        }

    private:
        std::ofstream file_;
        std::string init_file_path_root_;    // 记录初始的.log的路径
        std::string init_file_name_without_suffix_;  // 记录初始的文件名不含.log

        std::string current_file_path_;      // 当前滚动到的log文件

        int index_;
        size_t size_;
    };

}


#endif