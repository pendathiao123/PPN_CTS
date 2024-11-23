#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>  // Pour inet_addr
#include <unistd.h>     // Pour close()
#include <cstring>      // Pour memset
#include <fstream>
#include <thread>        // Pour gérer les connexions clients en threads
#include "../headers/Server.h"

// Constructeur
Server::Server(const std::string& ipAddress, int port, const std::string& configFile) 
    : ipAddress(ipAddress), port(port) {
    std::cout << "Serveur initialisé à l'adresse " << ipAddress 
              << " sur le port " << port << " avec le fichier de configuration : " << configFile << std::endl;
    setCryptos(configFile);  // Charger les cryptomonnaies à partir du fichier
}

// Chargement des cryptomonnaies
void Server::setCryptos(const std::string& configFile) {
    std::ifstream file(configFile);
    if (!file.is_open()) {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier " << configFile << std::endl;
        return;
    }

    std::string name;
    double price, changeRate;

    // Lecture ligne par ligne
    while (file >> name >> price >> changeRate) {
        cryptos.emplace_back(name, price, changeRate);  
        std::cout << "Crypto ajoutée : " << name << " | Prix : " << price 
                  << " | Taux de variation : " << changeRate << std::endl;
    }
    file.close();

    if (cryptos.empty()) {
        std::cerr << "Aucune cryptomonnaie chargée depuis le fichier de configuration.\n";
    }
}

// Retourne les cryptomonnaies
const std::vector<Crypto>& Server::getCryptos() const {
    return cryptos;
}

// Méthode principale du serveur
void Server::start() {
    // Création du socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Erreur : Impossible de créer le socket.\n";
        exit(EXIT_FAILURE);
    }

    // Configuration de l'adresse du serveur
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = inet_addr(ipAddress.c_str());

    // Liaison du socket
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "Erreur : Échec du bind sur " << ipAddress << ":" << port << "\n";
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    // Mise en écoute des connexions
    if (listen(serverSocket, 5) < 0) {
        std::cerr << "Erreur : Échec de listen().\n";
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    std::cout << "Serveur démarré et en attente de connexions sur " 
              << ipAddress << ":" << port << "...\n";

    while (true) {
        // Accepter une connexion client
        sockaddr_in clientAddress{};
        socklen_t clientAddressLen = sizeof(clientAddress);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientAddressLen);
        if (clientSocket < 0) {
            std::cerr << "Erreur : Échec de l'acceptation d'une connexion.\n";
            continue;  // Passer à la prochaine connexion
        }

        // Afficher l'adresse du client
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddress.sin_addr, clientIP, sizeof(clientIP));
        std::cout << "Nouvelle connexion acceptée de " << clientIP 
                  << ":" << ntohs(clientAddress.sin_port) << "\n";


        close(clientSocket);  // Fermer après traitement (ou après le thread)
    }

    close(serverSocket);  // Fermer le socket du serveur
}
