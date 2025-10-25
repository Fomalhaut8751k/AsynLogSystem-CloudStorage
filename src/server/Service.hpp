#ifndef SERVICE_H
#define SERVICE_H

#include <event.h>
#include <event2/listener.h>

#include <sys/queue.h>
#include <event.h>

#include <evhttp.h>
#include <event2/http.h>

#include <fcntl.h>
#include <sys/stat.h>

#include <regex>

#include "base64.h"
#include "StorageDataManager.hpp"

extern mystorage::DataManager* storage_data_;

namespace mystorage
{
    class StorageServer
    {
    private:
        uint16_t server_port_;
        std::string server_ip_;
        std::string download_prefix_;

        struct event_base* base_;
        sockaddr_in sin_;

        evhttp* httpd_;

        // 回调函数
        static void GenHandler(struct evhttp_request* req, void* arg)
        {
            std::string path = evhttp_uri_get_path(evhttp_request_get_evhttp_uri(req));
            path = UrlDecode(path);
            mylog::GetLogger("default")->Log({("get request, uri: " + path), mylog::LogLevel::INFO});

            // 根据请求中的内容判断是什么请求
            // 下载请求
            if(path.find("/download/") != std::string::npos)
            {
                Download(req, arg);
            }
            // 上传
            else if(path == "/upload")
            {
                Upload(req, arg);
            }
            // 显示已存储文件列表
            else if(path == "/")
            {
                ListShow(req, arg);
            }
            // 把文件列表发送给客户端
            else if(path == "/list")
            {
                ListShowForClient(req, arg);
            }
            // 删除文件
            else if(path == "/remove")
            {
                Remove(req, arg);
            }
            else
            {
                evhttp_send_reply(req, HTTP_NOTFOUND, "Not Found", NULL);
            }
        }

        // 上传文件
        static int Upload(struct evhttp_request* req, void* arg)
        {
            mylog::GetLogger("default")->Log({"Upload start", mylog::LogLevel::INFO});
            /*
                请求中包含"low_storage"，说明请求中存在文件数据，并希望普通存储
                请求中包含"deep_storage，则压缩后存储
            */
            struct evbuffer* buf = evhttp_request_get_input_buffer(req);
            if(nullptr == buf)
            {
                mylog::GetLogger("default")->Log({"Upload fail because evhttp_request_get_input_buffer is empty", mylog::LogLevel::WARN});
                return -1;
            }

            size_t len = evbuffer_get_length(buf);  // 获取请求体的长度
            // mylog::GetLogger("default")->Log({"evbuffer_get_length is " + std::to_string(len), mylog::LogLevel::INFO});
            if(0 == len)
            {
                evhttp_send_reply(req, HTTP_BADREQUEST, "file empty", NULL);
                mylog::GetLogger("default")->Log({"Upload fail because evbuffer_get_length is zero", mylog::LogLevel::WARN});
                return -1;
            }

            std::string content(len, 0);
            if(-1 == evbuffer_copyout(buf, (void*)content.c_str(), len))
            {
                evhttp_send_reply(req, HTTP_INTERNAL, NULL, NULL);
                mylog::GetLogger("default")->Log({"Upload fail because evbuffer_copyout error", mylog::LogLevel::WARN});
                return -1;
            }

            // 获取文件名
            std::string filename = evhttp_find_header(req->input_headers, "FileName");
            // 解码文件名
            filename = base64_decode(filename);

            // 获取存储类型，客户端自定义请求头，StorageInfo
            std::string storage_type;
            const char* storage_type_ = evhttp_find_header(req->input_headers, "StorageType");
            if(nullptr != storage_type_)
            {
                storage_type = storage_type_;
            }
            else
            {
                storage_type = "low";
            }
            // 组织存储路径
            std::string storage_path;
            if(storage_type == "deep")
            {
                storage_path = Config::GetInstance().GetDeepStorageDir();  // 读取自配置文件
            }
            else if(storage_type == "low")
            {
                storage_path = Config::GetInstance().GetLowStorageDir();  // 读取自配置文件
            }
            else
            {
                evhttp_send_reply(req, HTTP_BADREQUEST, "Illengal storage type", NULL);
                mylog::GetLogger("default")->Log({"Upload fail because illegal storage type", mylog::LogLevel::ERROR});
                return -1;
            }

            // 如果不存在low或者deep目录就创建
            FileUtil dirCreate(storage_path);
            dirCreate.CreateDirectory();

            // 目录后追加文件名就是最终要写入的文件目录
            storage_path += filename;

            FileUtil fu(storage_path);
            if(storage_path.find("low_storage") != std::string::npos)
            {
                // 往文件中写入内容
                if(-1 == fu.SetContent(content.c_str(), len))
                {
                    evhttp_send_reply(req, HTTP_INTERNAL, "server error", NULL);
                    mylog::GetLogger("default")->Log({"Upload fail because low storage SetContent error", mylog::LogLevel::ERROR});
                    return -1;
                }
                else
                {
                    mylog::GetLogger("default")->Log({"low storage success", mylog::LogLevel::INFO});
                }
            }
            else
            {   // 深度存储需要先压缩文件
                if(-1 == fu.Compress(content, Config::GetInstance().GetBundleFormat()))  // 包含了SetContent
                {
                    evhttp_send_reply(req, HTTP_INTERNAL, "server error", NULL);
                    mylog::GetLogger("default")->Log({"Upload fail because deep storage Compress error", mylog::LogLevel::ERROR});
                    return -1;
                }
                else
                {
                    mylog::GetLogger("default")->Log({"deep storage success", mylog::LogLevel::INFO});
                }
            }

            // 上传之后就要添加对应的StorageInfo信息
            StorageInfo info;
            info.NewStorageInfo(storage_path);
            storage_data_->Insert(info);

            struct evbuffer* output_buf = evhttp_request_get_output_buffer(req);
            std::string json_response = "{\"status\":\"success\",\"message\":\"File uploaded successfully\",\"filename\":\"" + filename + "\"}\n";
            evbuffer_add(output_buf, json_response.c_str(), json_response.size());
            evhttp_add_header(req->output_headers, "Content-Type", "application/json");

            evhttp_send_reply(req, HTTP_OK, "Success", NULL);
            mylog::GetLogger("default")->Log({"Upload finish", mylog::LogLevel::INFO});

            return 0;
        }

