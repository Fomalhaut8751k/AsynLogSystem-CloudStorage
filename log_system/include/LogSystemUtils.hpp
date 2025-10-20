#ifndef LOGSYSTEMUTILS_H
#define LOGSYSTEMUTILS_H
/* Util.hpp包含一下功能：
    1. 获取时间  class Date
    2. 文件操作  class File
                  a 判断文件是否存在  Exist
                  b 从文件路径中获取文件名字 Path
                  c 创建目录  CreateDirectory
                  d 获取文件大小 FileSize
                  e 获取文件内容 GetContent
    3. Json操作  class JsonUtil
                  a 序列化  Serialize
                  b 反序列化 UnSerialize
*/
#pragma once
#include <ctime>
#include <fstream>
#include "json.hpp"
#include <sys/stat.h>
#include <sys/types.h>
#include <algorithm>

#include <iostream>
using std::cout;
using std::endl;

namespace mylog
{
    namespace Util
    {
        class Date
        {
        public:  
            // 返回时间
            static time_t Now() { return time(nullptr);}
        };

        class File
        {
        public:
            // 判断文件或者目录是否存在  
            static bool Exist(const std::string& filename)
            {
                struct stat st;
                return (stat(filename.c_str(), &st) == 0);
            }

            // 从文件路径中获取文件名字 
            static std::pair<std::string, std::string> Path(const std::string& file_path)
            {
                if(file_path.empty()) return {"", ""};
                
                // 找到最后一个路径分隔符
                size_t last_slash = file_path.find_last_of("/\\");
                
                if(last_slash == std::string::npos)
                {
                    return {"", file_path};  // 没有分隔符，返回整个字符串
                }
                else if(last_slash == file_path.length() - 1)
                {
                    return {"", ""};  // 分隔符在末尾，返回空字符串
                }
                else
                {
                    std::string file_name = file_path.substr(last_slash + 1);  // 返回最后一个分隔符之后的内容
                    std::string file_path_root = file_path.substr(0, last_slash+1);  // 路径
                    return {file_path_root, file_name};
                }
            }

            // 创建目录  
            static void CreateDirectory(const std::string& filename)
            {
                if(filename == "")
                {
                    perror("file path is empty");
                }
                // 文件不存在再创建
                if(!Exist(filename))
                {
                    std::string file_path = filename;
                    // 删除所有空格
                    file_path.erase(std::remove(file_path.begin(), file_path.end(), ' '), file_path.end());
                    
                    size_t pos, index = 0;
                    size_t size = filename.size();
                    
                    while(index < size)
                    {   // index为查找的起始位置，pos为index开始第一个/或者
                        pos = filename.find_first_of("/\\", index);
                        if(pos == std::string::npos)
                        {
                            mkdir(filename.c_str(), 0755);
                            return;
                        }
                        if(pos == index)
                        {
                            index = pos + 1;
                            continue;
                        }

                        std::string sub_path = filename.substr(0, pos);
                        if(sub_path == "." || sub_path == "..")
                        {
                            index = pos + 1;
                            continue;
                        }
                        if(Exist(sub_path))
                        {
                            index = pos + 1;
                            continue;
                        }

                        mkdir(sub_path.c_str(), 0755);
                        index = pos + 1;
                    }
                }
            }

            // 获取文件大小 
            static int64_t FileSize(const std::string& filename)
            {
                if(!Exist(filename))
                {
                    return -1;
                }
                struct stat st;
                if(stat(filename.c_str(), &st) == -1)
                {
                    perror("Get file size failed");
                    return -1;
                }
                return st.st_size;
            }

            // 获取文件内容
            static bool GetContent(std::string* content, std::string filename)  // content: 用于存储读取的文件内容
            {
                // 打开文件
                std::ifstream ifs;
                ifs.open(filename.c_str(), std::ios::binary);
                // 如果打开失败
                if(ifs.is_open() == false)
                {
                    std::cout << "file open error" << std::endl;
                    return false;
                }

                // 读入content
                ifs.seekg(0, std::ios::beg);  // 把指针移动到开头
                size_t len = FileSize(filename);
                content->resize(len);
                ifs.read(&(*content)[0], len);  // 从文件中读取长度为len的数据，从content地址头开始放
                if(!ifs.good())  // 检查流的状态是否良好
                {
                    std::cout << __FILE__ << __LINE__ << "-"
                          << "read file content error" << std::endl;
                    ifs.close();
                    return false;
                }
                return true;
            }
        };
    }
}

#endif