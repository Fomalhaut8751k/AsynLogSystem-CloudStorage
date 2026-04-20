#ifndef UTILS2_H
#define UTILS2_H

#include <iostream>
#include <sys/statvfs.h>
#include <string>
#include <list>
#include <thread>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <iomanip>
#include <deque>

using namespace std;

class DiskSpaceChecker {
public:
    struct DiskInfo {
        unsigned long total_bytes;
        unsigned long free_bytes;
        unsigned long available_bytes;
        double usage_percent;
    };
    
    static DiskInfo get_disk_info(const std::string& path = "/") {
        struct statvfs stat;
        DiskInfo info = {0, 0, 0, 0.0};
        
        if (statvfs(path.c_str(), &stat) != 0) {
            perror("statvfs failed");
            return info;
        }
        
        // 计算字节数
        unsigned long block_size = stat.f_frsize;
        info.total_bytes = stat.f_blocks * block_size;
        info.free_bytes = stat.f_bfree * block_size;
        info.available_bytes = stat.f_bavail * block_size;
        
        if (info.total_bytes > 0) {
            info.usage_percent = (1.0 - (double)info.available_bytes / info.total_bytes) * 100;
        }
        
        return info;
    }
    
    static void print_disk_info(const DiskInfo& info) {
        std::cout << "总空间: " << format_bytes(info.total_bytes) << std::endl;
        std::cout << "已用空间: " << format_bytes(info.total_bytes - info.free_bytes) << std::endl;
        std::cout << "剩余空间: " << format_bytes(info.free_bytes) << std::endl;
        std::cout << "可用空间: " << format_bytes(info.available_bytes) << std::endl;
        std::cout << "使用率: " << info.usage_percent << "%" << std::endl;
    }
    
private:
    static std::string format_bytes(unsigned long bytes) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int unit_index = 0;
        double size = bytes;
        
        while (size >= 1024 && unit_index < 4) {
            size /= 1024;
            unit_index++;
        }
        
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[unit_index]);
        return std::string(buffer);
    }
};

class LogQueue{
private:
    struct LogNode{
        LogNode(string s):str(s){}
        // ~LogNode(){ std::cout << "~LogNode()" << std::endl; }
        string str; 
        std::shared_ptr<LogNode> prev;  // 必须有一个方向是强智能指针，不然创建完后就被释放
        std::weak_ptr<LogNode> next;
    };

    std::shared_ptr<LogNode> pre;  // 不存储数据的头部节点
    std::shared_ptr<LogNode> tail;

    std::uint32_t logSize;  // 当前队列中日志的大小

    std::mutex Mutex;

public:
    LogQueue():pre(std::make_shared<LogNode>("pre")){
        std::shared_ptr<LogNode> labelNode = std::make_shared<LogNode>("label");
        pre->next = labelNode;
        labelNode->prev = pre;
        tail = labelNode;
        logSize = 0;
    }

    ~LogQueue() = default;

    std::uint32_t insertFromHead(string s, const int& slen = 0, unsigned int insert_threshold = 512 * 1024 * 1024){
        std::lock_guard<std::mutex> lock(Mutex);
        if(slen != -1 && s.length() + logSize > insert_threshold){  // 如果链表已经满了，就不再添加
            return 0; 
        }
        std::shared_ptr<LogNode> nnode = std::make_shared<LogNode>(s);
        std::shared_ptr<LogNode> preNext = pre->next.lock();

        pre->next = nnode;
        nnode->prev = pre;
        nnode->next = preNext;
        preNext->prev = nnode;

        std::uint32_t logSizeCopy = 0;
        if(slen == -1){ // 给删除用
            logSizeCopy = logSize;
            logSize = 0;  // 插入label节点后，就相当于label到pre的这段空间没有节点，是空的了，可以插入
            return logSizeCopy;  // 删除操作会先插入一个label，然后返回此时的日志量
        }
        else logSize += (s.length() + 1);
        return logSize;
    }

    void deleteFromTail(std::string& logBuffer){
        if(empty()) return;  
        insertFromHead("label");
        do{
            std::shared_ptr<LogNode> dnode = tail; 
            tail = tail->prev;
            if(dnode->str != "label"){
                logBuffer += (dnode->str + "\n");
            }
            dnode.reset();
        }while(tail->str != "label");
    }

