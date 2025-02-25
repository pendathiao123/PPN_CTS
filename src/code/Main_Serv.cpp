#include <iostream>
#include "../headers/Server.h"
#include "../headers/Client.h"


int main() {
    /*
    std::string ok;
    std::cout << "ook == " << ok.empty() << std::endl;
    */

    Server server{4433, "../configFile.csv","../log.csv"};
    //server.StartServer(4433, "../server.crt", "../server.key", "../configFile.csv","../log.csv");
    server.StartServer("../server.crt", "../server.key");
    return 0;
}

