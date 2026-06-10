#ifndef STORAGESERVER_H
#define STORAGESERVER_H

// #include "../../HttpServer/include/http/HttpRequest.h"
// #include "../../HttpServer/include/http/HttpResponse.h"
// #include "../../HttpServer/include/http/HttpServer.h"
#include "../../HttpServer_v2/include/http/HttpRequest.h"
#include "../../HttpServer_v2/include/http/HttpResponse.h"
#include "../../HttpServer_v2/include/http/HttpServer.h"

// #include "../../HttpServer/include/router/RouterHandler.h"
#include "../../HttpServer_v2/include/router/RouterHandler.h"
#include "../../HttpServer_v2/include/middleware/cors/CorsMiddleware.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <regex>
#include <map>

// #include <mymuduo/Logger.h>

#include "../../thirdparty/base64.h"
#include "StorageDataManager.hpp"

extern mystorage::DataManager* storage_data_;
extern std::string logger_name_;

// using namespace http;
namespace http = http_v2;
using namespace http_v2;

namespace mystorage
{
    class CloudStorageServer{
    private:
        std::string download_prefix_;

        // 将时间戳转为字符串格式
        std::string TimetoStr(time_t t){
            return std::ctime(&t);
        }

        // 文件大小格式化函数
        static std::string formatSize(uint64_t bytes){
            /*
                为文件大小的数字(单位为B)进行格式化，例如：
                2048B -> 2KB
            */
            const char* units[] = {"B", "KB", "MB", "GB"};
            int uint_index = 0;
            double size = bytes;

            // while(size >= 1024 && uint_index < 3)  // 最大单位为GB
            while(size >= 1000 && uint_index < 3)  // 使用网页/磁盘常见的十进制单位
            {
                // size /= 1024;
                size /= 1000;
                uint_index++;
            }

            std::stringstream ss;
            // std::fixed: 使用固定小数格式， std::setprecsion(2): 设置小数点后保留2位
            ss << std::fixed << std::setprecision(2) << size << " " << units[uint_index];
            return ss.str();
        }
    
        // 前端代码处理函数，在渲染函数中直接处理StorageInfo
        /*
            std::string generateModernFileList(const std::vector<StorageInfo>& files){
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

        */
        
        // 目录树节点：dirs 是子目录(名字->节点)，files 是本层直属文件
        struct TreeNode {
            std::map<std::string, TreeNode> dirs;
            std::vector<const StorageInfo*> files;
        };

        // 按 rel_path_ 把文件插入目录树：逐段下钻目录，最后一段为文件名挂到当前层
        void insertToTree(TreeNode& root, const StorageInfo* file){
            const std::string& rp = file->rel_path_;
            TreeNode* cur = &root;
            size_t start = 0;
            while(true){
                size_t sep = rp.find('/', start);
                if(sep == std::string::npos){
                    cur->files.push_back(file);
                    break;
                }
                cur = &cur->dirs[rp.substr(start, sep - start)];
                start = sep + 1;
            }
        }

        // 递归渲染一层目录内容：先渲染可折叠子目录，再渲染本层文件。
        // prefix 为当前层完整相对路径(带尾部 '/'，根层为空)，用于"下载文件夹"按钮的标识。
        void renderTreeChildren(std::stringstream& ss, const TreeNode& node, const std::string& prefix){
            for(const auto& kv : node.dirs){
                std::string full = prefix + kv.first;  // 该子目录的完整相对路径
                ss << "<div class='folder-group'>"
                   << "<div class='folder-header' onclick='toggleFolder(this)'>"
                   << "<span class='folder-toggle'>&#9654;</span>"
                   << "<span class='folder-name'>&#128193; " << kv.first << "</span>"
                   << "<button class='btn-download' onclick=\"event.stopPropagation(); downloadFolder('"
                   << escapeJs(full) << "')\">&#128230; 下载文件夹</button>"
                   << "<button class='btn-delete' onclick=\"event.stopPropagation(); deleteFolder('"
                   << escapeJs(full) << "')\">&#128465; 删除文件夹</button>"
                   << "</div>"
                   << "<div class='folder-children' style='display:none;'>";
                renderTreeChildren(ss, kv.second, full + "/");
                ss << "</div></div>";
            }
            for(const StorageInfo* file : node.files){
                renderFileItem(ss, *file);
            }
        }

