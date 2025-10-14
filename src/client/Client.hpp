#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <memory>
#include <cstdlib>
#include <algorithm>
#include <iomanip>

#include "base64.h"

class StorageClient {
private:
    std::string server_url_;
     
    
    // 执行 wget 命令并返回结果
    int ExecuteWget(const std::string& command) {
        std::cout << "Executing: " << command << std::endl;
        return system(command.c_str());
    }
    
    // URL 编码（简单版本）
    std::string UrlEncode(const std::string& value) {
        std::ostringstream escaped;
        escaped.fill('0');
        escaped << std::hex;
        
        for (char c : value) {
            // 保留字母数字和一些特殊字符
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                escaped << c;
                continue;
            }
            
            // 其他字符进行百分比编码
            escaped << '%' << std::setw(2) << int((unsigned char)c);
        }
        
        return escaped.str();
    }
    
public:
    StorageClient(const std::string& server_url) : server_url_(server_url) {}
    
    // 上传文件 - 使用 wget
    bool Upload(const std::string& file_path, const std::string& storage_type) {
        // 获取文件名
        size_t last_slash = file_path.find_last_of("/\\");
        std::string filename = (last_slash != std::string::npos) ? 
                              file_path.substr(last_slash + 1) : file_path;
        
        std::string encoded_name = base64_encode(filename);
        
        // 构建 wget 命令
        std::string command = "wget --post-file=\"" + file_path + "\" ";
        command += "--header=\"FileName: " + encoded_name + "\" ";
        command += "--header=\"StorageType: " + storage_type + "\" ";
        command += "--header=\"Content-Type: application/octet-stream\" ";
        command += "-O /tmp/upload_response.txt ";  // 保存响应到临时文件
        command += "\"" + server_url_ + "/upload\" ";
        command += "2>/dev/null";  // 隐藏错误输出
        
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
        std::string encoded_filename = UrlEncode(filename);
        
        std::string command = "wget --progress=bar:force ";
        command += "-O \"" + save_path + "\" ";
        command += "\"" + server_url_ + "/download/" + encoded_filename + "\"";
        
        std::cout << "Downloading: " << filename << std::endl;
        int result = system(command.c_str());
        
        if (result == 0) {
            // 检查文件大小
            std::ifstream file(save_path, std::ios::binary | std::ios::ate);
            if (file.is_open()) {
                std::streamsize size = file.tellg();
                file.close();
                std::cout << "Successfully downloaded: " << save_path 
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

        // std::cerr << return_code << " || " << result << std::endl;
        // Json::Value response;
        // mystorage::JsonUtil::UnSerialize(result, &response);
        
        // if ("success" == response["status"].asString()) {
        //     std::cout << "Remove file: " + filename + " success" << std::endl;
        //     return true;
        // } else {
        //     std::cerr << "Remove file: " + filename + " failed" << std::endl;
        //     return false;
        // }

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