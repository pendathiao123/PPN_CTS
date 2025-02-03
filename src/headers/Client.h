#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <memory>
#include <openssl/ssl.h>
#include <unordered_map>
#include <netinet/in.h>

class Bot;

class Client
{
private:
    // Socket utilisé par le client
    int clientSocket;
    // objet permettant d'établir des connexions TLS/SSL
    SSL_CTX *ctx;
    // structure de données permettant une communication SSL (Secure Sockets Layer)
    SSL *ssl;
    // structure spécifiant l'adresse du serveur
    struct sockaddr_in serverAddr;
    std::shared_ptr<Bot> tradingBot;
    // Initialiser le contexte SSL pour le client
    SSL_CTX *InitClientCTX();
    // Établir une connexion SSL
    SSL *ConnectSSL(SSL_CTX *ctx, int clientSocket);
    // envoie d'une requête
    void sendRequest(const std::string &request);
    // reception des reponses du serveur
    std::string receiveResponse();

public:
    // Constructeur de classe
    Client();
    // Destructeur
    ~Client();
    // Fonction principale pour démarrer le client
    void StartClient(const std::string &serverAddress, int port, const std::string &clientId, const std::string &clientToken);
    // Méthode d'achat de cryptomonaie
    void buy(const std::string &currency, double percentage);
    // Méthode de vente de cryptomonaie
    void sell(const std::string &currency, double percentage);
    // Fermeture de la connexion
    void closeConnection();
    // Renvoie si le Client est connecté ou pas
    bool isConnected() const;

};

#endif // CLIENT_H