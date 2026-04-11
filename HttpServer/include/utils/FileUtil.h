#ifndef FILEUTIL_H
#define FILEUTIL_H

#include <fstream>
#include <string>
#include <vector>

#include "mymuduo/Alogger.h"

class FileUtil
{
public:
    FileUtil(std::string filepath):
        filePath_(filepath),
        file_(filepath, std::ios::binary)  // 打开文件，二进制
    {

    }

    ~FileUtil() { file_.close(); }

    // 判断是否是有效路径
    bool isValid() const { return file_.is_open(); }

    // 重置打开默认文件
    void resetDefaultFile()
    {
        file_.close();
        file_.open("/Gomoku/GomokuServer/resource/NotFound.html", std::ios::binary);
    }

    uint64_t size()
    {
        file_.seekg(0, std::ios::end);  // 定位到文件末尾
        uint64_t fileSize = file_.tellg();
        file_.seekg(0, std::ios::beg);  // 返回到文件头
        return fileSize;
    }

    void readFile(std::vector<char>& buffer)
    {
        if(file_.read(buffer.data(), size()))
        {
            logger_->INFO("File content load into memory (" + std::to_string(size()) + ") bytes");
        }
        else
        {
            logger_->ERROR("File read failed");
        }
    }

private:
    std::string filePath_;
    std::ifstream file_;
};

#endif