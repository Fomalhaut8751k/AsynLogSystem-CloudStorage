#include "AsyncLogger.hpp"
#include "Manager.hpp"
#include <iostream>

// using namespace std;
using namespace mylog;

int main()
{
    // 使用日志器建造者一个名字叫asynclogger的日志器
    std::shared_ptr<mylog::LoggerBuilder> Glb = std::make_shared<mylog::LoggerBuilder>();
    Glb->BuildLoggerName("asynclogger");
    Glb->BuildLoggerFlush<mylog::RollFileFlush>("./log/app1.log", 100);

    // 将日志器添加到日志管理者中，管理者是全局单例类 
    mylog::LoggerManager::GetInstance().AddLogger(Glb->Build());

    // 调用GetLogger写入日志
    mylog::LoggerManager::GetInstance().GetLogger("asynclogger")->Info("克里斯蒂亚诺罗纳尔多多斯桑托斯阿伟罗");

    return 0;
}