    void deleteFromTail(std::stringstream& ss){
        if(empty()) return;  
        insertFromHead("label");
        do{
            std::shared_ptr<LogNode> dnode = tail; 
            tail = tail->prev;
            if(dnode->str != "label"){
                ss << dnode->str << "\n";
            }
            dnode.reset();
        }while(tail->str != "label");
    }

    void deleteFromTail(std::string& logBuffer, int version){
        if(empty()) return;  
        std::uint32_t logSize = insertFromHead("label");  // 要处理的日志大小
        logBuffer.reserve(logSize);
        do{
            std::shared_ptr<LogNode> dnode = tail; 
            tail = tail->prev;
            if(dnode->str != "label"){
                logBuffer.append(dnode->str);
                logBuffer.append("\n");
                // logBuffer += (dnode->str + "\n");
            }
            dnode.reset();
        }while(tail->str != "label");
    }

    uint32_t getLogSize(){
        std::lock_guard<std::mutex> lock(Mutex);
        return logSize;
    }

    bool empty(){  // 插入操作
        return tail->prev == pre;
    }

    // void showQueueAllNode(){
    //     std::lock_guard<std::mutex> lock(Mutex);
    //     LogNode* it = pre.get();
    //     while(it){
    //         cout << it->str << " ";
    //         it = it->next.lock().get();
    //     }cout << endl;
    // }
};


class LogQueueWithPool{
private:
    struct LogNode{  // sizeof(LogNode) = 72
        LogNode(string s, bool fp = false):str(s), fromPool(fp){}
        // ~LogNode(){ std::cout << "~LogNode()" << std::endl; }
        string str; 
        std::shared_ptr<LogNode> prev;  // 必须有一个方向是强智能指针，不然创建完后就被释放
        std::weak_ptr<LogNode> next;
        bool fromPool;  // 判断是否是节点池的
    };

    std::shared_ptr<LogNode> pre;  // 不存储数据的头部节点
    std::shared_ptr<LogNode> tail;

    std::uint32_t logSize;  // 当前队列中日志的大小

    std::mutex Mutex;  // 用来互斥插入和删除操作

    // 预设的节点池，当日志量少时，直接使用池中的节点，可以避免频繁的创建和销毁
    const uint32_t NODE_POOL_INIT_SIZE; 
    std::deque<std::shared_ptr<LogNode>> logNodePool_;  
    std::mutex MutexForPool;

    std::shared_ptr<LogNode> getConnection(){  // 获取节点
        // [优化] 用 MutexForPool 单独保护节点池，与队列的 Mutex 分离，
        // 避免 insert 路径上两把锁嵌套时 MutexForPool 成为瓶颈。
        // retConnection 也使用同一把锁，保证 logNodePool_ 访问的线程安全。
        std::lock_guard<std::mutex> lock(MutexForPool);
        if(logNodePool_.empty()){
            return nullptr;
        }
        std::shared_ptr<LogNode> nptr = logNodePool_.front();
        logNodePool_.pop_front();
        return nptr;
    }

    void retConnection(std::shared_ptr<LogNode>& nptr){  // 归还节点
        std::lock_guard<std::mutex> lock(MutexForPool);
        logNodePool_.push_back(nptr);
    }

public:
    LogQueueWithPool():pre(std::make_shared<LogNode>("pre")),
                       NODE_POOL_INIT_SIZE(1024 * 1024){
        std::shared_ptr<LogNode> labelNode = std::make_shared<LogNode>("label");
        pre->next = labelNode;
        labelNode->prev = pre;
        tail = labelNode;
        logSize = 0;

        for(int i = 0; i < NODE_POOL_INIT_SIZE; i++){
            logNodePool_.push_back(std::make_shared<LogNode>("", true));
        }
    }

    ~LogQueueWithPool(){

    }

