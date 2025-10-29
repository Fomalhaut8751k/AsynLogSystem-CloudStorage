#include <iostream>
#include "Service.hpp"
#include "StorageUtils.hpp"


mystorage::DataManager* storage_data_;
std::unique_ptr<mylog::ThreadPool> threadpool_;

int main()
{   
    // std::unique_ptr<mylog::ThreadPool>& threadpool_ = mylog::threadpool_; 

    std::pair<std::string, mylog::LogLevel> log_system_config_message 
                = mylog::Config::GetInstance().ReadConfig();

    // 初始化并启动线程池 
    threadpool_ = std::make_unique<mylog::ThreadPool>();
    threadpool_->setup();

    std::pair<std::string, mylog::LogLevel> threadpool_connected_message = threadpool_->startup();

    // 初始化启动日志系统并创建日志器
    mylog::LoggerManager::GetInstance().AddDefaultLogger(threadpool_.get());
    mylog::GetLogger("default")->Log(log_system_config_message);
    mylog::GetLogger("default")->Log(threadpool_connected_message);

    // 创建新的日志器
    std::shared_ptr<mylog::LoggerBuilder> Glb = std::make_shared<mylog::LoggerBuilder>();
    Glb->BuildLoggerName("asynclogger");
    Glb->BuildLoggerFlush<mylog::ConsoleFlush>("", 0);
    Glb->BuildLoggerThreadPool(threadpool_.get());
    mylog::LoggerManager::GetInstance().AddLogger(Glb->Build(mylog::LogLevel::DEBUG));
    mylog::AsyncLogger::n_ptr ptr = mylog::LoggerManager::GetInstance().GetLogger("asynclogger").get();

    // 初始化并启动存储服务器 

    std::pair<std::string, mylog::LogLevel> storage_config_message = mystorage::Config::GetInstance().ReadConfig();
    mylog::GetLogger("default")->Log(storage_config_message);

    storage_data_ = new mystorage::DataManager();

    std::thread t1([]()->void{
        std::unique_ptr<mystorage::StorageServer> storage_server_  = std::make_unique<mystorage::StorageServer>();
        storage_server_->InitializeConfiguration(threadpool_.get());
        storage_server_->PowerUp();
    });
    
    t1.join();

    delete storage_data_;

    return 0;
}