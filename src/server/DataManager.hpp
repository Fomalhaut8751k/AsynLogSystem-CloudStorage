#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include "Config.hpp"
#include <unordered_map>
#include <pthread.h>


/*
    文件上传后会生成Storage.dat，其中的每个字段是json串格式
    包含了：文件最后一次访问时间，大小，修改时间，文件的存储路径，url下载地址

    Storage.dat就相当一个表，表中包含了每个存储文件的信息

    当程序启动后，会首先加载Storage.dat中曾经持久化的日志信息，当有新的文件被上传
    存储时，则使用json序列化方式插入新的StorageInfo
*/

namespace mystorage
{
    struct StorageInfo
    {
        time_t atime_;        // 文件最后一次访问时间
        time_t mtime_;        // 文件最后一次修改时间
        int64_t fsize_;              // 文件大小
        std::string storage_path_;          // 文件的存储路径
        std::string url_;                // url下载地址

        int NewStorageInfo(const std::string &storage_path)
        {
            mylog::GetLogger("default")->Log({"New storageinfo start",  mylog::LogLevel::INFO});
            FileUtil fu(storage_path);
            if(!fu.Exists())
            {
                mylog::GetLogger("default")->Log({"the file \"" + storage_path + "\" is not exist",  mylog::LogLevel::INFO});
                return -1;
            }
            atime_ = fu.LastAccessTime();
            mtime_ = fu.LastModifyTime();
            fsize_ = fu.FileSize();
            storage_path_ = storage_path;
            // url是用户下载文件请求的路径
            // 下载路径前缀+文件名
            mystorage::Config& config = mystorage::Config::GetInstance();
            url_ = config.GetDownLoadPrefix() + fu.FileName();
            std::string log = "";
            log += "New storageinfo start\n";
            log += "Storage Information: \n";
            log += ("  Download Url: " + url_ + "\n");
            log += ("  Storage Path: " + storage_path_ + "\n");
            log += ("  File Size: " + std::to_string(fsize_) + "\n");
            log += ("  Last Access Time: " + std::to_string(atime_) + "\n");
            log += ("  Last Modify Time: " + std::to_string(mtime_));
            mylog::GetLogger("default")->Log({log, mylog::LogLevel::INFO});
            
            return 0;
        }
    };

    
    class DataManager
    {
    private:
        std::string storage_file_;
        std::mutex mutex_;
        std::unordered_map<std::string, StorageInfo> table_;
        bool need_persist_;

    public:
        // 初始化，并加载storage.dat中原有的storageinfo
        DataManager()
        {
            mylog::GetLogger("default")->Log({"Initialize datamanager configuration",  mylog::LogLevel::INFO});
            mylog::GetLogger("default")->Log(Config::GetInstance().ReadConfig());
            // storage_file_ = "storage.dat";
            storage_file_ = mystorage::Config::GetInstance().GetStorageInfo();
            need_persist_ = false;
            InitLoad();
            need_persist_ = true;

        }

        ~DataManager() = default;


        // 初始化程序运行时从文件读取数据
        int InitLoad()
        {
            mystorage::FileUtil fu(storage_file_);
            if(!fu.Exists())  // 没有storage.data，就说明不需要加载
            {
                return -1;
            }
            mylog::GetLogger("default")->Log({"load storage data from " + storage_file_ , mylog::LogLevel::INFO});
            // 从storage.dat中读取内容
            std::string content;
            if(-1 == fu.GetContent(&content))
            {   
                mylog::GetLogger("default")->Log({"Initialize datamanager configuration failed!",  mylog::LogLevel::ERROR});
                return -1;
            }
            // 反序列化
            Json::Value root;
            mystorage::JsonUtil::UnSerialize(content, &root);  // string -> json
            for(int i = 0; i < root.size(); i++)
            {
                try
                {
                    StorageInfo info;
                    info.atime_ = root[i]["atime_"].asInt();
                    info.mtime_ = root[i]["mtime_"].asInt();
                    info.fsize_ = root[i]["fsize_"].asInt();
                    info.storage_path_ = root[i]["storage_path_"].asString();
                    info.url_ = root[i]["url_"].asString();
                    Insert(info);
                }
                catch(const std::exception& err)
                {
                    mylog::GetLogger("default")->Log({err.what(), mylog::LogLevel::ERROR});
                    mylog::GetLogger("default")->Log({"Initialize datamanager configuration failed!",  mylog::LogLevel::ERROR});
                    return -1;
                }
                
            }
            mylog::GetLogger("default")->Log({"Initialize datamanager configuration success",  mylog::LogLevel::ERROR});
            return 0;
        }
        