        std::string generateModernFileList(const std::vector<StorageInfo>& files){
            std::stringstream ss;
            ss << "<div class='file-list'><h3>已上传文件</h3>";

            // 构建嵌套目录树后递归渲染：根级文件平铺，文件夹可展开/折叠
            TreeNode root;
            for(const auto& file : files){
                insertToTree(root, &file);
            }
            renderTreeChildren(ss, root, "");

            ss << "</div>";
            return ss.str();
        }

        // 渲染单个文件条目，下载/删除均以 rel_path_ 作为标识
        void renderFileItem(std::stringstream& ss, const StorageInfo& file){
            std::string rel_path = file.rel_path_;
            std::string display = FileUtil(file.storage_path_).FileName();

            std::string storage_type = "low";
            if(file.storage_path_.find("deep") != std::string::npos)
                storage_type = "deep";

            uint64_t file_size_bytes = file.fsize_;

            ss << "<div class='file-item'>"
            << "<div class='file-info'>"
            << "<span>📄 " << display << "</span>"
            << "<span class='file-type'>"
            << (storage_type == "deep" ? "深度存储" : "普通存储")
            << "</span>"
            << "<span class='file-size'>💾 " << formatSize(file_size_bytes) << "</span>"
            << "<span class='file-time'>🕒 " << TimetoStr(file.mtime_) << "</span>"
            << "</div>"
            << "<div class='button-group'>"
            << "<button class='btn-download' onclick=\"downloadFile('" << escapeJs(rel_path) << "', '" << file_size_bytes << "')\">📥 下载</button>"
            << "<button class='btn-delete' onclick=\"deleteFile('" << escapeJs(rel_path) << "')\">🗑️ 删除</button>"
            << "</div>"
            << "</div>";
        }

        // 辅助函数：转义 JavaScript 字符串
        std::string escapeJs(const std::string& str) {
            std::string result;
            for (char c : str) {
                if (c == '\'') result += "\\'";
                else if (c == '"') result += "\\\"";
                else if (c == '\\') result += "\\\\";
                else if (c == '\n') result += "\\n";
                else if (c == '\r') result += "\\r";
                else result += c;
            }
            return result;
        }

        // 校验客户端传来的相对路径是否安全，防止目录穿越:
        // 拒绝空串、绝对路径(以 / 开头)、含 ".." 的路径
        static bool IsSafeRelPath(const std::string& rel_path) {
            if (rel_path.empty()) return false;
            if (rel_path.front() == '/') return false;
            if (rel_path.find("..") != std::string::npos) return false;
            return true;
        }

        // ETAG协商缓存
        std::string GetETag(const StorageInfo& info){
            // 自定义的etag： filename-fsize-mtime
            std::stringstream etag;
            FileUtil fu(info.storage_path_);
            etag << fu.FileName();
            etag << "-";
            etag << std::to_string(fu.FileSize());
            etag << "-";
            // etag << TimetoStr(fu.LastModifyTime());
            // ctime() 会带尾部换行，写入 HTTP 头会破坏响应头边界，导致浏览器把后续头当成文件内容。
            etag << std::to_string(fu.LastModifyTime());
            return etag.str();
        } 

        class GenHandler: public http::router::RouterHandler{
        public:
            explicit GenHandler(CloudStorageServer* server): server_(server){}

