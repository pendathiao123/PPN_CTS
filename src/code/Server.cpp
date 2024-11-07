#include "Server.h"
#include "Crypto.h"
#include "ClientHandler.h"
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>  // Pour inet_addr
#include <unistd.h>     // Pour close()
#include <cstring>      // Pour memset
#include <sstream>
#include <unordered_map> 

Server::Server(const std::string& ipAddress, int port, const std::string& configFile) 
    : port(port), ipAddress(ipAddress) {
    std::cout << "Serveur initialisé à l'adresse " << ipAddress 
              << " sur le port " << port << " avec " << configFile << std::endl;
}

void Server::start() {
    // Création du socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Erreur : Impossible de créer le socket.\n";
        exit(1);
    }

    // Configuration de l'adresse du serveur
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = inet_addr(ipAddress.c_str());  // Utilise l'IP spécifiée

    // Liaison du socket à l'adresse et au port
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "Erreur : Échec du bind.\n";
        close(serverSocket);
        exit(1);
    }

    // Mise en écoute des connexions
    if (listen(serverSocket, 5) < 0) {
        std::cerr << "Erreur : Échec de listen().\n";
        close(serverSocket);
        exit(1);
    }

    std::cout << "Serveur démarré et à l'écoute des connexions sur " 
              << ipAddress << ":" << port << "...\n";

     while (true) {
        // Accepter une nouvelle connexion
        sockaddr_in clientAddress{};
        socklen_t clientAddressLen = sizeof(clientAddress);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientAddressLen);
        if (clientSocket < 0) {
            std::cerr << "Erreur : Échec de l'acceptation de la connexion.\n";
            continue;  // Passer à la prochaine tentative d'acceptation
        }

    std::cout << "Client connecté.\n";

    // Créer une instance de ClientHandler et traiter la requête
    ClientHandler handler(clientSocket,balances);
    handler.processRequest();
    
    close(clientSocket);          
}

 close(serverSocket);

}