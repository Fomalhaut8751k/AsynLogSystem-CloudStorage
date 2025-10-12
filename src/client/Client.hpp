#include <iostream>
#include <string>
#include <vector>
#include <curl/curl.h>
// #include <jsoncpp/json/json.h>
#include <fstream>
#include <sstream>
#include <memory>

class StorageClient {
private:
    std::string server_url_;
    
    // CURL 写回调函数
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response) {
        size_t total_size = size * nmemb;
        response->append((char*)contents, total_size);
        return total_size;
    }
    
    // 文件读回调函数
    static size_t ReadCallback(void* ptr, size_t size, size_t nmemb, void* stream) {
        std::ifstream* file = static_cast<std::ifstream*>(stream);
        file->read(static_cast<char*>(ptr), size * nmemb);
        return file->gcount();
    }
    
    // Base64 编码
    std::string Base64Encode(const std::string& input) {
        static const std::string base64_chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";
            
        std::string encoded;
        int i = 0;
        int j = 0;
        unsigned char char_array_3[3];
        unsigned char char_array_4[4];
        
        for (size_t n = 0; n < input.size(); n++) {
            char_array_3[i++] = input[n];
            if (i == 3) {
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;
                
                for(i = 0; i < 4; i++)
                    encoded += base64_chars[char_array_4[i]];
                i = 0;
            }
        }
        
        if (i) {
            for(j = i; j < 3; j++)
                char_array_3[j] = '\0';
                
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for (j = 0; j < i + 1; j++)
                encoded += base64_chars[char_array_4[j]];
                
            while(i++ < 3)
                encoded += '=';
        }
        
        return encoded;
    }
    
public:
    StorageClient(const std::string& server_url) : server_url_(server_url) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
    
    ~StorageClient() {
        curl_global_cleanup();
    }
    
    bool Upload(const std::string& file_path) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::cerr << "Failed to initialize CURL" << std::endl;
            return false;
        }
        
        // 获取文件名
        size_t last_slash = file_path.find_last_of("/\\");
        std::string filename = (last_slash != std::string::npos) ? 
                              file_path.substr(last_slash + 1) : file_path;
        
        std::string encoded_name = Base64Encode(filename);
        
        // 打开文件
        std::ifstream file(file_path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << file_path << std::endl;
            curl_easy_cleanup(curl);
            return false;
        }
        
        std::streamsize file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        // 准备 multipart 表单数据
        std::string boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
        std::string body;
        
        // 构造 multipart 数据
        body += "--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"file\"; filename=\"" + filename + "\"\r\n";
        body += "Content-Type: application/octet-stream\r\n\r\n";
        
        // 读取文件内容到 body
        std::vector<char> file_buffer(file_size);
        file.read(file_buffer.data(), file_size);
        body.append(file_buffer.data(), file_size);
        
        body += "\r\n--" + boundary + "--\r\n";
        
        file.close();
        
        // 设置 CURL 选项
        std::string response;
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, ("FileName: " + encoded_name).c_str());
        headers = curl_slist_append(headers, ("Content-Type: multipart/form-data; boundary=" + boundary).c_str());
        
        curl_easy_setopt(curl, CURLOPT_URL, (server_url_ + "/upload").c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        
        CURLcode res = curl_easy_perform(curl);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            std::cerr << "Upload failed: " << curl_easy_strerror(res) << std::endl;
            return false;
        }
        
        std::cout << "Upload response: " << response << std::endl;
        return true;
    }
    
    bool Download(const std::string& filename, const std::string& save_path) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::cerr << "Failed to initialize CURL" << std::endl;
            return false;
        }
        
        std::ofstream file(save_path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to create file: " << save_path << std::endl;
            curl_easy_cleanup(curl);
            return false;
        }
        
        // 设置 CURL 选项
        curl_easy_setopt(curl, CURLOPT_URL, (server_url_ + "/download/" + filename).c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, [](void* ptr, size_t size, size_t nmemb, void* stream) -> size_t {
            std::ofstream* file = static_cast<std::ofstream*>(stream);
            file->write(static_cast<char*>(ptr), size * nmemb);
            return size * nmemb;
        });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
        
        CURLcode res = curl_easy_perform(curl);
        file.close();
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            std::cerr << "Download failed: " << curl_easy_strerror(res) << std::endl;
            return false;
        }
        
        std::cout << "Downloaded to: " << save_path << std::endl;
        return true;
    }
    
    bool ListFiles() {
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::cerr << "Failed to initialize CURL" << std::endl;
            return false;
        }
        
        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, (server_url_ + "/list").c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            std::cerr << "List files failed: " << curl_easy_strerror(res) << std::endl;
            return false;
        }
        
        std::cout << "Files list: " << response << std::endl;
        return true;
    }
};