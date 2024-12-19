#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>  // Pour inet_addr
#include <unistd.h>     // Pour close()
#include <cstring>      // Pour memset
#include "../headers/Client.h"


Client::Client() : clientSocket(){}

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

int Client::getsocket(){
    return clientSocket;
}

void Client::sendRequest(const std::string& request) {
    std::cout << "Requête envoyée : " << request << std::endl;
    int clientSocket = getsocket();
    ssize_t bytesSent = send(clientSocket, request.c_str(), request.size(), 0);
    if (bytesSent < 0) {
        std::cerr << "Erreur lors de l'envoi de la requête. Code erreur : " << errno << std::endl;
        perror("Détails de l'erreur");
    } else {
        std::cout << "Nombre d'octets envoyés : " << bytesSent << std::endl;
    }
}

std::string Client::receiveResponse() {
    char buffer[1024];  // Buffer pour stocker la réponse
    int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);

    if (bytesRead <= 0) {
        std::cerr << "Erreur : Réception échouée.\n";
        return "";
    }
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0'; // Terminer la chaîne correctement
        std::string response(buffer); // Convertir le tampon en string
        std::cout << "Réponse reçue : [" << response << "]" << std::endl;
        
        // Retourner la réponse reçue
        return response;
    }
}

void Client::buy(const std::string& currency, double percentage) {
    // Construire la requête d'achat
    std::string request = "BUY " + currency + " " + std::to_string(percentage);
    
    // Envoyer la requête d'achat au serveur
    sendRequest(request);
    
    // Recevoir et afficher la réponse
    std::string response = receiveResponse();
}

void Client::sell(const std::string& currency, double percentage) {
    // Construire la requête d'achat
    std::string request = "SELL " + currency + " " + std::to_string(percentage);
    
    // Envoyer la requête d'achat au serveur
    sendRequest(request);
    
    // Recevoir et afficher la réponse
    std::string response = receiveResponse();
}

Client::~Client() {
    close(clientSocket);  // Fermeture du socket
    std::cout << "Client : Connexion fermée par le destructeur.\n";
}


