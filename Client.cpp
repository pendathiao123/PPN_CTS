#include "Client.h"
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>  // Pour inet_addr
#include <unistd.h>     // Pour close()
#include <cstring>      // Pour memset

Client::Client(const std::string& address, int port) {
    std::cout << "Connexion au serveur à l'adresse " << address << " sur le port " << port << std::endl;
    
    // Création du socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        std::cerr << "Erreur : Impossible de créer le socket.\n";
        exit(1);
    }

    // Configuration de l'adresse du serveur
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = inet_addr(address.c_str());

    // Connexion au serveur
    if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "Erreur : Connexion échouée.\n";
        exit(1);
    }

    std::cout << "Connexion réussie au serveur.\n";
}

void Client::sendRequest(const std::string& request) {
    std::cout << "Requête envoyée : " << request << std::endl;
    send(clientSocket, request.c_str(), request.size(), 0);  // Envoi de la requête
}

std::string Client::receiveResponse() {
    char buffer[1024] = {0};  // Buffer pour stocker la réponse
    int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);

    if (bytesRead < 0) {
        std::cerr << "Erreur : Réception échouée.\n";
        return "";
    }

    std::string response(buffer, bytesRead);  // Conversion du buffer en string
    std::cout << "Réponse reçue : " << response << std::endl;
    return response;
}

Client::~Client() {
    close(clientSocket);  // Fermeture du socket
}


