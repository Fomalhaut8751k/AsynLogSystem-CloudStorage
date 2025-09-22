#include "ClientBackupLog.hpp"

int main()
{
    Client client("127.0.0.1", 8000);
    client.start();

    return 0;
}