        static std::string TimetoStr(time_t t)
        {
            return std::ctime(&t);
        }

        // 前端代码处理函数
        // 在渲染函数中直接处理StorageInfo
        static std::string generateModernFileList(const std::vector<StorageInfo>& files)
        {
            std::stringstream ss;
            ss << "<div class='file-list'><h3>已上传文件</h3>";

            for(const auto& file: files)  // file的类型: StorageInfo
            {
                std::string filename = FileUtil(file.storage_path_).FileName();  // 通过StorageInfo中记录的文件的存储地址，获取文件的名字(../../filename.xx)

                // 从路径中解析存储类型(../deep/.., ../low/..)
                std::string storage_type = "low";
                if(file.storage_path_.find("deep") != std::string::npos)
                    storage_type = "deep";

                ss << "<div class='file-item'>"
                   << "<div class='file-info'>"
                   << "<span>📄" << filename << "</span>"
                   << "<span class='file-type'>"
                   << (storage_type == "deep" ? "深度存储" : "普通存储")
                   << "</span>"
                   << "<span>" << formatSize(file.fsize_) << "</span>"
                   << "<span>" << TimetoStr(file.mtime_) << "</span>"
                   << "</div>"
                   << "<button onclick=\"window.location='" << file.url_ << "'\">⬇️ 下载</button>"
                   << "</div>";

            }

            ss << "</div>";
            return ss.str();
        }

        // 文件大小格式化函数
        static std::string formatSize(uint64_t bytes)
        {
            /*
                为文件大小的数字(单位为B)进行格式化，例如：
                2048B -> 2KB
            */
            const char* units[] = {"B", "KB", "MB", "GB"};
            int uint_index = 0;
            double size = bytes;

            while(size >= 1024 && uint_index < 3)  // 最大单位为GB
            {
                size /= 1024;
                uint_index++;
            }

            std::stringstream ss;
            // std::fixed: 使用固定小数格式， std::setprecsion(2): 设置小数点后保留2位
            ss << std::fixed << std::setprecision(2) << size << " " << units[uint_index];
            return ss.str();
        }

