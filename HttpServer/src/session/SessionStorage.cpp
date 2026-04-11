#include "../../include/session/SessionStorage.h"

namespace http
{

namespace session
{

void MemorySessionStorage::save(std::shared_ptr<Session> session)
{   
    // 创建会话副本并存储
    sessions_[session->getId()] = session;
}

std::shared_ptr<Session> MemorySessionStorage::load(const std::string& sessionId)
{
    auto it = sessions_.find(sessionId);
    // return it != sessions_.end() ? it->second : nullptr;
    if(it != sessions_.end())
    {
        if(!it->second->isExpired())
        {
            return it->second;
        }
        else
        {   // 如果会话过期，则从存储中移除
            sessions_.erase(it);
        }
    }
    return nullptr;
}

// 清除过期的会话
void MemorySessionStorage::removeExpired()
{
    auto it = sessions_.begin();
    for(;it != sessions_.end();)
    {
        if(it->second->isExpired())
        {
            it = sessions_.erase(it);
        }
        else
        {
            ++it;
        }
        
    }
}

// 通过会话Id从存储中移除会话
void MemorySessionStorage::remove(const std::string& sessionId)
{
    sessions_.erase(sessionId);
}


}

}