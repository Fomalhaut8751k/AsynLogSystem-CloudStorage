#ifndef MESSAGE_H
#define MESSAGE_H
#include "Level.hpp"
#include <string>
#include <ctime>
#include <cstdio>
#include "LogSystemUtils.hpp"

namespace mylog{

    // 日志格式化器
    class LoggerMessage
    {   // 将日志信息格式化为包含时间戳和日志级别的字符串
    public:
        std::string format(const std::string& unformatted_message, mylog::LogLevel level = mylog::LogLevel::INFO)
        {
            // [优化] thread_local 缓存时间戳：localtime/strftime 开销较大，
            // 同一秒内的日志直接复用上次结果，只在秒数变化时重新格式化。
            // 同时将 std::localtime（非线程安全）改为 localtime_r。
            thread_local time_t last_sec = -1;
            thread_local char time_buf[32] = {};
            std::time_t now = mylog::Util::Date::Now();
            if(now != last_sec){
                struct tm tm_buf;
                localtime_r(&now, &tm_buf);
                std::strftime(time_buf, sizeof(time_buf), "%a %b %d %H:%M:%S %Y", &tm_buf);
                last_sec = now;
            }

            const char* level_str = nullptr;
            switch(level)
            {
                case mylog::LogLevel::DEBUG: level_str = "DEBUG"; break;
                case mylog::LogLevel::INFO : level_str = "INFO" ; break;
                case mylog::LogLevel::WARN : level_str = "WARN" ; break;
                case mylog::LogLevel::ERROR: level_str = "ERROR"; break;
                case mylog::LogLevel::FATAL: level_str = "FATAL"; break;
                default:                     level_str = "FATAL"; break;
            }

            // [优化] 用 snprintf 写到栈上 buf，替换原来的 ostringstream，
            // 避免 ostringstream 内部的动态分配和流操作开销。
            char buf[256];
            int n = snprintf(buf, sizeof(buf), "[%s] [%s] ", time_buf, level_str);
            if(n < 0) n = 0;
            if(n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;

            std::string result;
            result.reserve(n + unformatted_message.size());
            result.append(buf, n);
            result.append(unformatted_message);
            return result;
        }
    };
}


#endif