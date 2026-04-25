#ifndef HTTPSERVER_V2_SESSIONMANAGER_H
#define HTTPSERVER_V2_SESSIONMANAGER_H

#include <memory>
#include <random>
#include <string>

#include "SessionStorage.h"
#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"

namespace http_v2
{
namespace session
{

class SessionManager
{
public:
    explicit SessionManager(std::unique_ptr<SessionStorage> storage);

    std::shared_ptr<Session> getSession(const HttpRequest& req, HttpResponse* resp);
    void destorySession(const std::string& sessionId);
    void cleanExpiredSession();
    void updateSession(std::shared_ptr<Session> session);

private:
    std::string generateSessionId();
    std::string getSessionIdFromCookie(const HttpRequest& req);
    void setSessionCookie(const std::string& sessionId, HttpResponse* resp);

    std::unique_ptr<SessionStorage> storage_;
    std::mt19937 rng_;
};

}
}

#endif
