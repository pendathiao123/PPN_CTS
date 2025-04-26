#include <iostream>
#include "../headers/Server.h"
#include "../headers/Client.h"


int main() {
    // creation du serveur
    Server server{4433, "../configFile.csv","../log.csv"};

    // lancement du serveur
    server.StartServer("../server.crt", "../server.key");
    
    return 0;
}

