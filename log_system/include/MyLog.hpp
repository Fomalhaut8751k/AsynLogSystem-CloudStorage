#ifndef MYLOG_H
#define MYLOG_H

#include "Manager.hpp"
#include "Level.hpp"
#include "LogSystemConfig.hpp"

namespace mylog
{
    // 用户获取日志器
    AsyncLogger::ptr GetLogger(const std::string& name)
    {
        return LoggerManager::GetInstance().GetLogger(name);
    }
}
#endif