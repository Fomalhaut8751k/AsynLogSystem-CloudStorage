#ifndef MESSAGE_H
#define MESSAGE_H
#include "Level.hpp"
#include <string>
#include <ctime>


// 日志格式化器
class LoggerMessage
{   // 将日志信息格式化为包含时间戳和日志级别的字符串
public:
    std::string format(mylog::LogLevel level, const std::string& message)
    {
        std::time_t now = mylog::Util::Date::Now();
        std::string time_str = std::ctime(&now);  // "Sun Sep  7 20:16:39 2025\n"
        time_str.pop_back();  // 去掉换行符
        
        std::string level_str = "";
        switch(level)
        {
            case mylog::LogLevel::DEBUG: level_str = "DEBUG"; break;
            case mylog::LogLevel::INFO : level_str = "INFO" ; break;
            case mylog::LogLevel::WARN : level_str = "WARN" ; break;
            case mylog::LogLevel::ERROR: level_str = "ERROR"; break;
            case mylog::LogLevel::FATAL: level_str = "FATAL"; break;
            default:                     level_str = "FATAL";
        }

        return "[" + time_str + "] [" + level_str + "] " + message;
    }
};

#endif