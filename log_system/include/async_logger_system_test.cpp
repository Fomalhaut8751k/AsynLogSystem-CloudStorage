#include "AsyncLogger.hpp"
#include "Manager.hpp"
#include "ThreadPool.hpp"

#include <iostream>
#include <thread>
#include <vector>

// using namespace std;
using namespace mylog;

void threadHandler1(const char* message, const char* logger_name)
{
    mylog::LoggerManager::GetInstance().GetLogger(logger_name)->Debug(message);
}

void threadHandler2(const char* message, const char* logger_name)
{
    mylog::LoggerManager::GetInstance().GetLogger(logger_name)->Info(message);
}

void threadHandler3(const char* message, const char* logger_name)
{
    mylog::LoggerManager::GetInstance().GetLogger(logger_name)->Warn(message);
}

void threadHandler4(const char* message, const char* logger_name)
{
    mylog::LoggerManager::GetInstance().GetLogger(logger_name)->Error(message);
}
void threadHandler5(const char* message, const char* logger_name)
{
    mylog::LoggerManager::GetInstance().GetLogger(logger_name)->Fatal(message);
}

int main()
{
    // 使用默认的日志器（fileflush）
    // mylog::LoggerManager::GetInstance().GetLogger("default")->Info("pdcHelloWorld");

    // 启动线程池
    mylog::ThreadPool::GetInstance().setup("127.0.0.1", 8000);
    mylog::ThreadPool::GetInstance().startup();

    mylog::LoggerManager::GetInstance().AddDefaultLogger(&mylog::ThreadPool::GetInstance());

    // 使用日志器建造者一个名字叫asynclogger的日志器
    std::shared_ptr<mylog::LoggerBuilder> Glb = std::make_shared<mylog::LoggerBuilder>();
    Glb->BuildLoggerName("asynclogger");
    Glb->BuildLoggerFlush<mylog::FileFlush>("./log/app.log", 700);
    Glb->BuildLoggerThreadPool(&mylog::ThreadPool::GetInstance());

    // 将日志器添加到日志管理者中，管理者是全局单例类 
    mylog::LoggerManager::GetInstance().AddLogger(Glb->Build(mylog::LogLevel::DEBUG));   // 小于Warn的Debug和Info不会被写到日志当中
    // Glb->Build();   
    // mylog::LoggerManager::GetInstance().GetLogger("asynclogger")->Debug("梦苻坚将天官使者，鬼兵数百突入营中");

    // // std::this_thread::sleep_for(std::chrono::seconds(2));
    // mylog::LoggerManager::GetInstance().GetLogger("asynclogger")->Info("苌俱，走入宫");

    // // std::this_thread::sleep_for(std::chrono::seconds(2));
    // mylog::LoggerManager::GetInstance().GetLogger("asynclogger")->Warn("宫人迎苌刺鬼，误中苌阴");

    // // std::this_thread::sleep_for(std::chrono::seconds(2));
    // mylog::LoggerManager::GetInstance().GetLogger("asynclogger")->Error("鬼相谓曰：正中死处");

    // // std::this_thread::sleep_for(std::chrono::seconds(2));
    // mylog::LoggerManager::GetInstance().GetLogger("asynclogger")->Fatal("拔矛，出血石余");

    // // std::this_thread::sleep_for(std::chrono::seconds(2));
    // mylog::LoggerManager::GetInstance().GetLogger("asynclogger")->Info("寐而惊悸，遂患阴肿");

    // // std::this_thread::sleep_for(std::chrono::seconds(2));
    // mylog::LoggerManager::GetInstance().GetLogger("asynclogger")->Warn("医刺之，出血如梦");

    std::vector<std::thread> threads_;
    threads_.push_back(std::thread(threadHandler1, "梦苻坚将天官使者，鬼兵数百突入营中", "asynclogger"));
    threads_.push_back(std::thread(threadHandler5, "苌俱，走入宫", "asynclogger"));
    threads_.push_back(std::thread(threadHandler5, "宫人迎苌刺鬼，误中苌阴", "default"));
    threads_.push_back(std::thread(threadHandler2, "鬼相谓曰：正中死处", "asynclogger"));
    threads_.push_back(std::thread(threadHandler4, "拔矛，出血石余", "default"));
    threads_.push_back(std::thread(threadHandler3, "寐而惊悸，遂患阴肿", "default"));
    threads_.push_back(std::thread(threadHandler4, "医刺之，出血如梦", "asynclogger"));
    threads_.push_back(std::thread(threadHandler1, "梦苻坚将天官使者，鬼兵数百突入营中", "asynclogger"));
    threads_.push_back(std::thread(threadHandler5, "苌俱，走入宫", "asynclogger"));
    threads_.push_back(std::thread(threadHandler5, "宫人迎苌刺鬼，误中苌阴", "default"));
    threads_.push_back(std::thread(threadHandler5, "鬼相谓曰：正中死处", "asynclogger"));
    threads_.push_back(std::thread(threadHandler5, "拔矛，出血石余", "default"));
    threads_.push_back(std::thread(threadHandler5, "寐而惊悸，遂患阴肿", "default"));
    threads_.push_back(std::thread(threadHandler5, "医刺之，出血如梦", "asynclogger"));

    for(std::thread& t: threads_)
    {
        t.join();
    }

    std::this_thread::sleep_for(std::chrono::seconds(6));  // 冷机
    // std::cerr << "main() over" << std::endl;
    return 0;
}