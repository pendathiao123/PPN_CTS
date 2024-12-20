#ifndef SERVER_H
#define SERVER_H

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unordered_map>
#include <string>
#include <fstream>

// Fonction pour générer une chaîne aléatoire
std::string GenerateRandomString(size_t length);

// Fonction pour générer un ID à 4 chiffres aléatoires
std::string GenerateRandomId();

// Fonction pour générer un token avec HMAC
std::string GenerateToken();

// Charger les utilisateurs depuis un fichier
std::unordered_map<std::string, std::string> LoadUsers(const std::string& filename);

// Sauvegarder les utilisateurs dans un fichier
void SaveUsers(const std::string& filename, const std::unordered_map<std::string, std::string>& users);

// Initialiser le contexte SSL pour le serveur
SSL_CTX* InitServerCTX(const std::string& certFile, const std::string& keyFile);

// Accepter une connexion SSL
SSL* AcceptSSLConnection(SSL_CTX* ctx, int clientSocket);

// Gérer une connexion client
void HandleClient(SSL* ssl, std::unordered_map<std::string, std::string>& users, const std::string& usersFile);


class Server {
public : 
    // Fonction principale pour démarrer le serveur
    void StartServer(int port, const std::string& certFile, const std::string& keyFile, const std::string& usersFile);

    void ProcessRequest(int clientSocket);
    std::string handleBuy(const std::string& request);
    std::string handleSell(const std::string& request);
private : 
    SSL* ssl;
};
#endif // SERVER_H
