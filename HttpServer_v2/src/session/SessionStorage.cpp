#include "../../include/session/SessionStorage.h"

namespace http_v2
{
namespace session
{

void MemorySessionStorage::save(std::shared_ptr<Session> session)
{
    sessions_[session->getId()] = std::move(session);
}

std::shared_ptr<Session> MemorySessionStorage::load(const std::string& sessionId)
{
    auto it = sessions_.find(sessionId);
    if(it == sessions_.end()) return nullptr;
    if(it->second->isExpired())
    {
        sessions_.erase(it);
        return nullptr;
    }
    return it->second;
}

void MemorySessionStorage::remove(const std::string& sessionId)
{
    sessions_.erase(sessionId);
}

void MemorySessionStorage::removeExpired()
{
    for(auto it = sessions_.begin(); it != sessions_.end();)
    {
        if(it->second->isExpired()) it = sessions_.erase(it);
        else ++it;
    }
}

}
}
