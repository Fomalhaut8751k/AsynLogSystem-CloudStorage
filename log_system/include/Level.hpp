#ifndef LEVEL_H
#define LEVEL_H

namespace mylog
{
    // 日志级别枚举
    enum class LogLevel : int
    {
        DEBUG = 1,    // 调试
        INFO,         // 信息
        WARN,         // 警告
        ERROR,        // 一般错误
        FATAL,        // 致命错误
    };
}
#endif