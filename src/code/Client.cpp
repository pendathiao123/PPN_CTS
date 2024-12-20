#include "../headers/Client.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

// Initialiser le contexte SSL pour le client
SSL_CTX* InitClientCTX() {
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
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
void StartClient(const std::string& serverAddress, int port, const std::string& clientId, const std::string& clientToken) {
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
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
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0'; // Terminer la chaîne correctement
        std::string response(buffer); // Convertir le tampon en string
        std::cout << "Réponse reçue : [" << response << "]" << std::endl;
        
        // Retourner la réponse reçue
        return response;
    }
}

    SSL_CTX* ctx = InitClientCTX();
    SSL* ssl = ConnectSSL(ctx, clientSocket);
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