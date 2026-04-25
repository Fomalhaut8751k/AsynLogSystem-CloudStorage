#include "../../include/session/SessionManager.h"

#include <sstream>

namespace http_v2
{
namespace session
{

SessionManager::SessionManager(std::unique_ptr<SessionStorage> storage):
    storage_(std::move(storage)),
    rng_(std::random_device{}())
{
}

std::shared_ptr<Session> SessionManager::getSession(const HttpRequest& req, HttpResponse* resp)
{
    std::string sessionId = getSessionIdFromCookie(req);
    std::shared_ptr<Session> session;

    if(!sessionId.empty())
    {
        session = storage_->load(sessionId);
    }

    if(!session)
    {
        if(!sessionId.empty()) storage_->remove(sessionId);
        sessionId = generateSessionId();
        session = std::make_shared<Session>(sessionId, this);
        setSessionCookie(sessionId, resp);
        storage_->save(session);
    }
    else
    {
        session->setManager(this);
    }

    session->refresh();
    return session;
}

void SessionManager::destorySession(const std::string& sessionId)
{
    storage_->remove(sessionId);
}

void SessionManager::cleanExpiredSession()
{
    storage_->removeExpired();
}

void SessionManager::updateSession(std::shared_ptr<Session> session)
{
    storage_->save(std::move(session));
}

std::string SessionManager::generateSessionId()
{
    std::stringstream ss;
    std::uniform_int_distribution<> dist(0, 15);
    for(int i = 0; i < 32; ++i)
    {
        ss << std::hex << dist(rng_);
    }
    return ss.str();
}

std::string SessionManager::getSessionIdFromCookie(const HttpRequest& req)
{
    std::string cookie = req.getHeader("Cookie");
    size_t pos = cookie.find("sessionId=");
    if(pos == std::string::npos) return "";

    pos += 10;
    size_t end = cookie.find(';', pos);
    return end == std::string::npos ? cookie.substr(pos) : cookie.substr(pos, end - pos);
}

void SessionManager::setSessionCookie(const std::string& sessionId, HttpResponse* resp)
{
    if(resp)
    {
        resp->addHeader("Set-Cookie", "sessionId=" + sessionId + "; Path=/; HttpOnly");
    }
}

}
}