        // 每次有信息改变则需要持久化存储一次，把table_中的数据转成json格式存入文件
        int Storage()
        {
            mylog::GetLogger("default")->Log({"Storageinfo persistence start", mylog::LogLevel::INFO});
            
            // 先把现在table_的storageinfo读到一个数组中，通过GetAll()
            std::vector<StorageInfo> arry;
            GetAll(&arry);

            // 先转为json格式   
            Json::Value root;
            for(StorageInfo& info: arry)
            {
                Json::Value val;
                val["atime_"] = (Json::Int64)info.atime_;
                val["mtime_"] = (Json::Int64)info.mtime_;
                val["fsize_"] = (Json::Int64)info.fsize_;
                val["storage_path_"] = info.storage_path_.c_str();
                val["url_"] = info.url_.c_str();

                root.append(val);
            }

            // 序列化为字符串
            std::string body;
            if(JsonUtil::Serialize(root, &body) == -1)
            {
                mylog::GetLogger("default")->Log({"Storageinfo persistence failed", mylog::LogLevel::WARN});
                return -1;
            }

            // 写入文件
            FileUtil fu(storage_file_);
            if(fu.SetContent(body.c_str(), strlen(body.c_str())) == -1)
            {
                mylog::GetLogger("default")->Log({"Storageinfo persistence failed", mylog::LogLevel::WARN});
                return -1;
            }

            mylog::GetLogger("default")->Log({"Storageinfo persistence success", mylog::LogLevel::INFO});
            return 0;
        }

        
        // 将StorageInfo写入table_
        int Insert(const StorageInfo &info)
        {
            mylog::GetLogger("default")->Log({"Insert storageinfo start", mylog::LogLevel::INFO});
            {
                std::unique_lock<std::mutex> lock(mutex_);
                table_[info.url_] = info;
            }
            // 如果需要持久化就存储到storage.dat中，如果存储返回-1就说明出错了
            // Insert到这里已经把storageinfo存入table_，再Storage()就是把包括他的所有storageinfo保存到storage.dat
            if(need_persist_ && Storage() == -1) 
            {
                mylog::GetLogger("default")->Log({"Insert storageinfo fail when persistence", mylog::LogLevel::ERROR});
                return -1;
            }
            mylog::GetLogger("default")->Log({"Insert storageinfo success", mylog::LogLevel::INFO});
            return 0;  
        }


        int Update(const StorageInfo& info)
        {
            mylog::GetLogger("default")->Log({"Update storageinfo start", mylog::LogLevel::INFO});
            
            {
                std::unique_lock<std::mutex> lock(mutex_);
                table_[info.url_] = info;
            }
            if(Storage() == -1)
            {
                mylog::GetLogger("default")->Log({"Update storageinfo failed", mylog::LogLevel::INFO});
                return -1;
            }
            mylog::GetLogger("default")->Log({"Update storageinfo success", mylog::LogLevel::INFO});
            return 0;
        }

        // 通过url获取StorageInfo
        int GetOneByURL(const std::string& key, StorageInfo* info)  // url是key
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if(table_.find(key) == table_.end())
            {
                return -1;
            }
            *info = table_[key];
            return 0;
        }

        // 通过storage_path_获取StorageInfo
        int GetOneByStoragePath(const std::string &storage_path, StorageInfo* info)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            for(auto item: table_)
            {
                if(item.second.storage_path_ == storage_path)
                {
                    *info = item.second;
                    return 0;
                }
            }
            return -1;
        }

        // 获取所有的StorageInfo
        int GetAll(std::vector<StorageInfo> *arry)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            for(std::pair<std::string, StorageInfo> item: table_)
            {
                arry->emplace_back(item.second);
            }
            return 0;
        }

    };
}




#endif