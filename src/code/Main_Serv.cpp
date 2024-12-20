#include <iostream>
#include "../headers/Server.h"
#include "../headers/Client.h"


int main() {
    Server server;
    server.StartServer(4433, "server.crt", "server.key", "configFile.csv");
    return 0;
}

