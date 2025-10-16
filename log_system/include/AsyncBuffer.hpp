#ifndef ASYNCBUFFER_H
#define ASYNCBUFFER_H
#include <iostream>
#include <cstring>
#include <vector>
#include <string>

#include "Message.hpp"

#define UNIT_SPACE 400

namespace mylog
{
    // 异步缓冲区模块
    class AsyncBuffer
    {
    protected:
        std::vector<char> buffer_;  // 缓冲区
        unsigned int buffer_size_;   // 缓冲区长度
        unsigned int buffer_pos_;    // 可写入位置的起点

        std::mutex BufferWriteMutex_;
        std::condition_variable cv_;

    public:
        AsyncBuffer()
        {
            buffer_.resize(1024, '\0');  // 预留1024大小的空间
            buffer_size_ = UNIT_SPACE;
            buffer_pos_ = 0;
        }

        ~AsyncBuffer() = default;

        // 获取缓冲区可用空间大小
        unsigned int getAvailable() { return UNIT_SPACE - buffer_pos_; }

        // 将用户的日志信息写入:
        void write(const char* message_unformatted, unsigned int length)
        {
            /*
                可能的情况：
                一个用户写入数据，此时消费者正在忙，之后另一个用户写入数据，
                此时消费者还在忙，如此，生产者缓冲区中就可能包含多条日志消息
                因此需要分开处理

                如果一个用户将要写入数据，发现缓冲区不够写了，就等待？
            */
            std::unique_lock<std::mutex> lock(BufferWriteMutex_);

            // if(UNIT_SPACE - buffer_pos_  < length)
            // {
            //     std::cerr << "空间不足: " << UNIT_SPACE << " " << buffer_pos_ << " " << length << std::endl;
            //     return;  // 先丢掉，等待之后实现
            // }
            std::memcpy(buffer_.data() + buffer_pos_, message_unformatted, length);
            buffer_pos_ += (length + 1);  // 加1空一个0作为结束符
        }

        // 获取缓冲区的日志信息
        std::vector<std::string> read() 
        {
            std::vector<std::string> msg_log_set;
            int start_pos = 0, end_pos = buffer_pos_;
            while(start_pos < end_pos)
            {
                const char* message_log = buffer_.data() + start_pos;
                start_pos += (strlen(message_log) + 1);
                msg_log_set.emplace_back(std::string(message_log));
            }
            return msg_log_set;
        }

        // 清空缓冲区的消息
        void clear()
        {
            buffer_.assign(1024, '\0');  // 清空缓冲区的数据
            buffer_pos_ = 0;
        }
    };
}

#endif