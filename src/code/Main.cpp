#include <iostream>
#include "Server.h"
#include "Client.h"

int main() {
    // Spécification de l'adresse IP et du port
    std::string serverIp = "127.0.0.1";
    int serverPort = 8080;

    // Initialisation du serveur avec une adresse IP spécifique
    Server server(serverIp, serverPort, "cryptos.json");
    server.start();

    // Création du client pour se connecter au serveur
    Client client(serverIp, serverPort);
    client.sendRequest("GET /market");

    std::cout << "Réponse du serveur: " << client.receiveResponse() << std::endl;

    return 0;
}
