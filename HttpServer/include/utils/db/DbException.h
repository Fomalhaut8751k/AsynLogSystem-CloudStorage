#ifndef DBEXCEPTION_H
#define DBEXCEPTION_H

#include <stdexcept>
#include <string>

namespace http
{

namespace db
{

// 简单的数据库异常类
class DbException: public std::runtime_error
{
public:
    explicit DbException(const std::string& message):
        std::runtime_error(message) {}

    explicit DbException(const char* message):
        std::runtime_error(message) {}
};

}

}

#endif