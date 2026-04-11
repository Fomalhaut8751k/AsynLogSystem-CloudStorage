#ifndef SESSION_H
#define SESSION_H

#include <memory>
#include <string>
#include <unordered_map>
#include <chrono>

namespace http
{

namespace session
{

class SessionManager;

/* std::enable_shared_from_this<>
    对象需要在自己的方法中获取自身的 shared_ptr
    主要用于异步操作、回调函数、链式调用等场景
    确保对象在回调期间保持存活

    在对象的方法中通过 shared_from_this() 返回对象的 shared_ptr 指针
    对象必须由 shared_ptr 管理
*/
class Session: public std::enable_shared_from_this<Session>
{
public:
    Session(const std::string& sessionId, 
            SessionManager* sessionManger, int maxAge=3600); // 默认一小时过期

    const std::string& getId() const { return sessionId_; }

    bool isExpired() const;  // 检查会话是否过期
    void refresh();  // 刷新过期时间

    void setManager(SessionManager* sessionManager) { sessionManager_ = sessionManager; }
    SessionManager* getManager() const { return sessionManager_; }

    // 数据存取
    void setValue(const std::string &key, const std::string &value);
    std::string getValue(const std::string &key) const;
    void remove(const std::string &key);
    void clear();


private:
    std::string sessionId_;
    std::unordered_map<std::string, std::string> data_;
    std::chrono::system_clock::time_point expiryTime_;
    int maxAge_;  // 过期时间(秒)
    SessionManager* sessionManager_;
};

}


}



#endif