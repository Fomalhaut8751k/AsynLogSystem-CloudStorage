#include <iostream>
#include "Service.hpp"
#include "Utils.hpp"

mystorage::DataManager* data_;

int main()
{
    // 初始化并启动线程池 
    std::unique_ptr<mylog::ThreadPool> threadpool_ = std::make_unique<mylog::ThreadPool>();
    threadpool_->setup("127.0.0.1", 8000);
    threadpool_->startup();

    // 初始化启动日志系统并创建日志器
    mylog::LoggerManager::GetInstance().AddDefaultLogger(threadpool_.get());

    // std::shared_ptr<mylog::LoggerBuilder> Glb = std::make_shared<mylog::LoggerBuilder>();
    // Glb->BuildLoggerName("asynclogger");
    // Glb->BuildLoggerFlush<mylog::ConsoleFlush>("", 0);
    // Glb->BuildLoggerThreadPool(threadpool_.get());
    // mylog::LoggerManager::GetInstance().AddLogger(Glb->Build(mylog::LogLevel::DEBUG));
    // mylog::AsyncLogger::n_ptr ptr = mylog::LoggerManager::GetInstance().GetLogger("asynclogger").get();

    // 初始化并启动存储服务器 
    data_ = new mystorage::DataManager();

    std::thread t1([]()->void{
        std::unique_ptr<mystorage::StorageServer> storage_server_  = std::make_unique<mystorage::StorageServer>();
        storage_server_->InitializeConfiguration();
        storage_server_->PowerUp();
    });
    
    t1.join();
    
    delete data_;

    return 0;
}