        // 在浏览器展示所有的StorageInfo
        static int ListShow(struct evhttp_request* req, void* arg)
        {
            mylog::GetLogger("default")->Log({"ListShow()", mylog::LogLevel::INFO});
            
            // 获取所有的StorageInfo, 都存放在DataManager的table_中
            std::vector<StorageInfo> arry;
            if(storage_data_->GetAll(&arry) == -1)
            {
                mylog::GetLogger("default")->Log({"ListShow() fail when load storageinfo", mylog::LogLevel::ERROR});
            }

            // 读取模板文件
            std::ifstream templateFile("../src/server/index.html");
            std::string templateContent(
                (std::istreambuf_iterator<char>(templateFile)), 
                std::istreambuf_iterator<char>());

            // 替换html文件中的占位符
            // 替换文件列表进html
            templateContent = std::regex_replace(
                templateContent,
                std::regex("\\{\\{FILE_LIST\\}\\}"),  // 把这个占位符换成StorageInfo  
                generateModernFileList(arry)  // 加载所有的StorageInfo并渲染为前端代码
            );

            // 替换服务器地址进html
            templateContent = std::regex_replace(
                templateContent,
                std::regex("\\{\\{BACKEND_URL\\}\\}"),
                "http://"+mystorage::Config::GetInstance().GetServerIp()+":"+std::to_string(mystorage::Config::GetInstance().GetServerPort())
            );

            // 获取请求输出的evbuffer
            struct evbuffer* buf = evhttp_request_get_output_buffer(req);
            auto response_body = templateContent;

            // 把前面的html数据给到evbuffer，然后设置响应头部字段，最后返回给浏览器
            evbuffer_add(buf, (const void*)response_body.c_str(), response_body.size());
            evhttp_add_header(req->output_headers, "Content-Type", "text/html;charset=utf-8");
            evhttp_send_reply(req, HTTP_OK, NULL, NULL);
            mylog::GetLogger("default")->Log({"ListShow() finish", mylog::LogLevel::INFO});

            return 0;
        }

        static int ListShowForClient(struct evhttp_request* req, void* arg)
        {
            mylog::GetLogger("default")->Log({"ClientListFiles()", mylog::LogLevel::INFO});
    
            // 获取所有的StorageInfo
            std::vector<StorageInfo> arry;
            if(storage_data_->GetAll(&arry) == -1)
            {
                mylog::GetLogger("default")->Log({"ClientListFiles() fail when load storageinfo", mylog::LogLevel::ERROR});
                // 返回错误响应
                struct evbuffer* buf = evhttp_request_get_output_buffer(req);
                std::string error_response = "{\"status\":\"error\",\"message\":\"Failed to load file list\"}";
                evbuffer_add(buf, error_response.c_str(), error_response.size());
                evhttp_add_header(req->output_headers, "Content-Type", "application/json;charset=utf-8");
                evhttp_send_reply(req, HTTP_INTERNAL, NULL, NULL);
                return -1;
            }

            // 构建JSON响应
            std::string json_response = "{\"files\":[";
            
            for (size_t i = 0; i < arry.size(); ++i) {
                json_response += "\n\t{";
                json_response += arry[i].url_;
                json_response += "\t      ";
                json_response += formatSize(arry[i].fsize_);
                json_response += "}";
            }
            json_response += "\n]}";

            // 获取请求输出的evbuffer
            struct evbuffer* buf = evhttp_request_get_output_buffer(req);
            
            // 添加JSON数据到响应
            evbuffer_add(buf, json_response.c_str(), json_response.size());
            evhttp_add_header(req->output_headers, "Content-Type", "application/json;charset=utf-8");
            evhttp_send_reply(req, HTTP_OK, NULL, NULL);
            
            mylog::GetLogger("default")->Log({"ClientListFiles() finish, returned " + std::to_string(arry.size()) + " files", mylog::LogLevel::INFO});

            return 0;
        }

        static std::string GetETag(const StorageInfo& info)
        {
            // 自定义的etag： filename-fsize-mtime
            std::stringstream etag;
            FileUtil fu(info.storage_path_);
            etag << fu.FileName();
            etag << "-";
            etag << std::to_string(fu.FileSize());
            etag << "-";
            etag << TimetoStr(fu.LastModifyTime());
            return etag.str();
        }

