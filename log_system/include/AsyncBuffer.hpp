#ifndef ASYNCBUFFER_H
#define ASYNCBUFFER_H
#include <iostream>
#include <cstring>
#include <vector>
#include <string>
#include <shared_mutex>

#include "Message.hpp"
#include "LogSystemConfig.hpp"

#define LOG_BUFFER_INIT_SIZE   (4 * 1024 * 1024)   // 4MB

namespace mylog
{
    // 异步缓冲区模块
    class AsyncBuffer
    {
    protected:
        std::vector<char> buffer_;  // 缓冲区
        unsigned int buffer_pos_;    // 可写入位置的起点

        uint32_t init_buffer_size_;   // 初始的缓冲区大小

        std::mutex BufferWriteMutex_;
        std::mutex BufferReadMutex_;
        std::condition_variable cv_;

        std::atomic_uint32_t buffer_size_;  // 缓冲区总大小，缓冲区总的大小在扩容和缩容的时候会发生改变

    public:
        AsyncBuffer(){
            // init_buffer_size_ = mylog::Config::GetInstance().GetInitBufferSize();  // 从配置文件中初始化
            init_buffer_size_ = LOG_BUFFER_INIT_SIZE;
            
            buffer_.resize(init_buffer_size_, '\0');  // 预留UNIT_SPACE大小的空间
            buffer_pos_ = 0;
        }

        ~AsyncBuffer() = default;

        // 获取缓冲区可用空间大小
        // unsigned int getAvailable() const { return buffer_.size() - buffer_pos_; }
        unsigned int getAvailable() const { return buffer_size_ - buffer_pos_;}

        // 获取缓冲区总的空间大小
        // unsigned int getSize() const { return buffer_.size(); }
        unsigned int getSize() const { return buffer_size_; }

        // 判断使用的缓冲区大小是否大于初始大小，用于扩容下的判断
        bool getIdleExpansionSpace() const { return buffer_pos_ < init_buffer_size_; }

        // 判断缓冲区是否为空
        bool getEmpty() const { return buffer_pos_ == 0; }

        // 更新缓冲区大小
        void updateBufferSize(){
            buffer_size_ = buffer_.size();
        }

        int scaleUp(unsigned int expand_size, int buffertype = 0)
        {
            // 扩容和写操作互斥，因为扩容可能会开辟新的空间
            if(expand_size <= 0) { return -1; }
            if(buffertype == 0){  // 写
                std::unique_lock<std::mutex> lock(BufferWriteMutex_); 
                buffer_.resize(buffer_.size() + expand_size, '\0');
            }
            else{ // (buffertype == 1)  // 读
                std::unique_lock<std::mutex> lock(BufferReadMutex_);
                buffer_.resize(buffer_.size() + expand_size, '\0');
            }
            // buffer_size_ += expand_size;
            return 0;
        }

        int scaleUpProductorWithLogQueueLoad(unsigned int expand_size, const char* message_unformatted, unsigned int length){
            if(expand_size <= 0 || length <= 0) { return -1; }
            std::unique_lock<std::mutex> lock(BufferWriteMutex_); 
            // 扩容
            buffer_.resize(buffer_.size() + expand_size, '\0');
            // 把logQueue_中的日志写到扩容后的缓冲区当中
            std::memcpy(buffer_.data() + buffer_pos_, message_unformatted, length);
            buffer_pos_ += length;  
            buffer_[buffer_pos_++] = '\n';  // 加1空一个\n作为换行符
            buffer_size_ = buffer_.size();
        }

        int scaleUpConsumer(unsigned int expand_size){
            std::unique_lock<std::mutex> lock(BufferReadMutex_);
            buffer_.resize(buffer_.size() + expand_size, '\0');    // 加1空一个\n作为换行符
            buffer_size_ = buffer_.size();
            return 0;
        }

        // 缩容
        int scaleDown()
        {   // 虽然说缩容没有开辟新空间的可能，但是还是加上锁安全一点
            buffer_.resize(init_buffer_size_);   // 回到初始大小
            updateBufferSize();
            return 0;
        }

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
            std::memcpy(buffer_.data() + buffer_pos_, message_unformatted, length);
            buffer_pos_ += length;  
            buffer_[buffer_pos_++] = '\n';  // 加1空一个\n作为换行符
        }

        // 获取缓冲区的日志信息
        std::vector<std::string> read() 
        {
            std::unique_lock<std::mutex> lock(BufferReadMutex_);
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

        // 获取缓冲区的日志信息二
        void read(std::vector<std::string>& msg_log_set) 
        {
            int start_pos = 0, end_pos = buffer_pos_;
            while(start_pos < end_pos)
            {
                const char* message_log = buffer_.data() + start_pos;
                start_pos += (strlen(message_log) + 1);
                msg_log_set.emplace_back(std::string(message_log));
            }
        }

        // 获取缓冲区的日志信息三
        std::string read(int option){
            std::unique_lock<std::mutex> lock(BufferReadMutex_);
            auto it = buffer_.begin() + buffer_pos_ - 1;  // 最后一个\n不要
            return std::string(buffer_.begin(), it);
        }

        // 清空缓冲区的消息
        void clear(){
            // buffer_.assign(buffer_size_, '\0');  // 清空缓冲区的数据
            buffer_.assign(buffer_.size(), '\0');
            buffer_pos_ = 0;
        }
    };
}

#endif