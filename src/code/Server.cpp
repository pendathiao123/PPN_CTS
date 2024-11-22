#include "Server.h"
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>  // Pour inet_addr
#include <unistd.h>     // Pour close()
#include <cstring>      // Pour memset
#include <thread>
#include <atomic>
#include <fstream>

void updateBitcoinPrices();
std::atomic<bool> stopRequested(false);

Server::Server(const std::string& ipAddress, int port, const std::string& configFile) 
    : ipAddress(ipAddress), port(port) {
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

    std::string filename = "SRD-BTC.dat";
    std::ofstream outFile(filename, std::ios::app);
    if (!outFile) {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier " << filename << ".\n";
        return;
    }

    std::thread bitcoinPriceThread(updateBitcoinPrices);

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

    close(clientSocket);          
}

 close(serverSocket);
 stopRequested = true;
 bitcoinPriceThread.join();

}

void updateBitcoinPrices() {
    std::string filename = "SRD-BTC.dat";
    std::ofstream outFile(filename, std::ios::app);
    if (!outFile) {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier " << filename << ".\n";
        return;
    }
    Crypto crypto;
    int i = 0;
    while (!stopRequested) {
        double bitcoinPrice = crypto.getPrice("SRD-BTC");
        std::time_t timestamp = std::time(nullptr);
        outFile << timestamp << " " << bitcoinPrice << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(1)); // Pause d'une seconde
        if (++i >= 10000) {  // Arrêter après 10000 itérations
            stopRequested = true;
            std::cout << "Fin de la mise à jour des prix de Bitcoin.\n";
        }
    }
}