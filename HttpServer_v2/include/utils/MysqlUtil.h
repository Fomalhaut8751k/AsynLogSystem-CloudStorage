#ifndef HTTPSERVER_V2_MYSQLUTIL_H
#define HTTPSERVER_V2_MYSQLUTIL_H

#include <cstddef>
#include <stdexcept>
#include <string>

namespace http_v2
{

class MysqlUtil
{
public:
    static void init(const std::string&, const std::string&, const std::string&, const std::string&, size_t = 10)
    {
        throw std::runtime_error("HttpServer_v2 MysqlUtil is a compatibility stub; database support is not implemented");
    }
};

}

#endif