        // 下载文件
        static int Download(struct evhttp_request* req, void* arg)
        {
            // 1. 获取客户端请求的资源路径path   req.path
            // 2. 根据资源路径，获取StorageInfo
            StorageInfo info;
            std::string resource_path = evhttp_uri_get_path(evhttp_request_get_evhttp_uri(req));

            resource_path = UrlDecode(resource_path);

            // 根据resource_path在tabel_中搜索对应的StorageInfo
            storage_data_->GetOneByURL(resource_path, &info);
            /*
                info.url_: /download/pcre-8.45.zip                文件将下载到客户端的这个位置?
                info.storageinfo_: ./low_storage/pcre-8.45.zip    文件存储在服务器的这个位置

                但是url_是table_的键？
            */
            // mylog::GetLogger("default")->Log({("request resource_path: %s", resource_path.c_str()), mylog::LogLevel::INFO});

            std::string download_path = info.storage_path_;
            // 如果是深度存储，压缩过的，就得先解压，把它放到low_storage中
            if(info.storage_path_.find(Config::GetInstance().GetDeepStorageDir()) != std::string::npos)
            {
                mylog::GetLogger("default")->Log({("uncompress: " + download_path), mylog::LogLevel::INFO});
                FileUtil fu(download_path);
                /*
                    info.url_: /download/pcre-8.45.zip   =>  pcre-8.45.zip
                    ./deep_storage/  +  pcre-8.45.zip  =  ./deep_storage/pcre-8.45.zip 
                */
                download_path = Config::GetInstance().GetLowStorageDir() + std::string(download_path.begin()
                    + download_path.find_last_of('/') + 1, download_path.end()
                );
                FileUtil dirCreate(Config::GetInstance().GetLowStorageDir());
                dirCreate.CreateDirectory();   // 之前可能用的都是深度存储，所以low_storage可能不存在
                
                // 把文件解压后放入low_storage中
                /*
                    fu创建的时候指向原来的download_path，即./deep_storage/xx.txt, 这个位置有确切的文件
                    new_download_path也就是后面的download_path是新创建的文件。
                    调用.Uncompress(new_download_path)就是读出./deep_storage/xx.txt中的内容，解压缩，
                    然后写入new_download_path中
                */
                fu.UnCompress(download_path);  // 把fu指向文件的内容解压后写入dirCreate执行文件的内容             
            }
            mylog::GetLogger("default")->Log({("request download_path: " + download_path), mylog::LogLevel::INFO});
            FileUtil fu(download_path);
            // deep storage中压缩文件存在，但是解压后的文件不存在，即压缩的时候出错
            if(-1 == fu.Exists() && info.storage_path_.find("deep_storage") != std::string::npos)
            {   
                mylog::GetLogger("default")->Log({"evhttp_send_reply: 500 - uncompress failed", mylog::LogLevel::INFO});
                evhttp_send_reply(req, HTTP_INTERNAL, NULL, NULL);
            }
            // low storage中文件不存在
            else if(-1 == fu.Exists() && info.storage_path_.find("low_storage") == std::string::npos)
            {
                mylog::GetLogger("default")->Log({"evhttp_send_reply: 400 - bad request, file not exists", mylog::LogLevel::INFO});
                evhttp_send_reply(req, HTTP_BADREQUEST, "file not exist", NULL);
            }   

            // 确认文件是否需要断点续传
            bool retrans = false;
            std::string old_etag;
            auto if_range = evhttp_find_header(req->input_headers, "If-Range");
            if(NULL != if_range)
            {
                old_etag = if_range;
                if(old_etag == GetETag(info))
                {
                    retrans = true;
                    mylog::GetLogger("default")->Log({(download_path + " need breakpoint continuous transmission"), mylog::LogLevel::INFO});
                }
            }

            // 读取文件数据，放入rsp.body中
            if(-1 == fu.Exists())
            {
                mylog::GetLogger("default")->Log({(download_path + " not exists"), mylog::LogLevel::WARN});
                download_path += "not exist";
                evhttp_send_reply(req, 404, download_path.c_str(), NULL);
                return -1;
            }
            evbuffer* outbuf = evhttp_request_get_output_buffer(req);
            int fd = open(download_path.c_str(), O_RDONLY);
            if(-1 == fd)
            {
                mylog::GetLogger("default")->Log({("open file error: " + download_path + " -- " + strerror(errno)), mylog::LogLevel::ERROR});
                evhttp_send_reply(req, HTTP_INTERNAL, strerror(errno), NULL);
                return -1;
            }
            
            if(-1 == evbuffer_add_file(outbuf, fd, 0, fu.FileSize()))
            {
                mylog::GetLogger("default")->Log({("evbuffer_add_file: " + std::to_string(fd) + " -- " + download_path + " -- " + strerror(errno)), mylog::LogLevel::ERROR});
                evhttp_send_reply(req, HTTP_INTERNAL, strerror(errno), NULL);
                return -1;
            }

            // 设置响应头部字段：ETag，Accept-Ranges: bytes
            evhttp_add_header(req->output_headers, "Accept-Ranges", "bytes");
            evhttp_add_header(req->output_headers, "ETag", GetETag(info).c_str());
            evhttp_add_header(req->output_headers, "Content-Type", "application/octet-stream");
            if(false == retrans)
            {
                evhttp_send_reply(req, HTTP_OK, "Success", NULL);
                mylog::GetLogger("default")->Log({"evhttp_send_reply: HTTP_OK", mylog::LogLevel::INFO});
            }
            else
            {
                evhttp_send_reply(req, 206, "breakpoint continuous transmission", NULL);
                mylog::GetLogger("default")->Log({"evhttp_send_reply: 206", mylog::LogLevel::INFO});
            }
            if(download_path != info.storage_path_)
            {
                remove(download_path.c_str());  // 删除文件
            }
            return 0;
        }

