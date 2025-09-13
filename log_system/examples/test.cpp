#include <iostream>
#include <string>
#include <ctime>
#include <fstream>
#include <memory>

/* #### 一个简单的日志系统 #### */


// 日志级别枚举
enum class LogLevel
{
    DEBUG = 1,    // 调试
    INFO,         // 信息
    WARN,         // 警告
    ERROR,        // 一般错误
    FATAL,        // 致命错误
};

// 日志格式化器
class Formatter
{   // 将日志信息格式化为包含时间戳和日志级别的字符串
public:
    std::string format(LogLevel level, const std::string& message)
    {
        std::time_t now = std::time(nullptr);
        std::string time_str = std::ctime(&now);  // "Sun Sep  7 20:16:39 2025\n"
        time_str.pop_back();  // 去掉换行符
        
        std::string level_str = "";
        switch(level)
        {
            case LogLevel::DEBUG: level_str = "DEBUG"; break;
            case LogLevel::INFO : level_str = "INFO" ; break;
            case LogLevel::WARN : level_str = "WARN" ; break;
            case LogLevel::ERROR: level_str = "ERROR"; break;
            case LogLevel::FATAL: level_str = "FATAL"; break;
            default:              level_str = "FATAL";
        }

        return "[" + time_str + "] [" + level_str + "] " + message;
    }
};

// 日志输出器
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


    void debug(const std::string& message)  // 调试信息
    {
        log(LogLevel::DEBUG, message);
    }

    void info(const std::string& message)  // 普通信息
    {
        log(LogLevel::INFO, message);
    }

    void warn(const std::string& message)  // 警告信息
    {
        log(LogLevel::WARN, message);
    }

    void error(const std::string& message)  // 错误信息
    {
        log(LogLevel::ERROR, message);
    }

    void fatal(const std::string& message)  // 致命信息
    {
        log(LogLevel::FATAL, message);
    }

private:
    LogLevel level_;  // 输入日志的最低级别
    std::unique_ptr<Formatter> formatter_;
    std::unique_ptr<Flush> console_Flush_;
    std::unique_ptr<Flush> file_Flush_;
};


int main()
{
    Logger logger;
    logger.info("this is an info message");
    logger.warn("this is an warn message");
    logger.error("this is an error message");
    logger.fatal("this is an fatal message");

    return 0;
}