    std::uint32_t insertFromHead(const std::string& s, const int& slen = 0, unsigned int insert_threshold = 512 * 1024 * 1024){
        // [优化] 将 logSize 的读写和链表操作都放在同一把锁内，
        // 原来 logSize 的阈值检查和更新在锁外，存在数据竞争。
        std::lock_guard<std::mutex> lock(Mutex);
        if(slen != -1 && s.length() + logSize > insert_threshold){  // 如果链表已经满了，就不再添加
            return -1;
        }

        std::shared_ptr<LogNode> nnode = getConnection();  // 如果节点池中有，就使用池中的节点，否则返回空
        if(!nnode){
            nnode = std::make_shared<LogNode>(s);
        }else{
            nnode->str = s;  // 等号赋值
        }

        std::shared_ptr<LogNode> preNext = pre->next.lock();

        pre->next = nnode;
        nnode->prev = pre;
        nnode->next = preNext;
        preNext->prev = nnode;

        std::uint32_t logSizeCopy = 0;
        if(slen == -1){ // 给删除用
            logSizeCopy = logSize;
            logSize = 0;  // 插入label节点后，就相当于label到pre的这段空间没有节点，是空的了，可以插入
            return logSizeCopy;  // 删除操作会先插入一个label，然后返回此时的日志量
        }
        else logSize += (s.length() + 1);
        return logSize;
    }

    std::uint32_t insertFromHead(std::string&& s, const int& slen = 0, unsigned int insert_threshold = 512 * 1024 * 1024){
        std::lock_guard<std::mutex> lock(Mutex);
        if(slen != -1 && s.length() + logSize > insert_threshold){  // 如果链表已经满了，就不再添加
            return -1; 
        }
        std::shared_ptr<LogNode> nnode = getConnection();  // 如果节点池中有，就使用池中的节点，否则返回空
        if(!nnode){
            nnode = std::make_shared<LogNode>(std::move(s));
        }else{
            nnode->str = std::move(s);  
        }
        std::shared_ptr<LogNode> preNext = pre->next.lock();
        
        pre->next = nnode;
        nnode->prev = pre;
        nnode->next = preNext;
        preNext->prev = nnode;

        std::uint32_t logSizeCopy = 0;
        if(slen == -1){ // 给删除用
            logSizeCopy = logSize;
            logSize = 0;  // 插入label节点后，就相当于label到pre的这段空间没有节点，是空的了，可以插入
            return logSizeCopy;  // 删除操作会先插入一个label，然后返回此时的日志量
        }
        else logSize += (s.length() + 1);
        return logSize;
    }

    void deleteFromTail(std::string& logBuffer){
        if(empty()) return;  
        insertFromHead("label", -1);
        do{
            std::shared_ptr<LogNode> dnode = tail; 
            tail = tail->prev;
            if(dnode->str != "label"){
                logBuffer += (dnode->str + "\n");
            }
            dnode.reset();
        }while(tail->str != "label");
    }

    void deleteFromTail(std::stringstream& ss){
        if(empty()) return;  
        insertFromHead("label", -1);
        do{
            std::shared_ptr<LogNode> dnode = tail; 
            tail = tail->prev;
            if(dnode->str != "label"){
                ss << dnode->str << "\n";
            }
            // 如果是从节点池中取出来的，就直接放回去
            if(dnode->fromPool){
                retConnection(dnode);  // 此时dnode的这个节点在reset之后依然存在
                dnode->prev = nullptr;  // 因为dnode还存在，所以他的prev作为强智能指针依然贡献了引用计数
            }
            dnode.reset();
        }while(tail->str != "label");
    }

    void deleteFromTail(std::string& logBuffer, int version){
        if(empty()) return;  
        std::uint32_t logSize = insertFromHead("label", -1);  // 要处理的日志大小
        logBuffer.reserve(logSize);
        do{
            std::shared_ptr<LogNode> dnode = tail; 
            tail = tail->prev;
            if(dnode->str != "label"){
                logBuffer.append(dnode->str);
                logBuffer.append("\n"); 
            }
            // 如果是从节点池中取出来的，就直接放回去
            if(dnode->fromPool){
                retConnection(dnode);  // 此时dnode的这个节点在reset之后依然存在
                dnode->prev = nullptr;  // 因为dnode还存在，所以他的prev作为强智能指针依然贡献了引用计数
            }
            dnode.reset();
        }while(tail->str != "label");
    }

    uint32_t getLogSize(){
        std::lock_guard<std::mutex> lock(Mutex);
        return logSize;
    }

    bool empty(){  // 插入操作
        return tail->prev == pre;
    }
};


// class LogQueueWithPoolv2{
// private:
//     struct LogNode{  // sizeof(LogNode) = 72
//         LogNode(const char* s, size_t slen, bool fp = false):
//             str(std::make_unique<char[]>(slen + 1))  
//             slen(slen), 
//             fromPool(fp){

