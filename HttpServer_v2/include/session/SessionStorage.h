#ifndef HTTPSERVER_V2_SESSIONSTORAGE_H
#define HTTPSERVER_V2_SESSIONSTORAGE_H

#include <memory>
#include <string>
#include <unordered_map>

#include "Session.h"

namespace http_v2
{
namespace session
{

class SessionStorage
{
public:
    virtual ~SessionStorage() = default;
    virtual void save(std::shared_ptr<Session> session) = 0;
    virtual std::shared_ptr<Session> load(const std::string& sessionId) = 0;
    virtual void remove(const std::string& sessionId) = 0;
    virtual void removeExpired() = 0;
};

class MemorySessionStorage: public SessionStorage
{
public:
    void save(std::shared_ptr<Session> session) override;
    std::shared_ptr<Session> load(const std::string& sessionId) override;
    void remove(const std::string& sessionId) override;
    void removeExpired() override;

private:
    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;
};

}
}

#endif
