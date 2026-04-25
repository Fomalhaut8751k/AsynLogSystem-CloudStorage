#ifndef HTTPSERVER_V2_TIMESTAMP_H
#define HTTPSERVER_V2_TIMESTAMP_H

#include <chrono>

namespace http_v2
{

class TimeStamp
{
public:
    using Clock = std::chrono::system_clock;

    TimeStamp(): timePoint_(Clock::now()) {}
    explicit TimeStamp(Clock::time_point timePoint): timePoint_(timePoint) {}

    static TimeStamp now() { return TimeStamp(Clock::now()); }
    Clock::time_point timePoint() const { return timePoint_; }

private:
    Clock::time_point timePoint_;
};

}

#endif
