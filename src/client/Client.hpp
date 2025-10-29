#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <memory>
#include <cstdlib>
#include <algorithm>
#include <iomanip>

#include "Utils.hpp"
#include "base64.h"

class StorageClient {
protected:
    std::string server_url_;
    
    // 执行 wget 命令并返回结果
    int ExecuteWget(const std::string& command) {
        std::cout << "Executing: " << command << std::endl;
        return system(command.c_str());
    }
    
public:
    StorageClient(const std::string& server_url) : server_url_(server_url) {}
    virtual ~StorageClient() = default;
    
    // 上传文件 - 使用 wget
    bool Upload(const std::string& file_path, const std::string& storage_type) {
        // 获取文件名
        size_t last_slash = file_path.find_last_of("/\\");
        std::string filename = (last_slash != std::string::npos) ? 
                              file_path.substr(last_slash + 1) : file_path;
        
        std::string encoded_name = base64_encode(filename);
        
        // 构建 wget 命令
        // std::string command = "wget --post-file=\"" + file_path + "\" ";
        std::string command = "wget --progress=bar:force --tries=1 --timeout=60 --post-file=\"" + file_path + "\" ";
        command += "--header=\"FileName: " + encoded_name + "\" ";
        command += "--header=\"StorageType: " + storage_type + "\" ";
        command += "--header=\"Content-Type: application/octet-stream\" ";
        command += "-O /tmp/upload_response.txt ";  // 保存响应到临时文件
        command += "\"" + server_url_ + "/upload\" ";
        // command += "2>/dev/null";  // 隐藏错误输出
        
        int result = ExecuteWget(command);
        
        if (result == 0) {
            // 读取响应文件
            std::ifstream response_file("/tmp/upload_response.txt");
            if (response_file.is_open()) {
                std::string response((std::istreambuf_iterator<char>(response_file)),
                                   std::istreambuf_iterator<char>());
                std::cout << "Upload response: " << response << std::endl;
                response_file.close();
            }
            // 清理临时文件
            system("rm -f /tmp/upload_response.txt");
            return true;
        } else {
            std::cerr << "Upload failed with exit code: " << result << std::endl;
            return false;
        }
    }
    
    // 下载文件 - 使用 wget
    bool Download(const std::string& filename, const std::string& save_path)
    {
        std::string encoded_filename = mystorage::UrlDecode(filename);
        std::string save_path_ = save_path;

        size_t last_slash = save_path_.find_last_of("/\\");

        if(last_slash == std::string::npos)
        {
            // filename.xx or ""
            if(save_path_ == "")
            {
                save_path_ += encoded_filename;
            }
        }
        else if(last_slash == save_path_.length() - 1)
        {
            // directory/
            mystorage::FileUtil createDir(save_path_.substr(0, last_slash));
            createDir.CreateDirectory();
            save_path_ += encoded_filename;
        }
        else if(last_slash == 1 && filename[0] == '.')
        {
            // ./filename.xx
        }
        else
        {   
            // .directory/filename.xx
            mystorage::FileUtil createDir(save_path_.substr(0, last_slash));
            createDir.CreateDirectory();
        }

        
        std::string command = "wget --progress=bar:force --tries=1 --timeout=60 ";
        command += "-O \"" + save_path_ + "\" ";
        command += "\"" + server_url_ + "/download/" + encoded_filename + "\"";

        std::cerr << save_path_ << " " << encoded_filename << std::endl;
        
        std::cout << "Download: " << filename << std::endl;
        int result = system(command.c_str());
        
        if (result == 0) {
            // 检查文件大小
            std::ifstream file(save_path_, std::ios::binary | std::ios::ate);
            if (file.is_open()) {
                std::streamsize size = file.tellg();
                file.close();
                std::cout << "Successfully downloaded: " << save_path_ 
                        << " (" << size << " bytes)" << std::endl;
                return true;
            } else {
                std::cerr << "Downloaded file is invalid" << std::endl;
                return false;
            }
        } else {
            std::cerr << "Download failed with exit code: " << result << std::endl;
            return false;
        }
    } 
    
    // 获取文件列表 - 使用 wget
    bool ListFiles() {
        // 构建 wget 命令
        std::string command = "wget -q -O - ";
        command += "\"" + server_url_ + "/list\" ";
        command += "2>/dev/null";  // 隐藏错误输出
        
        // 使用 popen 来捕获输出
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            std::cerr << "Failed to execute wget command" << std::endl;
            return false;
        }
        
        char buffer[128];
        std::string result = "";
        while (!feof(pipe)) {
            if (fgets(buffer, 128, pipe) != NULL)
                result += buffer;
        }
        
        int return_code = pclose(pipe);
        
