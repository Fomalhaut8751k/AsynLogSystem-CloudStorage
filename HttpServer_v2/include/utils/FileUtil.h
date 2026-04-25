#ifndef HTTPSERVER_V2_FILEUTIL_H
#define HTTPSERVER_V2_FILEUTIL_H

#include <cstdint>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace http_v2
{

class FileUtil
{
public:
    explicit FileUtil(std::string filepath):
        filePath_(std::move(filepath)),
        file_(filePath_, std::ios::binary)
    {
    }

    ~FileUtil()
    {
        if(file_.is_open()) file_.close();
    }

    bool isValid() const { return file_.is_open(); }

    uint64_t size()
    {
        if(!file_.is_open()) return 0;
        file_.seekg(0, std::ios::end);
        uint64_t fileSize = static_cast<uint64_t>(file_.tellg());
        file_.seekg(0, std::ios::beg);
        return fileSize;
    }

    bool readFile(std::vector<char>& buffer)
    {
        if(!file_.is_open()) return false;
        buffer.resize(size());
        if(buffer.empty()) return true;
        file_.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        return file_.good();
    }

private:
    std::string filePath_;
    std::ifstream file_;
};

}

#endif