            void handle(const http::HttpRequest& req, http::HttpResponse* resp) override{
                std::string path = req.path();
                path = UrlDecode(path);
                mylog::GetLogger(logger_name_)->Info("get request, uri: " + path);

                // 根据请求中的内容判断是什么请求
                if(path.find("/download_folder") != std::string::npos){  // 文件夹打包下载
                    server_->DownloadFolder(req, resp);
                }else if(path.find("/download") != std::string::npos){  // 下载
                    server_->Download(req, resp);
                }else if(path == "/upload"){  // 上传
                    server_->Upload(req, resp);
                }else if(path == "/"){  // 显示已存储文件列表
                    server_->ListShow(req, resp);
                }else if(path == "/list"){  // 把文件列表发送给客户端
                    
                }else if(path == "/remove_folder"){  // 删除整个文件夹(含子目录与文件)
                    server_->RemoveFolder(req, resp);
                }else if(path == "/remove"){  // 删除文件
                    server_->Remove(req, resp);
                }else{

                }
            }

        private:
            CloudStorageServer* server_;
        };
    
    public:
        // CloudStorageServer(int port, const std::string& name,
        //             TcpServer::Option option = TcpServer::kNoReusePort):
        //     httpServer_(port, name, true, option){
        CloudStorageServer(int port, const std::string& name,
                    http::HttpServer::Option option = http::HttpServer::kNoReusePort):
            httpServer_(port, name, false, option){
            initialize();
        }

        void setThreadNum(int numThreads){
            httpServer_.setThreadNum(numThreads);
        }

        void start(){
            httpServer_.start();
        }

        // 上传文件
        int Upload(const http::HttpRequest& req, http::HttpResponse* resp){
            mylog::GetLogger(logger_name_)->Info("Upload start");

            // 获取文件名
            std::string filename = req.getHeader("FileName");
            filename = base64_decode(filename);

            // 文件夹上传：相对路径放在 RelPath 头(base64)，保留目录层级；
            // 不存在时回退到 FileName(basename)，兼容单文件上传
            std::string rel_path = req.getHeader("RelPath");
            if(!rel_path.empty()){
                rel_path = base64_decode(rel_path);
                if(!IsSafeRelPath(rel_path)){
                    resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "ILLEGAL REL PATH");
                    mylog::GetLogger(logger_name_)->Error("Upload fail because illegal rel path: " + rel_path);
                    return -1;
                }
            }else{
                rel_path = filename;
            }

            // 获取请求体中的文件内容和长度
            std::string content = req.getBody();
            // size_t len = std::stoi(req.getHeader("Content-Length"));  如果是分块写入，那就不能是content-length
            size_t len = content.size();
            // 空文件(如 __init__.py / .gitkeep)是合法的：仅当声明了非零长度却收到空体时，
            // 才视为上传被截断而拒绝；客户端本就声明 Content-Length:0 的空文件放行。
            if(0 == len && req.contentLength() > 0){
                resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "FILE EMPTY");
                mylog::GetLogger(logger_name_)->Warn("Upload fail because body is empty but content-length is non-zero");
                return -1;
            }

            // 获取存储类型，客户端自定义请求头，StorageInfo
            std::string storage_type = req.getHeader("StorageType");
  
