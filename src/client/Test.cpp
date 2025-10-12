#include "Client.hpp"

int main(int argc, char* argv[]) 
{
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <command> [args...]" << std::endl;
        std::cout << "Commands:" << std::endl;
        std::cout << "  upload <file_path>" << std::endl;
        std::cout << "  download <filename> <save_path>" << std::endl;
        std::cout << "  list" << std::endl;
        return 1;
    }
    
    StorageClient client("http://172.30.173.224:8081");
    std::string command = argv[1];
    
    if (command == "upload" && argc >= 3) 
    {
        client.Upload(argv[2]);
    } 
    else if (command == "download" && argc >= 4) 
    {
        client.Download(argv[2], argv[3]);
    } 
    else if (command == "list") 
    {
        client.ListFiles();
    } 
    else 
    {
        std::cerr << "Invalid command or arguments" << std::endl;
        return 1;
    }
    
    return 0;
}