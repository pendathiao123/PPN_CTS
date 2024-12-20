#include "../headers/Client.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

Client::Client() : clientSocket(-1), ctx(nullptr), ssl(nullptr) {
    memset(&serverAddr, 0, sizeof(serverAddr));
}

Client::~Client() {
    closeConnection();
}

// Initialiser le contexte SSL pour le client
SSL_CTX* InitClientCTX() {
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());

    // Charger le certificat public et la clé privée du client
    SSL_CTX_use_certificate_file(ctx, "../server.crt", SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(ctx, "../server.key", SSL_FILETYPE_PEM);

    if (!ctx) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

// Établir une connexion SSL
SSL* ConnectSSL(SSL_CTX* ctx, int clientSocket) {
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, clientSocket);
    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        return nullptr;
    }
    return ssl;
}

// Fonction principale pour démarrer le client
void Client::StartClient(const std::string& serverAddress, int port, const std::string& clientId, const std::string& clientToken) {
    this->clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    if (inet_pton(AF_INET, serverAddress.c_str(), &serverAddr.sin_addr) <= 0) {
        perror("Invalid address");
        close(clientSocket);
        exit(EXIT_FAILURE);
    }

    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Connection failed");
        close(clientSocket);
        exit(EXIT_FAILURE);
    }

    this->ctx = InitClientCTX();
    this->ssl = ConnectSSL(ctx, this->clientSocket);
    if (!ssl) {
        close(clientSocket);
        SSL_CTX_free(ctx);
        exit(EXIT_FAILURE);
    }

    // Construire le message à envoyer
    std::string message;
    if (clientId.empty() || clientToken.empty()) {
        message = "FIRST_CONNECTION";
    } else {
        message = "ID:" + clientId + ",TOKEN:" + clientToken;
    }

    // Envoyer le message au serveur
    SSL_write(ssl, message.c_str(), message.size());

    // Lire la réponse du serveur
    char buffer[1024] = {0};
    int bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        std::cout << "Server Response: " << buffer << std::endl;
    }

    SSL_free(ssl);
    close(clientSocket);
    SSL_CTX_free(ctx);
}

void Client::sendRequest(const std::string& request) {
    std::cout << "Requête envoyée : " << request << std::endl;
    int bytesSent = SSL_write(ssl, request.c_str(), request.length());
    if (bytesSent <= 0) {
        std::cerr << "Erreur lors de l'envoi de la requête SSL" << std::endl;
        ERR_print_errors_fp(stderr);
    } else {
        std::cout << "Nombre d'octets envoyés : " << bytesSent << std::endl;
    }
}

std::string Client::receiveResponse() {
    if (!ssl) {
        return "";
    }

    char buffer[1024];
    int bytesRead = SSL_read(ssl, buffer, sizeof(buffer) - 1);
    
    if (bytesRead <= 0) {
        std::cerr << "Erreur : Réception SSL échouée" << std::endl;
        ERR_print_errors_fp(stderr);
        return "";
    }

    buffer[bytesRead] = '\0';
    std::string response(buffer);
    std::cout << "Réponse reçue : [" << response << "]" << std::endl;
    return response;
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

void Client::closeConnection() {
    if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = nullptr;
    }
    if (clientSocket != -1) {
        close(clientSocket);
        clientSocket = -1;
    }
    if (ctx) {
        SSL_CTX_free(ctx);
        ctx = nullptr;
    }
}