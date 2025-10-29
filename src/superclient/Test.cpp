#include "../client/Client.hpp"

int main(int argc, char* argv[]) 
{
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <command> [args...]" << std::endl;
        std::cout << "Commands:" << std::endl;
        std::cout << "  upload <file_path>" << std::endl;
        std::cout << "  download <filename> <save_path>" << std::endl;
        std::cout << "  list" << std::endl;
        std::cout << "  remove <filename>" << std::endl;
        std::cout << "  reload  (*)" << std::endl;
        return 1;
    }
    
    StorageSuperClient super_client("http://172.30.173.224:8081");
    std::string command = argv[1];
    
    if (command == "upload" && argc >= 3) 
    {
        super_client.Upload(argv[2], argv[3]);
    } 
    else if (command == "download" && argc >= 3) 
    {
        if(argc ==3)  // 没有指定保存路径，就下载在当前路径
        {
            super_client.Download(argv[2], "");
        }
        else
        {
            super_client.Download(argv[2], argv[3]);
        }
        
    } 
    else if (command == "list") 
    {
        super_client.ListFiles();
    } 
    else if (command == "remove")
    {
        super_client.Remove(argv[2]);
    }
    else if (command == "reload")   // 重新连接线程池
    {
        if(super_client.ReConnectBackLog())
        {   // 如果超级用户尝试连接远程服务器成功，就可以通知线程池重连服务器
            std::cerr << "Successfully connected to remote server, thread pool will reconnect to server" << std::endl;
            super_client.ReLoadThreadPool();
        } 
        else
        {
            std::cerr << "Failed to connect to remote server" << std::endl;
        }
    }
    else 
    {
        std::cerr << "Invalid command or arguments" << std::endl;
        return 1;
    }
    
    return 0;
}