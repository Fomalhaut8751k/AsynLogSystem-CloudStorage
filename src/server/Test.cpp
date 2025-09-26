#include <iostream>
#include "Service.hpp"
#include "Utils.hpp"

#include "../../../log_system/include/Manager.hpp"

int main()
{
                
    // 启动线程池
    mylog::ThreadPool::GetInstance().setup("127.0.0.1", 8000);
    mylog::ThreadPool::GetInstance().startup();

    // 启动日志系统并创建日志器
    mylog::LoggerManager::GetInstance().AddDefaultLogger(&mylog::ThreadPool::GetInstance());

    std::shared_ptr<mylog::LoggerBuilder> Glb = std::make_shared<mylog::LoggerBuilder>();
    Glb->BuildLoggerName("asynclogger");
    Glb->BuildLoggerFlush<mylog::ConsoleFlush>("", 0);
    Glb->BuildLoggerThreadPool(&mylog::ThreadPool::GetInstance());
    mylog::LoggerManager::GetInstance().AddLogger(Glb->Build(mylog::LogLevel::DEBUG));
    
    // 启动存储服务器
    std::unique_ptr<mylog::StorageServer> storage_server_ = std::make_unique<mylog::StorageServer>();
    
    mylog::LoggerManager::GetInstance().GetLogger("asynclogger")      // 初始化服务器配置信息
                ->Log(storage_server_->InitializeConfiguration());

    mylog::LoggerManager::GetInstance().GetLogger("asynclogger")      // 初始化服务器
                ->Log(storage_server_->InitializeService());

    std::this_thread::sleep_for(std::chrono::seconds(5));

    return 0;
}