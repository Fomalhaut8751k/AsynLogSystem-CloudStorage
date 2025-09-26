#ifndef MANAGER_H
#define MANAGER_H

#include "AsyncLogger.hpp" 
#include "ThreadPool.hpp"
#include <string>
#include <unordered_map>

namespace mylog
{
    class LoggerManager
    {  // 全局单例
    private:
        LoggerManager(){}; // 构造函数私有化
        ~LoggerManager()
        {
            // std::cerr << "~LoggerManager()" << std::endl;
        }

        LoggerManager(const LoggerManager&) = delete;  // 禁止拷贝
        LoggerManager& operator=(const LoggerManager&) = delete;  // 禁止等号赋值  

        // 存放日志器
        std::unordered_map<std::string, mylog::AbstractAsyncLoggerPtr> LoggerMap;
        // 控制线程互斥
        std::mutex LoggerManagerMutex_;

    public:
        // 提供唯一的实例
        static LoggerManager& GetInstance()
        {
            static LoggerManager logger_manager_;
            return logger_manager_;
        }

        // 添加默认的日志器
        void AddDefaultLogger(mylog::ThreadPool* thread_pool)
        {
            std::shared_ptr<mylog::LoggerBuilder> Glb = std::make_shared<mylog::LoggerBuilder>();
            Glb->BuildLoggerName("default");
            Glb->BuildLoggerFlush<mylog::FileFlush>("./log/app.log", -1);
            Glb->BuildLoggerThreadPool(thread_pool);
            LoggerMap["default"] = Glb->Build();
        }

        // 添加日志器
        void AddLogger(mylog::AbstractAsyncLoggerPtr async_logger)
        {
            std::unique_lock<std::mutex> lock(LoggerManagerMutex_);
            // 判断一下是否存在
            const std::string logger_name = async_logger->getLoggerName();
            if(LoggerMap.find(logger_name) == LoggerMap.end())
                LoggerMap[logger_name] = async_logger;
        }

        // 用户获取默认日志器
        mylog::AbstractAsyncLoggerPtr DefaultLogger()
        {
            if(LoggerMap.find("default") == LoggerMap.end())
            {
                std::cerr << "default logger is not exist" << std::endl;
                return nullptr;
            }
            return LoggerMap["default"];
        }

        // 用户获取日志器
        mylog::AbstractAsyncLoggerPtr GetLogger(const std::string& name)
        {
            if(LoggerMap.find(name) != LoggerMap.end())
                return LoggerMap[name];    
            std::cerr << "can not find a logger name " << name << std::endl;
            return LoggerMap["default"]; 
        }

        // 查看当前所有的日志器（测试功能）
        void showLogger()
        {
            for(auto item: LoggerMap)
            {
                std::cerr << item.first << " " << item.second << " " << item.second->getLoggerName() << std::endl;
            }
        }

        
    };

}

#endif