#ifndef SESSIONSTORAGE_H
#define SESSIONSTORAGE_H

#include "Session.h"
#include <memory>

namespace http
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
    /* override: 显式声明一个函数是覆盖（重写）基类的虚函数 */
    virtual void save(std::shared_ptr<Session> session) override;  // 创建会话副本并存储
    virtual std::shared_ptr<Session> load(const std::string& sessionId) override;
    virtual void remove(const std::string& sessionId) override;  // 通过会话Id从存储中移除会话

    virtual void removeExpired() override;  // 清除过期的会话
private:
    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;
};

}


}


#endif