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
    /* Variables */
    // Identifiant du client
    const int ID;
    // Token permettant au client d'acceder à son compte pour echanges finnanciers
    std::string TOKEN;
    // Socket utilisé par le client
    int clientSocket;
    // objet permettant d'établir des connexions TLS/SSL
    SSL_CTX *ctx;
    // structure de données permettant une communication SSL (Secure Sockets Layer)
    SSL *ssl;
    // structure spécifiant l'adresse du serveur
    struct sockaddr_in serverAddr;

    /* Méthodes */
    // Initialiser le contexte SSL pour le client
    SSL_CTX *InitClientCTX();
    // Établir une connexion SSL
    SSL *ConnectSSL(SSL_CTX *ctx, int clientSocket);
    // affichages dans le terminal
    void affiche(std::string msg);
    // affichage des erreurs dans le terminal
    void afficheErr(std::string err);
    // envoie d'une requête
    int sendRequest(const std::string &request);
    // reception des reponses du serveur
    std::string receiveResponse();

public:
    // Constructeur de classe
    Client(int id);
    // Destructeur pour s'assurer de la fermeture de la connexion SSL
    ~Client();
    // Fonction principale pour démarrer le client (connecter le client au Serveur)
    void StartClient(const std::string &serverAddress, int port);

    // Méthode pour mettre de l'argent dans son solde
    void inject(const double money);
    // Méthode d'achat de cryptomonaie
    void buy(const std::string &currency, double amount);
    // Méthode de vente de cryptomonaie
    void sell(const std::string &currency, double amount);
    // Méthode qui fait appel à la technique d'investissement du serveur, fait par les Bots
    void invest();
    // Méthode qui fait appel à la technique de trading du serveur, fait par les Bots
    void trade();
    // Fermeture de la connexion SSL
    void closeConnection();
    // Renvoie si le Client est connecté ou pas
    bool isConnected() const;
    // Deconnexion du Serveur
    void EndClient();

};

#endif // CLIENT_H