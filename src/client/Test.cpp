// #include "Client.hpp"

// int main(int argc, char* argv[]) 
// {
//     if (argc < 2) {
//         std::cout << "Usage: " << argv[0] << " <command> [args...]" << std::endl;
//         std::cout << "Commands:" << std::endl;
//         std::cout << "  upload <file_path>" << std::endl;
//         std::cout << "  download <filename> <save_path>" << std::endl;
//         std::cout << "  list" << std::endl;
//         std::cout << "  remove <filename>" << std::endl;
//         return 1;
//     }
    
//     StorageClient client("http://172.30.173.24:8081");
//     std::string command = argv[1];
    
//     if (command == "upload" && argc >= 3) 
//     {
//         client.Upload(argv[2], argv[3]);
//     } 
//     else if (command == "download" && argc >= 3) 
//     {
//         if(argc ==3)  // 没有指定保存路径，就下载在当前路径
//         {
//             client.Download(argv[2], "");
//         }
//         else
//         {
//             client.Download(argv[2], argv[3]);
//         }
        
//     } 
//     else if (command == "list") 
//     {
//         client.ListFiles();
//     } 
//     else if (command == "remove")
//     {
//         client.Remove(argv[2]);
//     }
//     else 
//     {
//         std::cerr << "Invalid command or arguments" << std::endl;
//         return 1;
//     }
    
//     return 0;
// }

#include "Client.hpp"
#include <thread>
#include <string>

void threadfunc(StorageClient* client, int i){
    client->Download("2.zip", "save/" + std::to_string(i) + ".zip");
}



int main(int argc, char* argv[]) 
{
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <command> [args...]" << std::endl;
        std::cout << "Commands:" << std::endl;
        std::cout << "  upload <file_path>" << std::endl;
        std::cout << "  download <filename> <save_path>" << std::endl;
        std::cout << "  list" << std::endl;
        std::cout << "  remove <filename>" << std::endl;
        return 1;
    }
    
    StorageClient client("http://172.30.173.24:8081");
    std::string command = argv[1];

    if(command == "multi"){  // 测试功能
        std::thread t1(threadfunc, &client, 1);
        std::thread t2(threadfunc, &client, 2);
        std::thread t3(threadfunc, &client, 3);
        std::thread t4(threadfunc, &client, 4);
        std::thread t5(threadfunc, &client, 5);

        t1.join();
        t2.join();
        t3.join();
        t4.join();
        t5.join();
    }
    
    if (command == "upload" && argc >= 3) 
    {
        client.Upload(argv[2], argv[3]);
    } 
    else if (command == "download" && argc >= 3) 
    {
        if(argc ==3)  // 没有指定保存路径，就下载在当前路径
        {
            client.Download(argv[2], "");
        }
        else
        {
            client.Download(argv[2], argv[3]);
        }
        
    } 
    else if (command == "list") 
    {
        client.ListFiles();
    } 
    else if (command == "remove")
    {
        client.Remove(argv[2]);
    }
    else 
    {
        std::cerr << "Invalid command or arguments" << std::endl;
        return 1;
    }
    
    return 0;
}