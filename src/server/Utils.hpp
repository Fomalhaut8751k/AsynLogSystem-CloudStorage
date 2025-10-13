#ifndef UTILS_H
#define UTILS_H

#include "json.hpp"
#include "jsoncpp/json/json.h"

#include <cassert>
#include <sstream>
#include <memory>

#include "bundle.h"
#include "Config.hpp"

#include <iostream>
#include <experimental/filesystem>
#include <string>
#include <sys/stat.h>
#include <vector>
#include <fstream>
#include "../../log_system/include/MyLog.hpp"   

// 因为是基于异步日志系统的，所以这里操作有什么问题也要通过日志器生产对应的日志

namespace mystorage
{
    namespace fs = std::experimental::filesystem;

    // 进制转化 12->12+55=67->c; 7->7+48=7   
    static unsigned char ToHex(unsigned char x)
    {
        return x > 9 ? x + 55: x + 48;
    }

    //  进制转化（与上面的相反）
    static unsigned char FromHex(unsigned char x)
    {
        unsigned char y;
        if(x >= 'A' && x <= 'Z')
        {
            y = x - 'A' + 10;
        }
        else if(x >= 'a' && x <= 'z') 
        {
            y = x - 'a' + 10;
        }
        else if(x >= '0' && x <= '9')
        {
            y = x - '0';
        }
        else
        {
            assert(0);
        }
        return y;
    }

    static std::string UrlDecode(const std::string& str)
    {
        std::string strTemp = "";
        size_t length = str.length();
        for(size_t i = 0; i < length; i++)
        {
            if(str[i] == '%')
            {
                assert(i + 2 < length);
                unsigned char high = FromHex((unsigned char)str[++i]);
                unsigned char low = FromHex((unsigned char)str[++i]);
                strTemp += high * 16 + low;
            }
            else
            {
                strTemp += str[i];
            }
        }
        return strTemp;
    }

    class FileUtil
    {
    private:
        std::string filename_;  // 不仅仅是文件名，是整个文件路径

    public:
        FileUtil(const std::string filename): filename_(filename){}

        // 获取文件的大小
        int64_t FileSize()
        {
            if(!Exists())
            {
                return -1;
            }
            struct stat st;
            if(stat(filename_.c_str(), &st) == -1)
            {
                mylog::GetLogger("default")->Log({"get file size failed",  mylog::LogLevel::WARN});
                return -1;
            }
            return st.st_size;
        }

        // 获取文件最近访问时间
        time_t LastAccessTime()
        {
            if(!Exists())
            {
                return -1;
            }
            struct stat st;
            if(stat(filename_.c_str(), &st) == -1)
            {
                mylog::GetLogger("default")->Log({"get file access time failed",  mylog::LogLevel::WARN});
                return -1;
            }
            return st.st_atime;
        }

        // 获取文件最近修改时间
        time_t LastModifyTime()
        {
            if(!Exists())
            {
                return -1;
            }
            struct stat st;
            if(stat(filename_.c_str(), &st) == -1)
            {
                mylog::GetLogger("default")->Log({"get file motify time failed",  mylog::LogLevel::WARN});
                return -1;
            }
            return st.st_mtime;
        }

        // 从路径中解析出文件名
        std::string FileName()
        {
            if(filename_.empty()){ return ""; }

            // 找到最后一个路径分隔符
            size_t last_slash = filename_.find_last_of("/\\");

            if(last_slash == std::string::npos) { return filename_; }  // 没有分隔符，filename_就是文件名
            else if(last_slash == filename_.length() - 1)  // 分隔符在末尾，说明是个目录
            {
                mylog::GetLogger("default")->Log({"the filename is a directory, not file",  mylog::LogLevel::WARN});
                return "";
            }
            else { return filename_.substr(last_slash + 1);}  
        }

        // 从文件POS处获取len长度字符给content
        int GetPosLen(std::string* content, size_t pos, size_t len)
        {
            // 先判断是否存在
            if(!Exists())
            {
                mylog::GetLogger("default")->Log({"the file \" " + \
                                 filename_  + "\" is not exist", mylog::LogLevel::ERROR});
                return -1;
            }
            // 打开文件
            std::ifstream file;
            file.open(filename_.c_str(), std::ios::binary);
            if(!file.is_open())
            {
                mylog::GetLogger("default")->Log({"the file \" " + \
                                 filename_  + "\" open failed", mylog::LogLevel::ERROR});
                return -1;
            }
            
            file.seekg(pos, std::ios::beg);  // 定位到指定位置
            content->resize(len);
            file.read(&(*content)[0], len);

            if(!file.good())
            {
                mylog::GetLogger("default")->Log({"read file content failed", mylog::LogLevel::ERROR});
                file.close();
                return -1;
            }

            file.close();
            return 0;
        }

