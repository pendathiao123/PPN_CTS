#include "Server.h"
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>  // Pour inet_addr
#include <unistd.h>     // Pour close()
#include <cstring>      // Pour memset
#include <sstream>

Server::Server(const std::string& ipAddress, int port, const std::string& configFile) 
    : port(port), ipAddress(ipAddress) {
    std::cout << "Serveur initialisé à l'adresse " << ipAddress 
              << " sur le port " << port << " avec " << configFile << std::endl;
}

std::string handleMarket(const std::string& request) {
    // Traitement du market
    return "Accès Market réussi\n";
}


std::string handleBuy(const std::string& request) {
    // Extraire la paire de crypto et le montant de la requête
    std::istringstream iss(request);
    std::string action, currency;
    double amount;

    if (!(iss >> action >> currency >> amount)) {
        return "Erreur : Format de commande invalide\n";
    }

    // Traitement de l'achat
    return "Achat de " + std::to_string(amount) + " " + currency + " réussi\n";
}
std::string handleSell(const std::string& request) {
    // Extraire la paire de crypto et le montant de la requête
    std::istringstream iss(request);
    std::string action, currency;
    double amount;

    if (!(iss >> action >> currency >> amount)) {
        return "Erreur : Format de commande invalide\n";
    }

    // Traitement de la vente ici 
    return "Vente de " + std::to_string(amount) + " " + currency + " réussi\n";
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
    request(clientSocket); //Traitement des requetes du client
    close(clientSocket);          
}

 close(serverSocket);

}


void Server::request(int clientSocket) {
     while (true) {
    
        char buffer[1024] = {0}; // Tampon pour stocker la réponse
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead < 0) {
            std::cerr << "Erreur de réception." << std::endl;
            break;
        } else if (bytesRead == 0) { //Client déconnecté, permet de revenir à l'état d'attente de nouvelles connexions(Server::Start)
            std::cout << "Client déconnecté.\n"; //Affichage de la deconnexion du client sur le terminal Server
            break;
        }

        buffer[bytesRead] = '\0'; // Terminer la chaîne
        std::string request(buffer); // Convertir le tampon en std::string
        std::cout << "Requête reçue : " << request << std::endl; // Ajout de log


        std::string response;
        if (request.rfind("GET /market", 0) == 0) {
            std::string response = handleMarket(request);
            send(clientSocket, response.c_str(), response.size(), 0);
        } else if (request.rfind("BUY", 0) == 0) {
            std::cout << "Passage dans rfindBUY " << std::endl;
            // Traiter la commande BUY
            std::string response = handleBuy(request);
            send(clientSocket, response.c_str(), response.size(), 0);
        } else if (request.rfind("SELL", 0) == 0) {
            std::cout << "Passage dans rfindSELL " << std::endl;
            // Traiter la commande SELL
            std::string response = handleSell(request);
            send(clientSocket, response.c_str(), response.size(), 0);
        } else {
            response = "Commande inconnue\n";
        }

        send(clientSocket, response.c_str(), response.size(), 0);
        }      
}