        if (return_code == 0) {
            std::cout << "Files list: " << result << std::endl;
            return true;
        } else {
            std::cerr << "List files failed with exit code: " << return_code << std::endl;
            return false;
        }
    }

    // 删除某个文件
    bool Remove(const std::string& filename)
    {
        // 对文件名进行 Base64 编码
        std::string encoded_name = base64_encode(filename);

        // 构建 wget 命令
        std::string command = "wget -q -O - ";
        command += "--header=\"FileName: " + encoded_name + "\" ";
        command += "\"" + server_url_ + "/remove\" ";
        command += "2>/dev/null";
        
        std::cout << "Remove file: " << filename << std::endl;
        std::cout << "Executing: " << command << std::endl;
        
        // 使用 popen 来捕获输出
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            std::cerr << "Failed to execute wget command" << std::endl;
            return false;
        }
        
        char buffer[128];
        std::string result = "";
        while (!feof(pipe)) {
            if (fgets(buffer, 128, pipe) != NULL)
                result += buffer;
        }
        
        int return_code = pclose(pipe);

        // 若删除成功，服务器会发来一条响应的json串
        if(result == "")
        {
            std::cout << "Remove file: " + filename + " failed" << std::endl;
            return false;
        }
        else
        {
            std::cout << "Remove file: " + filename + " success" << std::endl;
            return true;
        }

        return true;
    }
};


// 超级用户即管理员
class StorageSuperClient: public StorageClient
{
private:
    struct event_base* base_;
    struct sockaddr_in server_addr_;
    struct bufferevent* bev_;

    std::string addr_;
    unsigned int port_;
    std::string server_ip_;

    std::unique_ptr<bool> backlogOnline_;

public:
    StorageSuperClient(const std::string& server_url): StorageClient(server_url) 
    {
        backlogOnline_ = std::make_unique<bool>(false); 
    }

    ~StorageSuperClient()
    {
        if (bev_) {
            bufferevent_free(bev_);
        }
        if (base_) {
            event_base_free(base_);
        }
    }

    // 尝试连接远程服务器
    bool ReConnectBackLog()
    {
        base_ = event_base_new();
        if(!base_)
        {
            mylog::GetLogger("default")->Log({"Reload ThreadPool faild when event_base_new", mylog::LogLevel::WARN});
            return false;
        }

        addr_ = "43.136.108.172";
        port_ = 8080;
        
        memset(&server_addr_, 0, sizeof(server_addr_));
        server_addr_.sin_family = AF_INET;
        server_addr_.sin_addr.s_addr = inet_addr(addr_.c_str());
        server_addr_.sin_port = htons(port_);

        bev_ = bufferevent_socket_new(base_, -1, BEV_OPT_CLOSE_ON_FREE);
        if (NULL == bev_) {
            std::cerr << "bufferevent_socket_new error!" << std::endl;
            event_base_free(base_);
            exit(-1);
        }

        // 打包读回调函数需要用到的参数
        bufferevent_setcb(bev_, NULL, NULL, event_callback, (void*)backlogOnline_.get());
        int ret = bufferevent_enable(bev_, EV_READ | EV_WRITE);
        if(ret < 0)
        {
            return false;
        }

        struct timeval tv = {2, 0}; // 2秒
        struct event *timeout_event = evtimer_new(base_, timeout_cb, (void*)base_);

        // 尝试连接服务器
        ret = bufferevent_socket_connect(bev_, (struct sockaddr*)&server_addr_, sizeof(server_addr_));
        if(ret < 0)  // ret = 0 不代表连接成功
        {
            return false;
        }

        // 启动事件循环，无论连接成功与否都会离开事件循环
        std::thread loop(event_base_dispatch, base_);

        loop.join();
        
        // 如果是因为连接到服务器触发了事件循环，那么*backlogOnline = true
        if(*backlogOnline_)
        {
            return true;
        }
        return false;
    }

    // 通知服务器让线程池重新连接远程服务器
    bool ReLoadThreadPool()
    {
        // 构建 wget 命令
        std::string command = "wget -q -O - ";
        command += "\"" + server_url_ + "/reload\" ";
        command += "2>/dev/null";  // 隐藏错误输出

        // 使用 popen 来捕获输出
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            std::cerr << "Failed to execute wget command" << std::endl;
            return false;
        }
        pclose(pipe);
        
        return true;

        // int return_code = pclose(pipe);
        
        // if (return_code == 0) {
        //     std::cout << "ReLoad threadpool successfully " << std::endl;
        //     return true;
        // } else {
        //     std::cerr << "Reload threadpool failed " << std::endl;
        //     return false;
        // }
    }

    const bool* GetBacklogOnline() const { return backlogOnline_.get(); }

    // 超时回调函数
    static void timeout_cb(evutil_socket_t fd, short event, void *arg) 
    {
        struct event_base* base = (struct event_base*)arg;
        event_base_loopexit(base, nullptr); // 超时即关闭事件循环
    }


    // 连接状态回调函数
    static void event_callback(struct bufferevent *bev, short events, void *ctx)
    {
        // std::cerr << "触发了事件回调函数" << std::endl;
        if (events & BEV_EVENT_CONNECTED) 
        {
            bool* backlogOnline = (bool*)ctx;
            *backlogOnline = true;
        }
        struct event_base *base = bufferevent_get_base(bev);    
        event_base_loopexit(base, NULL);  // 退出事件循环
    }
};