        // 获取文件内容
        int GetContent(std::string *content)
        {
            return GetPosLen(content, 0, FileSize());
        }

        // 写文件
        int SetContent(const char* content, size_t len)
        {
            std::ofstream file(filename_.c_str(), std::ios::binary);
            if(!file.is_open())
            {
                mylog::GetLogger("default")->Log({"file \" " +  filename_  + "\" open failed", mylog::LogLevel::WARN});
                return -1;
            }
            file.write(content, len);
            if(!file.good())
            {
                mylog::GetLogger("default")->Log({"file \" " +  filename_  + "\" set content error", mylog::LogLevel::WARN});
            }
            file.close();
            return 0;
        }


        // 压缩文件
        int Compress(std::string& content, int format)
        {
            std::string packed = bundle::pack(format, content);  // 压缩后的数据
            if(packed.size() == 0)
            {
                mylog::GetLogger("default")->Log({"compress packed size error", mylog::LogLevel::WARN});
                return -1;
            }
            FileUtil f(filename_);
            if(f.SetContent(packed.c_str(), packed.size()) == -1)   // 写入压缩包文件
            {
                mylog::GetLogger("default")->Log({"file " + filename_ + \
                        " compress packed size error: " + std::to_string(packed.size()), mylog::LogLevel::WARN});
                return -1;
            }
            return 0;
        }

        // 解压文件
        int UnCompress(std::string& download_path)
        {
            std::string body;
            if(this->GetContent(&body) == -2)
            {
                mylog::GetLogger("default")->Log({"file " + filename_ + \
                        " uncompress get file content error: ", mylog::LogLevel::WARN});
                return -1;
            }
            // 把压缩包中的内容读出来并解压
            std::string unpacked = bundle::unpack(body);
            FileUtil fu(download_path);

            // 把解压后的内容写到download_path中
            if(fu.SetContent(unpacked.c_str(), unpacked.size()) == -1)
            {
                mylog::GetLogger("default")->Log({"file " + filename_ + \
                        " uncompress write packed data error: ", mylog::LogLevel::WARN});
                return -1;
            }
            return 0;
        }

        // 判断目录或文件是否存在
        bool Exists()
        {
            return fs::exists(filename_);
        }

        // 创建目录
        int CreateDirectory()
        {
            if(Exists()) { return 0;}
            return fs::create_directories(filename_);
        }

        // 扫描目录
        int ScanDirectory(std::vector<std::string>* arry)
        {
            unsigned int cnt = 0;
            for(auto &p: fs::directory_iterator(filename_))
            {
                if(fs::is_directory(p)) { continue; }
                arry->push_back(fs::path(p).relative_path().string());
                cnt++;
            }
            return cnt;
        }
    };

    class JsonUtil
    {
    private:

    public:
        // 序列化
        static int Serialize(const Json::Value &val, std::string *str)
        {
            // 建造者生产->建造者实例化json写对象->调用写对象中的接口进行序列化写入str
            Json::StreamWriterBuilder swb;
            swb["emitUTF8"] = true;
            std::unique_ptr<Json::StreamWriter> usw(swb.newStreamWriter());
            std::stringstream ss;
            if(usw->write(val, &ss) != 0)
            {
                mylog::GetLogger("default")->Log({"serialize error", mylog::LogLevel::WARN});
                return -1;
            }
            *str = ss.str();
            return 0;
        }

        // 反序列化
        static int UnSerialize(const std::string &str, Json::Value* val)
        {
            Json::CharReaderBuilder crb;
            std::unique_ptr<Json::CharReader> ucr(crb.newCharReader());
            std::string err;
            if(ucr->parse(str.c_str(), str.c_str() + str.size(), val, &err) == false)
            {
                mylog::GetLogger("default")->Log({"unserialize error", mylog::LogLevel::WARN});
                return -1;
            }
            return 0;
        }
    };
}

#endif