            // 组织存储路径
            std::string storage_path;
            if(storage_type == "deep"){
                storage_path = "./deep_storage/";
            }else if(storage_type == "low"){
                storage_path = "./low_storage/";
            }else{
                resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "ILLEGAL STORAGE TYPE");
                mylog::GetLogger(logger_name_)->Error("Upload fail because illegal storage type");
                return -1;
            }

            // 如果不存在low或者deep目录就创建
            FileUtil dirCreate(storage_path);
            dirCreate.CreateDirectory();

            // 目录后追加相对路径就是最终要写入的文件目录(保留文件夹层级)
            storage_path += rel_path;
            FileUtil fu(storage_path);
            // 若相对路径包含子目录，先递归创建其父目录
            size_t last_sep = storage_path.find_last_of('/');
            if(last_sep != std::string::npos){
                FileUtil subDirCreate(storage_path.substr(0, last_sep));
                subDirCreate.CreateDirectory();
            }
            // 浅度存储
            if(storage_path.find("low_storage") != std::string::npos){
                // 往文件中写入内容
                if(req.getBody().size() < req.contentLength()){  // 如果是分块写入
                    if(-1 == fu.SetContentApp(content.c_str(), len)){ 
                        resp->setStatusLine(req.getVersion(), http::HttpResponse::k500InternalServerError, "SERVER ERROR");
                        mylog::GetLogger(logger_name_)->Error("Upload fail because low storage SetContent error");
                        return -1;
                    }else{
                        mylog::GetLogger(logger_name_)->Info("low storage success");
                    }
                }else{
                    if(-1 == fu.SetContent(content.c_str(), len)){ 
                        resp->setStatusLine(req.getVersion(), http::HttpResponse::k500InternalServerError, "SERVER ERROR");
                        mylog::GetLogger(logger_name_)->Error("Upload fail because low storage SetContent error");
                        return -1;
                    }else{
                        mylog::GetLogger(logger_name_)->Info("low storage success");
                    }
                }
            }
            // 深度存储
            else{
                // 空文件无需压缩(bundle::pack 对空内容返回空串会被 Compress 判定为失败)，直接写空文件
                if(0 == len){
                    if(-1 == fu.SetContent("", 0)){
                        resp->setStatusLine(req.getVersion(), http::HttpResponse::k500InternalServerError, "SERVER ERROR");
                        mylog::GetLogger(logger_name_)->Error("Upload fail because deep storage empty file SetContent error");
                        return -1;
                    }else{
                        mylog::GetLogger(logger_name_)->Info("deep storage empty file success");
                    }
                }
                else if(-1 == fu.Compress(content, 4)){
                    resp->setStatusLine(req.getVersion(), http::HttpResponse::k500InternalServerError, "SERVER ERROR");
                    mylog::GetLogger(logger_name_)->Error("Upload fail because deep storage SetContent error");
                    return -1;
                }else{
                    mylog::GetLogger(logger_name_)->Info("deep storage success");
                }
            }

            // 上传之后就要添加对应的StorageInfo信息
            StorageInfo info;
            info.NewStorageInfo(storage_path, rel_path);
            storage_data_->Insert(info);
            
            json successResp;
            successResp["status"] = "success";
            successResp["message"] = "File uploaded successfully, filename: " + filename;
            resp->addHeader("Content-Type", "application/json");
            resp->setBody("{\"status\":\"success\",\"message\":\"File uploaded successfully\",\"filename\":\"" + filename + "\"}\n");
            
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
            resp->setContentType("text/html;charset=utf-8");
            
            resp->setCloseConnection(false);
            
            mylog::GetLogger(logger_name_)->Info("Upload finishs");
            return 0;
        }

        // 在浏览器展示所有的StorageInfo
        int ListShow(const http::HttpRequest& req, http::HttpResponse* resp){
            mylog::GetLogger(logger_name_)->Info("ListShow()");

            std::vector<StorageInfo> arry;  // 获取所有的StorageInfo, 都存放在DataManager的table_中
            if(storage_data_->GetAll(&arry) == -1){
                mylog::GetLogger(logger_name_)->Error("ListShow() fail when load storageinfo");
            }

            // 读取模板文件
            std::ifstream templateFile("../src/server/index.html");
            std::string templateContent(
                (std::istreambuf_iterator<char>(templateFile)), 
                std::istreambuf_iterator<char>()
            );

            // 替换html文件中的占位符, 替换文件列表进html
            templateContent = std::regex_replace(
                templateContent,
                std::regex("\\{\\{FILE_LIST\\}\\}"),  // 把这个占位符换成StorageInfo  
                generateModernFileList(arry)  // 加载所有的StorageInfo并渲染为前端代码
            );

            // 替换服务器地址进html
            templateContent = std::regex_replace(
                templateContent,
                std::regex("\\{\\{BACKEND_URL\\}\\}"),
                // "http://"+mystorage::Config::GetInstance().GetServerIp()+":"+std::to_string(mystorage::Config::GetInstance().GetServerPort())
                ""
            );

            resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
            resp->setContentType("text/html;charset=utf-8");
            resp->setContentLength(templateContent.size());
            resp->setBody(templateContent);

            // mylog::GetLogger("default")->Log({"ListShow() finish", mylog::LogLevel::INFO});
            mylog::GetLogger(logger_name_)->Info("ListShow() finish");
            resp->setCloseConnection(false);

            return 0;
        }

        int ListShowForClient(const http::HttpRequest& req, http::HttpResponse* resp){

            return 0;
        }

        // 下载文件
        int Download(const http::HttpRequest& req, http::HttpResponse* resp){
            mylog::GetLogger(logger_name_)->Info("Download start");

            std::string encoded_file_name = req.getHeader("FileName");
            if(encoded_file_name.empty()){
                encoded_file_name = UrlDecode(req.getQueryParameters("filename"));
            }
            std::string file_name = base64_decode(encoded_file_name);

            std::string file_size_str = req.getHeader("FileSize");
            if(file_size_str.empty()){
                file_size_str = req.getQueryParameters("filesize");
            }
            std::uint64_t file_size = file_size_str.empty() ? 0 : std::stoull(file_size_str);

            std::string resource_path = UrlDecode(req.path()) + "/" + file_name;
            mylog::GetLogger(logger_name_)->Info("resource path: " + resource_path);

            StorageInfo info;
            storage_data_->GetOneByURL(resource_path, &info);
            std::string download_path = info.storage_path_;

            // 如果是深度存储，先解压到临时文件
            bool is_temp = false;
            if(info.storage_path_.find(Config::GetInstance().GetDeepStorageDir()) != std::string::npos){
                mylog::GetLogger(logger_name_)->Info("uncompress: " + download_path);
                FileUtil fu(download_path);
                // 解压到 /tmp 下的唯一临时文件名，避免嵌套目录下同名文件相互覆盖
                std::string base = std::string(
                    download_path.begin() + download_path.find_last_of('/') + 1,
                    download_path.end()
                );
                download_path = "/tmp/deepdl_" + std::to_string(::getpid()) + "_"
                    + std::to_string((unsigned long)time(nullptr)) + "_" + base;
                fu.UnCompress(download_path);
                // [分块下载] 标记为临时文件，由 writeCompleteCallback 在发完后删除，
                // 避免原来在 Download() 末尾立即删导致分块发送时文件已不存在
                is_temp = true;
            }

            FileUtil fu(download_path);
            if(-1 == fu.Exists() && info.storage_path_.find("deep_storage") != std::string::npos){
                mylog::GetLogger(logger_name_)->Error("uncompress failed: " + download_path);
                resp->setStatusLine(req.getVersion(), http::HttpResponse::k500InternalServerError, "NULL");
                return -1;
            }

            // [分块下载] chunk_prepare 模式：onRequest 大文件路径调用，
            // 只做解压/查元数据准备，把文件路径和 is_temp 标记写入响应头，不发数据
            if(resp->getBody() == "chunk_prepare"){
                resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
                resp->addHeader("Accept-Ranges", "bytes");
                resp->addHeader("ETag", GetETag(info).c_str());
                resp->addHeader("X-File-Path", download_path);
                resp->addHeader("X-Is-Temp", is_temp ? "1" : "0");
                resp->addHeader("Content-Disposition", "attachment; filename=\"" + file_name + "\"");
                mylog::GetLogger(logger_name_)->Info("Download chunk_prepare done: " + download_path);
                return 0;
            }

            // 小文件路径：一次性读取并发送
            std::string content;
            if(-1 == fu.GetContent(&content)){
                mylog::GetLogger(logger_name_)->Error("read failed: " + download_path);
                resp->setStatusLine(req.getVersion(), http::HttpResponse::k500InternalServerError, "NULL");
                return -1;
            }
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
            resp->addHeader("Accept-Ranges", "bytes");
            resp->addHeader("ETag", GetETag(info).c_str());
            resp->addHeader("Content-Type", "application/octet-stream");
            resp->addHeader("Content-Length", std::to_string(file_size));
            resp->addHeader("Content-Disposition", "attachment; filename=\"" + file_name + "\"");
            resp->setBody(content);

            mylog::GetLogger(logger_name_)->Info("Download success");

            // 小文件：发完后立即删除临时文件
            if(is_temp){
                ::remove(download_path.c_str());
            }

            return 0;
        }

        // 文件夹打包下载：把某顶层文件夹下的所有文件打包成临时 zip，复用大文件分块下发机制
        int DownloadFolder(const http::HttpRequest& req, http::HttpResponse* resp){
            mylog::GetLogger(logger_name_)->Info("DownloadFolder start");

            // 文件夹相对路径放在 folder 查询参数(base64)
            std::string folder = base64_decode(UrlDecode(req.getQueryParameters("folder")));
            if(folder.empty() || !IsSafeRelPath(folder)){
                resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "ILLEGAL FOLDER");
                mylog::GetLogger(logger_name_)->Error("DownloadFolder fail because illegal folder: " + folder);
                return -1;
            }
            // 去掉结尾的 '/'，统一用 "folder/" 作为前缀筛选
            while(!folder.empty() && folder.back() == '/') folder.pop_back();
            std::string prefix = folder + "/";

            // 只信任服务端 table_ 中的数据，按 rel_path_ 前缀筛出该文件夹下的文件
            std::vector<StorageInfo> all;
            storage_data_->GetAll(&all);
            std::vector<StorageInfo> picked;
            for(const auto& info : all){
                if(info.rel_path_.compare(0, prefix.size(), prefix) == 0){
                    picked.push_back(info);
                }
            }
            if(picked.empty()){
                resp->setStatusLine(req.getVersion(), http::HttpResponse::k404NotFound, "FOLDER EMPTY");
                mylog::GetLogger(logger_name_)->Warn("DownloadFolder: no file under " + prefix);
                return -1;
            }

            // 生成唯一临时 staging 目录，所有路径均由服务端自己构造，不拼接客户端字符串
            std::string token = std::to_string(::getpid()) + "_" + std::to_string((unsigned long)time(nullptr));
            std::string staging = "/tmp/folderdl_" + token;
            std::string zip_path = staging + ".zip";

            // 把每个文件按 rel_path_ 还原到 staging 目录中(保留子结构)，deep 文件解压
            for(const auto& info : picked){
                std::string dest = staging + "/" + info.rel_path_;
                size_t sep = dest.find_last_of('/');
                if(sep != std::string::npos){
                    FileUtil(dest.substr(0, sep)).CreateDirectory();
                }
                FileUtil src(info.storage_path_);
                if(info.storage_path_.find(Config::GetInstance().GetDeepStorageDir()) != std::string::npos){
                    src.UnCompress(dest);  // 深度存储：解压写入
                }else{
                    std::string content;
                    if(src.GetContent(&content) == -1){
                        mylog::GetLogger(logger_name_)->Warn("DownloadFolder read fail: " + info.storage_path_);
                        continue;
                    }
                    FileUtil(dest).SetContent(content.c_str(), content.size());
                }
            }

            // 在 staging 目录内打包，命令只使用服务端生成的固定路径
            std::string cmd = "cd " + staging + " && zip -r -q " + zip_path + " .";
            int rc = ::system(cmd.c_str());
            // staging 目录已无用，打包后即可删除(zip 已生成在 staging 之外)
            ::system(("rm -rf " + staging).c_str());
            if(rc != 0 || !FileUtil(zip_path).Exists()){
                resp->setStatusLine(req.getVersion(), http::HttpResponse::k500InternalServerError, "ZIP FAIL");
                mylog::GetLogger(logger_name_)->Error("DownloadFolder zip fail, rc=" + std::to_string(rc));
                ::remove(zip_path.c_str());
                return -1;
            }

            std::string download_name = folder + ".zip";

            // chunk_prepare：把临时 zip 路径交给大文件分块发送流程，发完由 writeCallback 删除
            if(resp->getBody() == "chunk_prepare"){
                resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
                resp->addHeader("X-File-Path", zip_path);
                resp->addHeader("X-Is-Temp", "1");
                resp->addHeader("Content-Disposition", "attachment; filename=\"" + download_name + "\"");
                mylog::GetLogger(logger_name_)->Info("DownloadFolder chunk_prepare done: " + zip_path);
                return 0;
            }

            // 非分块路径(zip 小于阈值)：一次性读取发送，发完删除临时 zip
            FileUtil zf(zip_path);
            std::string content;
            if(zf.GetContent(&content) == -1){
                resp->setStatusLine(req.getVersion(), http::HttpResponse::k500InternalServerError, "NULL");
                ::remove(zip_path.c_str());
                return -1;
            }
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
            resp->addHeader("Content-Type", "application/octet-stream");
            resp->addHeader("Content-Length", std::to_string(content.size()));
            resp->addHeader("Content-Disposition", "attachment; filename=\"" + download_name + "\"");
            resp->setBody(content);
            ::remove(zip_path.c_str());

            mylog::GetLogger(logger_name_)->Info("DownloadFolder success");
            return 0;
        }

        // 删除文件
        int Remove(const http::HttpRequest& req, http::HttpResponse* resp){
            mylog::GetLogger(logger_name_)->Info("Remove start");

            // 获取文件名
            std::string filename = req.getHeader("FileName");
            filename = base64_decode(filename);
            mylog::GetLogger(logger_name_)->Info("The file: " + filename + " will be remove");

            mystorage::StorageInfo info;
            // 理论上在网页端点击，不可能删掉不存在的
            if(storage_data_->GetOneByURL("/download/" + filename, &info) == -1){
                mylog::GetLogger(logger_name_)->Warn("file is not exist");
                resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
                return -1;
            }

            // 删除文件
            remove(info.storage_path_.c_str());
            // 从table_中删除对应的storageinfo并更新
            storage_data_->Erase(info.url_);
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
            resp->addHeader("Content-Type", "application/json");
            resp->setBody("{\"status\":\"success\",\"message\":\"File remove successfully\",\"filename\":\"" + filename + "\"}\n");

            mylog::GetLogger(logger_name_)->Info("Remove success");

            return 0;
        }

        // 删除整个文件夹：删掉该相对路径前缀下的所有文件及其 StorageInfo，
        // 再清理磁盘上残留的空子目录。folder 放在 FileName 头(base64)。
        int RemoveFolder(const http::HttpRequest& req, http::HttpResponse* resp){
            mylog::GetLogger(logger_name_)->Info("RemoveFolder start");

            std::string folder = base64_decode(req.getHeader("FileName"));
            if(folder.empty() || !IsSafeRelPath(folder)){
                resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "ILLEGAL FOLDER");
                mylog::GetLogger(logger_name_)->Error("RemoveFolder fail because illegal folder: " + folder);
                return -1;
            }
            // 去掉结尾的 '/'，统一用 "folder/" 作为前缀筛选
            while(!folder.empty() && folder.back() == '/') folder.pop_back();
            std::string prefix = folder + "/";

            // 只信任服务端 table_ 数据，按 rel_path_ 前缀筛出该文件夹下的所有文件
            std::vector<StorageInfo> all;
            storage_data_->GetAll(&all);
            int removed = 0;
            for(const auto& info : all){
                if(info.rel_path_.compare(0, prefix.size(), prefix) == 0){
                    remove(info.storage_path_.c_str());   // 删磁盘文件
                    storage_data_->Erase(info.url_);       // 删元数据
                    removed++;
                }
            }
            if(removed == 0){
                resp->setStatusLine(req.getVersion(), http::HttpResponse::k404NotFound, "FOLDER EMPTY");
                mylog::GetLogger(logger_name_)->Warn("RemoveFolder: no file under " + prefix);
                return -1;
            }

            // 清理两类存储根目录下残留的空子目录(文件已删，目录树可能空着)
            std::error_code ec;
            std::experimental::filesystem::remove_all(Config::GetInstance().GetLowStorageDir() + folder, ec);
            std::experimental::filesystem::remove_all(Config::GetInstance().GetDeepStorageDir() + folder, ec);

            resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
            resp->addHeader("Content-Type", "application/json");
            resp->setBody("{\"status\":\"success\",\"message\":\"Folder removed\",\"count\":" + std::to_string(removed) + "}\n");

            mylog::GetLogger(logger_name_)->Info("RemoveFolder success, removed=" + std::to_string(removed));
            return 0;
        }

    private:
        void initialize(){
            // 初始化数据库连接池
            // http::MysqlUtil::init("tcp://127.0.0.1:3306", "root", "root", "chat", 10);
            // 初始化会话
            initializeSession();
            // 初始化中间件
            initializeMiddleWare();
            // 初始化路由
            initializeRouter();
            // 初始化SSL服务
            initializeSSLTLS();
        }

        void initializeSession(){
            // 创建会话存储
            auto sessionStorage = std::make_unique<http::session::MemorySessionStorage>();
            // 创建会话管理器
            auto sessionManager = std::make_unique<http::session::SessionManager>(std::move(sessionStorage));
            // 设置会话管理器
            setSessionManager(std::move(sessionManager));
        }

        void initializeRouter(){
            httpServer_.Get("/", std::make_shared<GenHandler>(this));
            httpServer_.Post("/upload", std::make_shared<GenHandler>(this));
            
            // 实际上可以把文件名放在请求头中，就不需要附在路径上了
            // auto downloadhandler = std::make_shared<GenHandler>(this);
            // httpServer_.addRoute(http::HttpRequest::kGet, "/download/(.+)", downloadhandler);
            httpServer_.Get("/download", std::make_shared<GenHandler>(this));
            httpServer_.Get("/download_folder", std::make_shared<GenHandler>(this));

            httpServer_.Delete("/remove", std::make_shared<GenHandler>(this));
            httpServer_.Delete("/remove_folder", std::make_shared<GenHandler>(this));
        }

        void initializeMiddleWare(){
            // 创建中间件
            auto corsMiddleware = std::make_shared<http::middleware::CorsMiddleware>();
            // 添加中间件
            httpServer_.addMiddleware(corsMiddleware);
        }

        void initializeSSLTLS(){
            // httpServer_.setSslConfig(ssl::SslConfig());
            httpServer_.setSslConfig(http::ssl::SslConfig());
        }

        void setSessionManager(std::unique_ptr<http::session::SessionManager> manager){
            httpServer_.setSeesionManager(std::move(manager));
        }

        http::session::SessionManager* getSessionManager() const{
            return httpServer_.getSessionManager();
        }

        void packageResp(const std::string& version, http::HttpResponse::HttpStatusCode statusCode, 
                        const std::string& statusMsg, bool close, const std::string& contentType,
                        int contentLen, const std::string& body, http::HttpResponse* resp){

            if(!resp){
                // logger_->ERROR("Response pointer is null");
                mylog::GetLogger(logger_name_)->Error("Response pointer is null");
                return;
            }

            try{
                resp->setVersion(version);
                resp->setStatusCode(statusCode);
                resp->setStatusMessage(statusMsg);
                resp->setCloseConnection(close);
                resp->setContentType(contentType);
                resp->setContentLength(contentLen);
                resp->setBody(body);

                mylog::GetLogger(logger_name_)->Info("Response packaged successfully");
            }
            catch(const std::exception& e){
                mylog::GetLogger(logger_name_)->Error(std::string("Error in packageResp: ") + e.what());
                
                // 设置一个基本的错误响应
                resp->setStatusCode(http::HttpResponse::k500InternalServerError);
                resp->setStatusMessage("Internal Server Error");
                resp->setCloseConnection(true);
            }
        }

        http::HttpServer httpServer_;
    };
}


#endif
