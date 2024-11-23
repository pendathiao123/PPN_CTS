#include <iostream>
#include "../headers/Server.h"
#include "../headers/Client.h"


int main() {
    // Spécification de l'adresse IP et du port
    std::string serverIp = "127.0.0.1";
    int serverPort = 8080;

    // Initialisation du serveur avec une adresse IP spécifique
    Server server(serverIp, serverPort, "cryptos.json");
    server.start();
    return 0;
}

