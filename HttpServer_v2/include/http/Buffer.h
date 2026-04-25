#ifndef HTTPSERVER_V2_BUFFER_H
#define HTTPSERVER_V2_BUFFER_H

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace http_v2
{

class Buffer
{
public:
    size_t readableBytes() const { return buffer_.size() - readIndex_; }
    const char* peek() const { return buffer_.data() + readIndex_; }

    const char* beginWrite() const { return buffer_.data() + buffer_.size(); }

    const char* findCRLF() const
    {
        const char* begin = peek();
        const char* end = buffer_.data() + buffer_.size();
        const char* crlf = std::search(begin, end, kCRLF, kCRLF + 2);
        return crlf == end ? nullptr : crlf;
    }

    void retrieve(size_t len)
    {
        if(len >= readableBytes())
        {
            retrieveAll();
            return;
        }
        readIndex_ += len;
        compactIfNeeded();
    }

    void retrieveUntil(const char* end)
    {
        retrieve(static_cast<size_t>(end - peek()));
    }

    void retrieveAll()
    {
        buffer_.clear();
        readIndex_ = 0;
    }

    void append(const char* data, size_t len)
    {
        if(len == 0) return;
        buffer_.insert(buffer_.end(), data, data + len);
    }

    void append(const char* data) { append(data, std::strlen(data)); }
    void append(const std::string& data) { append(data.data(), data.size()); }

private:
    void compactIfNeeded()
    {
        if(readIndex_ > 4096 && readIndex_ * 2 >= buffer_.size())
        {
            buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<long>(readIndex_));
            readIndex_ = 0;
        }
    }

    static constexpr const char* kCRLF = "\r\n";
    std::vector<char> buffer_;
    size_t readIndex_{0};
};

}

#endif