//             std::memcpy(str.get(), s, slen);  // 深拷贝
//             str[slen] = '\0';
//         }

//         std::unique_ptr<char[]> str;
//         size_t slen;

//         std::shared_ptr<LogNode> prev;  // 必须有一个方向是强智能指针，不然创建完后就被释放
//         std::weak_ptr<LogNode> next;
//         bool fromPool;  // 判断是否是节点池的
//     };

//     unsigned int insert_threshold; 

//     std::shared_ptr<LogNode> pre;  // 不存储数据的头部节点
//     std::shared_ptr<LogNode> tail;

//     std::uint32_t logSize;  // 当前队列中日志的大小

//     std::mutex Mutex;  // 用来互斥插入和删除操作

//     // 预设的节点池，当日志量少时，直接使用池中的节点，可以避免频繁的创建和销毁
//     const uint32_t NODE_POOL_INIT_SIZE; 
//     std::deque<std::shared_ptr<LogNode>> logNodePool_;  
//     std::mutex MutexForPool;

//     std::shared_ptr<LogNode> getConnection(){  // 获取节点
//         std::lock_guard<std::mutex> lock(MutexForPool);
//         if(logNodePool_.empty()){
//             return nullptr;
//         }
//         std::shared_ptr<LogNode> nptr = logNodePool_.front();
//         logNodePool_.pop_front();
//         return nptr;
//     }

//     void retConnection(std::shared_ptr<LogNode>& nptr){  // 归还节点
//         std::lock_guard<std::mutex> lock(MutexForPool);
//         logNodePool_.push_back(nptr);
//     }

// public:
//     LogQueueWithPool():pre(std::make_shared<LogNode>("pre")),
//                        NODE_POOL_INIT_SIZE(1024 * 1024),
//                        insert_threshold(512 * 1024 * 1024){
//         std::shared_ptr<LogNode> labelNode = std::make_shared<LogNode>("label");
//         pre->next = labelNode;
//         labelNode->prev = pre;
//         tail = labelNode;
//         logSize = 0;

//         for(int i = 0; i < NODE_POOL_INIT_SIZE; i++){
//             logNodePool_.push_back(std::make_shared<LogNode>("", true));
//         }
//     }

//     ~LogQueueWithPool(){

//     }

//     std::uint32_t insertFromHead(const std::string& s, const int& slen = 0){
//         std::lock_guard<std::mutex> lock(Mutex);
//         if(slen != -1 && s.length() + logSize > insert_threshold){  // 如果链表已经满了，就不再添加
//             return -1; 
//         }
//         // std::shared_ptr<LogNode> nnode = std::make_shared<LogNode>(s);

//         std::shared_ptr<LogNode> nnode = getConnection();  // 如果节点池中有，就使用池中的节点，否则返回空
//         if(!nnode){
//             nnode = std::make_shared<LogNode>(s);
//         }else{
//             nnode->str = s; 
//         }
//         std::shared_ptr<LogNode> preNext = pre->next.lock();
        
//         pre->next = nnode;
//         nnode->prev = pre;
//         nnode->next = preNext;
//         preNext->prev = nnode;

//         std::uint32_t logSizeCopy = 0;
//         if(slen == -1){ // 给删除用
//             logSizeCopy = logSize;
//             logSize = 0;  // 插入label节点后，就相当于label到pre的这段空间没有节点，是空的了，可以插入
//             return logSizeCopy;  // 删除操作会先插入一个label，然后返回此时的日志量
//         }
//         else logSize += (s.length() + 1);
//         return logSize;
//     }

//     std::uint32_t insertFromHead(const char* s, const int& slen){
//         std::lock_guard<std::mutex> lock(Mutex);
//         if(slen != -1 && slen + logSize > insert_threshold){  // 如果链表已经满了，就不再添加
//             return -1; 
//         }
//         std::shared_ptr<LogNode> nnode = getConnection();  // 如果节点池中有，就使用池中的节点，否则返回空
//         if(!nnode){
//             nnode = std::make_shared<LogNode>(s);
//         }else{
//             nnode->str = s;   // std::unique_ptr<char[]> str;
//         }
//         std::shared_ptr<LogNode> preNext = pre->next.lock();
        
