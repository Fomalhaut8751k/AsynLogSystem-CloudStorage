#include "../../include/http/HttpRequest.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <utility>

namespace http_v2
{

HttpRequest::HttpRequest(): method_(kInvalid) {}

void HttpRequest::setReceiveTime(TimeStamp t)
{
    receiveTime_ = t;
}

bool HttpRequest::setMethod(const char* start, const char* end)
{
    std::string method(start, end);
    if(method == "GET") method_ = kGet;
    else if(method == "POST") method_ = kPost;
    else if(method == "HEAD") method_ = kHead;
    else if(method == "PUT") method_ = kPut;
    else if(method == "DELETE") method_ = kDelete;
    else if(method == "OPTIONS") method_ = kOptions;
    else method_ = kInvalid;
    return method_ != kInvalid;
}

void HttpRequest::setPath(const char* start, const char* end)
{
    path_.assign(start, end);
}

void HttpRequest::setPathParameters(const std::string& key, const std::string& value)
{
    pathParameters_[key] = value;
}

std::string HttpRequest::getPathParameters(const std::string& key) const
{
    auto it = pathParameters_.find(key);
    return it == pathParameters_.end() ? "" : it->second;
}

void HttpRequest::setQueryParameters(const char* start, const char* end)
{
    std::string query(start, end);
    std::stringstream ss(query);
    std::string item;
    while(std::getline(ss, item, '&'))
    {
        size_t equal = item.find('=');
        if(equal != std::string::npos)
        {
            queryParameters_[item.substr(0, equal)] = item.substr(equal + 1);
        }
        else if(!item.empty())
        {
            queryParameters_[item] = "";
        }
    }
}

std::string HttpRequest::getQueryParameters(const std::string& key) const
{
    auto it = queryParameters_.find(key);
    return it == queryParameters_.end() ? "" : it->second;
}

void HttpRequest::addHeader(const char* start, const char* colon, const char* end)
{
    std::string field(start, colon);
    ++colon;
    while(colon < end && std::isspace(static_cast<unsigned char>(*colon)))
    {
        ++colon;
    }
    std::string value(colon, end);
    while(!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
    {
        value.pop_back();
    }
    headers_[field] = value;
}

std::string HttpRequest::getHeader(const std::string& field) const
{
    auto it = headers_.find(field);
    return it == headers_.end() ? "" : it->second;
}

void HttpRequest::setBody(const char* start, const char* end)
{
    content_.assign(start, end);
}

void HttpRequest::swap(HttpRequest& that)
{
    std::swap(method_, that.method_);
    version_.swap(that.version_);
    path_.swap(that.path_);
    pathParameters_.swap(that.pathParameters_);
    queryParameters_.swap(that.queryParameters_);
    std::swap(receiveTime_, that.receiveTime_);
    headers_.swap(that.headers_);
    content_.swap(that.content_);
    std::swap(contentLength_, that.contentLength_);
}

}
