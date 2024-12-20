#include "../headers/Server.h"
#include "../headers/Crypto.h"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <openssl/rand.h>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

// Fonction pour générer une chaîne aléatoire
std::string GenerateRandomString(size_t length) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string result;
    unsigned char rand_bytes[length];
    if (RAND_bytes(rand_bytes, length) != 1) {
        std::cerr << "Erreur de génération aléatoire" << std::endl;
        return "";
    }

    for (size_t i = 0; i < length; ++i) {
        result += charset[rand_bytes[i] % (sizeof(charset) - 1)];
    }

    return result;
}

// Fonction pour générer un ID à 4 chiffres aléatoires
std::string GenerateRandomId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);

    int id = dis(gen);
    return std::to_string(id);
}

// Fonction pour générer un token avec HMAC
std::string GenerateToken() {
    std::string key = GenerateRandomString(32);
    std::string message = GenerateRandomString(16);

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;

    HMAC(EVP_sha256(), key.c_str(), key.size(),
         reinterpret_cast<const unsigned char*>(message.c_str()), message.size(),
         hash, &hash_len);

    std::ostringstream oss;
    for (unsigned int i = 0; i < hash_len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }

    return oss.str();
}

// Charger les utilisateurs depuis un fichier
std::unordered_map<std::string, std::string> LoadUsers(const std::string& filename) {
    std::unordered_map<std::string, std::string> users;
    std::ifstream file(filename);
    std::string id, token;
    while (file >> id >> token) {
        users[id] = token;
    }
    return users;
}

// Sauvegarder les utilisateurs dans un fichier
void SaveUsers(const std::string& filename, const std::unordered_map<std::string, std::string>& users) {
    std::ofstream file(filename, std::ios::trunc);
    for (const auto& [id, token] : users) {
        file << id << " " << token << "\n";
    }
}

// Initialiser le contexte SSL pour le serveur
SSL_CTX* InitServerCTX(const std::string& certFile, const std::string& keyFile) {
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_certificate_file(ctx, certFile.c_str(), SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, keyFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        exit(EXIT_FAILURE);
    }

    return ctx;
}

// Accepter une connexion SSL
SSL* AcceptSSLConnection(SSL_CTX* ctx, int clientSocket) {
     SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, clientSocket);
    if (SSL_accept(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        return nullptr;
    }
    return ssl;
}

// Gérer une connexion client
void HandleClient(SSL* ssl, std::unordered_map<std::string, std::string>& users, const std::string& usersFile) {
    char buffer[1024] = {0};
    int bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1);
    if (bytes <= 0) {
        return;
    }

    buffer[bytes] = '\0';
    std::string receivedMessage(buffer);
    std::cout << "Received: " << receivedMessage << std::endl;

    std::string response;
    std::string id, token;
    std::string prefix_id = "ID:";
    std::string prefix_token = "TOKEN:";

    if (receivedMessage.find(prefix_id) != std::string::npos && receivedMessage.find(prefix_token) != std::string::npos) {
        size_t id_start = receivedMessage.find(prefix_id) + prefix_id.length();
        size_t id_end = receivedMessage.find(",", id_start);
        id = receivedMessage.substr(id_start, id_end - id_start);

        size_t token_start = receivedMessage.find(prefix_token) + prefix_token.length();
        token = receivedMessage.substr(token_start);

        std::cout << "Extracted ID: " << id << ", Token: " << token << std::endl;

        auto it = users.find(id);
        if (it != users.end() && it->second == token) {
            response = "AUTH OK";
        } else {
            response = "AUTH FAIL";
        }
    } else {
        id = GenerateRandomId();
        token = GenerateToken();
        users[id] = token;
        SaveUsers(usersFile, users);

        std::cout << "New ID: " << id << ", New Token: " << token << std::endl;
        response = "Identifiants: " + id + " " + token;
    }

    SSL_write(ssl, response.c_str(), response.size());
}

// Fonction principale pour démarrer le serveur
void Server::StartServer(int port, const std::string& certFile, const std::string& keyFile, const std::string& usersFile) {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    if (listen(serverSocket, 5) < 0) {
        perror("Listen failed");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    SSL_CTX* ctx = InitServerCTX(certFile, keyFile);
    std::cout << "Server listening on port " << port << std::endl;

    auto users = LoadUsers(usersFile);

    while (true) {
        socklen_t addrLen = sizeof(serverAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&serverAddr, &addrLen);

        if (clientSocket < 0) {
            perror("Accept failed");
            continue;
        }

        SSL* ssl = AcceptSSLConnection(ctx, clientSocket);
        if (ssl) {
            HandleClient(ssl, users, usersFile);
            SSL_free(ssl);
        }

        ProcessRequest(ssl);

        close(clientSocket);
    }

    close(serverSocket);
    SSL_CTX_free(ctx);
}

void Server::ProcessRequest(SSL* ssl){
    char buffer[1024] = {0}; // Tampon pour stocker la réponse

    try {
        int bytesRead = SSL_read(ssl, buffer, sizeof(buffer) - 1);
        if (bytesRead <= 0) {
            return;
        }

        buffer[bytesRead] = '\0'; // Terminer la chaîne
        std::string request(buffer); // Convertir le tampon en std::string
        std::cout << "Requête reçue : " << request << std::endl;

        std::string response;
        if (request.rfind("BUY", 0) == 0) {
            std::cout << "Passage dans rfindBUY " << std::endl;
            // Traiter la commande BUY
            std::string response = handleBuy(request);
            //send(clientSocket, response.c_str(), response.size(), 0);
        } 
        else if (request.rfind("SELL", 0) == 0) {
            std::cout << "Passage dans rfindSELL " << std::endl;
            // Traiter la commande SELL
            std::string response = handleSell(request);
            //send(clientSocket, response.c_str(), response.size(), 0);
        } 
        else {
            response = "Commande inconnue\n";
        }
        //send(clientSocket, response.c_str(), response.size(), 0);
        //std::cout << "Réponse envoyée : " << response << std::endl; 
        if (!response.empty()) {
            SSL_write(ssl, response.c_str(), response.length());
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Erreur dans ProcessRequest : " << e.what() << std::endl;
    }
}

Crypto crypto;
std::string Server::handleBuy(const std::string& request){
    // Extraire la paire de crypto et le montant de la requête
    std::istringstream iss(request);
    std::string action, currency;
    double percentage;
    if (!(iss >> action >> currency >> percentage)) {
        return "Erreur : Format de commande invalide\n";
    }
    if (percentage <=0 || percentage > 100) {
        return "Erreur : Pourcentage invalide\n";
    }
    crypto.buyCrypto(currency, percentage);
    return "Achat de " + std::to_string(percentage) + "% " + currency + " réussi\n";
}
std::string Server::handleSell(const std::string& request){
    // Extraire la paire de crypto et le montant de la requête
    std::istringstream iss(request);
    std::string action, currency;
    double percentage;
    if (!(iss >> action >> currency >> percentage)) {
        return "Erreur : Format de commande invalide\n";
    }
    if (percentage <=0 || percentage > 100) {
        return "Erreur : Pourcentage invalide\n";
    }
    crypto.sellCrypto(currency, percentage);
    return "Achat de " + std::to_string(percentage) + "% " + currency + " réussi\n";
}