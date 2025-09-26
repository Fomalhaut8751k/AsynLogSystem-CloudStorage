#ifndef CONFIG_H
#define CONFIG_H

// #include <filesystem>
#include "json.hpp"
#include "Utils.hpp"

#include <iostream>
#include <sys/stat.h>
#include <fstream>
#include <cstring>

#include "../../../log_system/include/Level.hpp"

using json = nlohmann::json;

namespace mylog
{
    class Config
    {
    private:
        Config(){};
        Config(const Config&) = delete;
        Config& operator=(const Config&) = delete;

        // ip地址和端口号
        std::string server_ip_;
        unsigned int server_port_;

        // 下载目录前缀，深度存储目录，浅度存储目录
        std::string download_prefix_;
        std::string deep_storage_dir_;
        std::string low_storage_dir_;

        // 深度存储的文件类型
        unsigned int bundle_format_;
        // 已存储的文件信息
        std::string storage_info_;

    public:
        static Config& GetInstance()
        {
            static Config config;
            return config;
        }

        std::pair<std::string, mylog::LogLevel> ReadConfig()
        {
            // namespace fs = std::filesystem;

            // fs::path current_path = fs::current_path();
            // std::cout << "当前工作目录: " << current_path << std::endl;

            // Storage.conf中的信息以json串的形式存储
            std::ifstream file_;
            struct stat st;
            // 如果配置文件不存在
            if(stat("../src/server/Storage.conf", &st) != 0)
            { 
                return {"the Storage.conf is not existed", mylog::LogLevel::ERROR};
            }
            file_.open("../src/server/Storage.conf", std::ios::ate);
            if(!file_.is_open()) 
            { 
                return {"the Storage.conf is not open", mylog::LogLevel::ERROR};
            }

            auto size = file_.tellg();        // 文件长度
            std::string content(size, '\0');  // 用于存储
            file_.seekg(0);                   // 从起点开始
            file_.read(&content[0], size);    // 把size大小的内容写入

            json config_js = json::parse(content);   // 读取到json串中

            server_ip_ = config_js["server_ip"];                 // ip地址
            server_port_ = config_js["server_port"];             // 端口号
            download_prefix_ = config_js["download_prefix"];     // 下载目录前缀
            deep_storage_dir_ = config_js["deep_storage_dir"];   // 深度存储目录
            low_storage_dir_ = config_js["low_storage_dir"];     // 浅度存储目录
            bundle_format_ = config_js["bundle_format"];   // 深度存储的文件类型
            storage_info_ = config_js["storage_info"];   // 已存储的文件信息

            // 把初始化配置发送给异步日志系统
            std::string config_log = "";
            config_log += "Initialize storage configuration:";
            config_log += "\nSERVER:";
            config_log += ("\n  server_ip: " + server_ip_);
            config_log += ("\n  server_port: " + std::to_string(server_port_));
            config_log += "\nSTORAGE_DIR: ";
            config_log += ("\n  download_prefix: " + download_prefix_);
            config_log += ("\n  deep_storage_dir: " +  deep_storage_dir_);
            config_log += ("\n  low_storage_dir: " +  low_storage_dir_);
            config_log += "\nFORMAT: ";
            config_log += ("\n  bundle_format: " + std::to_string(bundle_format_));
            config_log += ("\n  storage_info: " + storage_info_);

            file_.close();

            return {config_log, mylog::LogLevel::INFO};
        }

        std::string GetServerIp() const { return server_ip_; }
        unsigned int GetServerPort() const { return server_port_; }

        std::string GetDownLoadPrefix() const { return download_prefix_; }
        std::string GetDeepStorageDir() const { return deep_storage_dir_; }
        std::string GetLowStorageDir() const { return low_storage_dir_; }

        unsigned int GetBundleFormat() const { return bundle_format_; }
        std::string GetStorageInfo() const { return storage_info_; }

    };
}
#endif