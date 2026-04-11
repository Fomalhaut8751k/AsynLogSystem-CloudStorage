#include "../../include/session/SessionManager.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <cassert>

namespace http
{

namespace session
{

// 初始化会话管理器，设置会话存储对象和随机数生成器
SessionManager::SessionManager(std::unique_ptr<SessionStorage> storage):
    storage_(std::move(storage)),
    rng_(std::random_device{}())  // 初始化随机数生成器，用于生成一个随机的会话ID
{

}

// 从请求中获取或创建会话
std::shared_ptr<Session> SessionManager::getSession(const HttpRequest& req, HttpResponse* resp)
{
    // 从req中获取sessionId，但是创建会话前没有这个Id
    std::string sessionId = getSessionIdFromCookie(req);
    std::shared_ptr<Session> session;

    /*
        sessionId为空————没有会话————(创建会话)
        sessionId不为空————有会话————先加载会话
            会话过期了————移除过期会话并创建新的会话
            会话没过期————直接用这个会话，并且刷新一下时间
    */
   
    if(sessionId.empty())
    {
        sessionId = generateSessionId();  // 创建一个新的Id
        session = std::make_shared<Session>(sessionId, this);
        setSessionCookie(sessionId, resp);
        storage_->save(session);
    }
    else
    {
        session = storage_->load(sessionId);
        // if(session->isExpired())  // 如果过期了
        if(!session)
        {
            storage_->remove(sessionId);
            sessionId = generateSessionId();  // 创建一个新的Id
            session = std::make_shared<Session>(sessionId, this);
            setSessionCookie(sessionId, resp);
            storage_->save(session);
        }    
        else  // 没有过期
        {
            session->setManager(this);  // 为现有会话设置管理器
        }
    }
    session->refresh();
    return session;
    
}

// 销毁会话
void SessionManager::destorySession(const std::string& sessionId)
{
    storage_->remove(sessionId);
}

// 清理过期会话
void SessionManager::cleanExpiredSession()
{
    /*
        注意：这个实现依赖于具体的存储实现
        对于内存存储，可以在加载时检查是否过期
        对于其他存储的实现，可能需要定期清理过期会话
    */
   storage_->removeExpired();
}

// 更新会话
void SessionManager::updateSession(std::shared_ptr<Session> session)
{
    storage_->save(session);
}

// 生产唯一的会话标识符，确保会话的唯一性和安全性
std::string SessionManager::generateSessionId()
{
    std::stringstream ss;
    std::uniform_int_distribution<> dist(0, 15);  // dist生成的随机整数范围为0-15

    // 生成32个字符的会话ID，每个字符第一个十六进制数字
    for(int i = 0; i < 32; ++i)
    {
        ss << std::hex << dist(rng_);  // dist(rng_)生成一个随机数，转成16进制，一共32个并拼接在一起
    }
    return ss.str();
}

// 从HTTP请求的Cookie头部中提取会话ID
std::string SessionManager::getSessionIdFromCookie(const HttpRequest& req)
{
    /*  存放在请求头中：
        Cookie: sessionId=abc123def456; username=john; theme=dark 
    */
    std::string sessionId;
    std::string cookie = req.getHeader("Cookie");
    if(!cookie.empty())
    {
        size_t pos = cookie.find("sessionId=");
        if(pos != std::string::npos)
        {
            pos += 10;  // 跳过sessionId=这10个字符
            size_t end = cookie.find(";", pos);  // 从pos开始找
            if(end != std::string::npos)
            {
                sessionId = cookie.substr(pos, end-pos);
            }
            else
            {
                sessionId = cookie.substr(pos);
            }
        }
    }
    return sessionId;
}

void SessionManager::setSessionCookie(const std::string& sessionId, HttpResponse* resp)
{
    // 把会话Id设置到响应头中，作为Cookie
    std::string cookie = "sessionId=" + sessionId + "; Path=/; HttpOnly";
    resp->addHeader("Set-Cookie", cookie);
}

}

}