        // 删除文件
        static int Remove(struct evhttp_request* req, void* arg)
        {
            mylog::GetLogger("default")->Log({"Remove start", mylog::LogLevel::INFO});

            struct evbuffer* buf = evhttp_request_get_input_buffer(req);

            // 获取文件名
            std::string filename = evhttp_find_header(req->input_headers, "FileName");
            // 解码文件名
            filename = base64_decode(filename);

            mylog::GetLogger("default")->Log({"The file: " + filename + " will be remove", mylog::LogLevel::INFO});

            mystorage::StorageInfo info;
            if(storage_data_->GetOneByURL("/download/" + filename, &info) == -1)
            {
                mylog::GetLogger("default")->Log({"file is not exist", mylog::LogLevel::WARN});
                evhttp_send_reply(req, HTTP_OK, "0", NULL);
                return -1;
            }

            // 删除文件
            remove(info.storage_path_.c_str());
            // 从table_中删除对应的storageinfo并更新
            storage_data_->Erase(info.url_);
            // evhttp_send_reply(req, HTTP_OK, "0", NULL);

            struct evbuffer* output_buf = evhttp_request_get_output_buffer(req);
            std::string json_response = "{\"status\":\"success\",\"message\":\"File remove successfully\",\"filename\":\"" + filename + "\"}\n";
            evbuffer_add(output_buf, json_response.c_str(), json_response.size());
            // evhttp_add_header(req->output_headers, "Content-Type", "application/json");

            evhttp_send_reply(req, HTTP_OK, NULL, NULL);
            mylog::GetLogger("default")->Log({"Remove success", mylog::LogLevel::INFO});

            return 0;
        }

    public:
        StorageServer()
        {
            // server_ip_ = Config::GetInstance().GetServerIp();
            server_port_ = Config::GetInstance().GetServerPort();
            download_prefix_ = Config::GetInstance().GetDownLoadPrefix();
        }

        // 初始化设置
        int InitializeConfiguration()
        {
            // mylog::GetLogger("default")->Log({"Initialize storage server configuration", mylog::LogLevel::INFO});
            base_ = event_base_new();
            if(!base_)
            {   
                mylog::GetLogger("default")->Log({"Initialize terminate when event_base_new", mylog::LogLevel::ERROR});
                return -1;
            }

            memset(&sin_, 0, sizeof(sin_));
            sin_.sin_family = AF_INET;
            sin_.sin_port = htons(server_port_);

            httpd_ = evhttp_new(base_);
            if(evhttp_bind_socket(httpd_, "0.0.0.0", server_port_) != 0)
            {
                mylog::GetLogger("default")->Log({"Initialize terminate when evhttp_bind_socket", mylog::LogLevel::ERROR});
                return -1;
            }
            evhttp_set_gencb(httpd_, GenHandler, NULL);

            return 0;
        }

        // 启动
        int PowerUp()
        {
            int ret = 0;
            if(base_)
            {
                mylog::GetLogger("default")->Log({"Power up storage server", mylog::LogLevel::INFO});
                // 启动事件循环
                if(-1 == event_base_dispatch(base_))
                {
                    mylog::GetLogger("default")->Log({"Power up fail when event_base_dispatch", mylog::LogLevel::ERROR});
                    mylog::GetLogger("default")->Log({"Power off storage server", mylog::LogLevel::INFO});
                    ret = -1;
                }
                mylog::GetLogger("default")->Log({"Power off storage server", mylog::LogLevel::INFO});
            }
            if(base_)
            {
                event_base_free(base_);
            }
            if(httpd_)
            {
                evhttp_free(httpd_);
            }
            
            return ret;
        }
    };

}
#endif