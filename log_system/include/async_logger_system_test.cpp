#include "MyLog.hpp"
#include <iostream>
#include <thread>
#include <vector>

// using namespace std;
using namespace mylog;

void threadHandler1(const char* message, const char* logger_name)
{
    mylog::GetLogger(logger_name)->Debug(message);
}

void threadHandler2(const char* message, const char* logger_name)
{
    mylog::GetLogger(logger_name)->Info(message);
}

void threadHandler3(const char* message, const char* logger_name)
{
    mylog::GetLogger(logger_name)->Warn(message);
}

void threadHandler4(const char* message, const char* logger_name)
{
    mylog::GetLogger(logger_name)->Error(message);
}
void threadHandler5(const char* message, const char* logger_name)
{
    mylog::GetLogger(logger_name)->Fatal(message);
}

#if 0
int main()
{
    // 使用默认的日志器（fileflush）
    // mylog::LoggerManager::GetInstance().GetLogger("default")->Info("pdcHelloWorld");

    // 启动线程池
    std::unique_ptr<mylog::ThreadPool> threadpool_ = std::make_unique<mylog::ThreadPool>();
    threadpool_->setup("127.0.0.1", 8000);
    threadpool_->startup();

    mylog::LoggerManager::GetInstance().AddDefaultLogger(threadpool_.get());

    // 使用日志器建造者一个名字叫asynclogger的日志器
    std::shared_ptr<mylog::LoggerBuilder> Glb = std::make_shared<mylog::LoggerBuilder>();
    Glb->BuildLoggerName("asynclogger");
    Glb->BuildLoggerFlush<mylog::FileFlush>("./log/app.log", 100 * 1024);
    Glb->BuildLoggerThreadPool(threadpool_.get());

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
#endif

void threadfunc()
{
    auto start = std::chrono::steady_clock::now();
    bool label = true;
    
    int cnt = 0;
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(2))
    {
        cnt++;
        mylog::GetLogger("asynclogger")->Info("克里斯蒂亚诺罗纳尔多多斯桑托斯阿伟罗先生,五届世界杯淘汰赛0球0助,三次入选世界杯淘汰赛最差阵容");
        if(label && std::chrono::steady_clock::now() - start >= std::chrono::seconds(1))
        {
            label = false;
            std::cout << "1秒内发送了: " << cnt << " 条日志" << std::endl;
        }
    }
    std::cout << "循环结束，运行了1秒\n" << "执行了: " << cnt << "次" << std::endl;

}

#if 1
int main()
{
    std::pair<std::string, mylog::LogLevel> log_system_config_message = mylog::Config::GetInstance().ReadConfig();
    
    // 启动线程池
    std::unique_ptr<mylog::ThreadPool> threadpool_ = std::make_unique<mylog::ThreadPool>();
    threadpool_->setup("127.0.0.1", 8000);
    threadpool_->startup();

    mylog::LoggerManager::GetInstance().AddDefaultLogger(threadpool_.get());

    // 使用日志器建造者一个名字叫asynclogger的日志器
    std::shared_ptr<mylog::LoggerBuilder> Glb = std::make_shared<mylog::LoggerBuilder>();
    Glb->BuildLoggerName("asynclogger");
    Glb->BuildLoggerFlush<mylog::FileFlush>("./log/app.log", 100 * 1024);
    Glb->BuildLoggerThreadPool(threadpool_.get());
    mylog::LoggerManager::GetInstance().AddLogger(Glb->Build(mylog::LogLevel::DEBUG));


    std::thread t1(threadfunc);
    // std::thread t2(threadfunc);

    t1.join();
    // t2.join();

    return 0;
}
#endif