//         pre->next = nnode;
//         nnode->prev = pre;
//         nnode->next = preNext;
//         preNext->prev = nnode;

//         std::uint32_t logSizeCopy = 0;
//         if(slen == -1){ // 给删除用
//             logSizeCopy = logSize;
//             logSize = 0;  // 插入label节点后，就相当于label到pre的这段空间没有节点，是空的了，可以插入
//             return logSizeCopy;  // 删除操作会先插入一个label，然后返回此时的日志量
//         }
//         else logSize += (s.length() + 1);
//         return logSize;
//     }

//     void deleteFromTail(std::string& logBuffer){
//         if(empty()) return;  
//         std::uint32_t logSize = insertFromHead("label", -1);  // 要处理的日志大小
//         logBuffer.reserve(logSize);
//         do{
//             std::shared_ptr<LogNode> dnode = tail; 
//             tail = tail->prev;
//             if(dnode->str != "label"){
//                 logBuffer.append(dnode->str);
//                 logBuffer.append("\n"); 
//             }
//             // 如果是从节点池中取出来的，就直接放回去
//             if(dnode->fromPool){
//                 retConnection(dnode);  // 此时dnode的这个节点在reset之后依然存在
//                 dnode->prev = nullptr;  // 因为dnode还存在，所以他的prev作为强智能指针依然贡献了引用计数
//             }
//             dnode.reset();
//         }while(tail->str != "label");
//     }

//     uint32_t getLogSize(){
//         std::lock_guard<std::mutex> lock(Mutex);
//         return logSize;
//     }

//     bool empty(){  // 插入操作
//         return tail->prev == pre;
//     }
// };

class LogQueueByDeque{
private:
    std::deque<char> logDeque;

    std::uint32_t logSize;  // 当前队列中日志的大小
    std::mutex Mutex;  // 用来互斥插入和删除操作

public:
    LogQueueByDeque():logSize(0){
        // logDeque.reserve(size);
    }

    ~LogQueueByDeque(){

    }

    std::uint32_t insertFromHead(std::string& s, const int& slen = 0){
        std::lock_guard<std::mutex> lock(Mutex);
        // 头部插入，并且是反向插入，在日志输出的时候在整体反转一下，就能保证日志的输出顺序
        logDeque.insert(logDeque.begin(), s.rbegin(), s.rend());  
        logDeque.push_front('\n');
        logSize += (s.length() + 1);
        return logSize;
    }

    void deleteFromTail(std::string& logBuffer){
        std::deque<char>::reverse_iterator rit;
        {   std::lock_guard<std::mutex> lock(Mutex);
            rit = logDeque.rend();
        }
        // 只要至少有一个节点，头插就不会影响rbegin()和end()
        if(rit == logDeque.rbegin()){
            return;
        }
        logSize = 0; 
        logBuffer = std::string(logDeque.rbegin(), rit);
        logDeque.erase(rit.base(), logDeque.end());  
    }
};

class Semaphore {
private:
    std::mutex mtx;
    std::condition_variable cv;
    int count;
 
public:
    // 构造函数：初始化资源数量
    explicit Semaphore(int initial_count = 0) : count(initial_count) {}
 
    // P操作 (acquire / wait)
    void acquire() {
        std::unique_lock<std::mutex> lock(mtx);
        
        // 使用 lambda 表达式防止虚假唤醒 (Spurious Wakeup)
        // 只有当 count > 0 时才继续，否则阻塞
        cv.wait(lock, [this]() { return count > 0; });
        
        count--; // 消耗资源
    }
 
    // V操作 (release / signal)
    void release() {
        std::unique_lock<std::mutex> lock(mtx);
        count++; // 增加资源
        
        // 唤醒一个等待的线程。
        // 注意：通常先修改数据再 notify，lock 可以自动释放
        cv.notify_one(); 
    }
};


unsigned int calculateTotalCapacity(unsigned int logSize, unsigned int bufferSize) {
    if (logSize <= bufferSize || bufferSize == 0) return bufferSize;
    
    unsigned int ratio = (logSize + bufferSize - 1) / bufferSize;
    
    // 手动实现 bit_ceil
    unsigned int multiplier = 1;
    while (multiplier < ratio) {
        multiplier <<= 1;
    }
    
    return bufferSize * multiplier;
}


#endif