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
    Glb->BuildLoggerFlush<mylog::FileFlush>("./logfile/File_log.log", 1024 * 1024);

    // 将日志器添加到日志管理者中，管理者是全局单例类 
    mylog::LoggerManager::GetInstance().AddLogger(Glb->Build());

    mylog::LoggerManager::GetInstance().GetLogger("asynclogger")->Info("爰举义旗");

    return 0;
}