#ifndef SERVER_H
#define SERVER_H

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unordered_map>
#include <string>
#include <fstream>
#include <memory>
#include <vector>
#include <array>
#include "Bot.h"
#include "Crypto.h"

// Fonction pour générer une chaîne de caractères aléatoire
std::string GenerateRandomString(size_t length);

// Fonction pour générer un ID aléatoire à 4 chiffres
std::string GenerateRandomId();

// Fonction pour générer un jeton avec HMAC
std::string GenerateToken();

// Charger les utilisateurs à partir d'un fichier
std::unordered_map<std::string, std::string> LoadUsers(const std::string &filename);

// Sauvegarder les utilisateurs dans un fichier
void SaveUsers(const std::string &filename, const std::unordered_map<std::string, std::string> &users);

// Initialiser le contexte SSL pour le serveur
SSL_CTX *InitServerCTX(const std::string &certFile, const std::string &keyFile);

// Accepter une connexion SSL
SSL *AcceptSSLConnection(SSL_CTX *ctx, int clientSocket);

class Server
{
private:
    /* Variables du serveur */
    // port atribué au serveur
    const int PORT;

    // unordered map des comptes clients
    std::unordered_map<std::string, std::string> users;
    // chemin vers fichier contenant les comptes utilisateurs
    const std::string usersFile;

    // chemin vers fichier des logs (transactions de $)
    const std::string logFile;

    // pointeur intelligent vers le Bot fourni par le serveur
    std::shared_ptr<Bot> serverBot;

    // lien vers la classe qui gère les cryptos
    Crypto crypto_monaie;
    // structure pour gerer les soldes des tous les clients
    std::unordered_map<std::string,std::array<double,2>> soldes;

    /*
    // structure de données pour le compte d'un client    
    struct portfolio{
        std::string id_Client;
        double currency[2]: // pour l'instant on se limite à 2: [$,SRD-BTC]
    };
    // structure pour gerer les soldes des tous les clients:
    std::vector soldes<portfolio>;
    */
    

    /* Méthodes privés du serveur */
    // Gestion des affichages dans le terminal
    void affiche(std::string msg);
    // Gestion des affichage d'erreurs dans le terminal
    void afficheErr(std::string err);

    // Fonction pour gérer la reception de requêtes
    std::string receiveRequest(SSL *ssl);
    // Fonction pour gérer l'envoie de reponses
    int sendResponse(SSL *ssl, const std::string &response);

    // Créer un nouveau compte client:
    std::string newConnection(const std::string idClient);
    // Gérer les connexions des clients
    int Connection(SSL *ssl, const std::string idClient, std::string msgClient);
    // Gérer les deconnxions des clients
    std::string DeConnection(const std::string idClient);

    //==================Méthodes critiques !!==================
    // Méthode pour mettre l'argent des clients dans leur solde
    std::string putMoney(std::string idClient, std::string order);
    // Méthode pour recuperer tout l'argent des clients de leur solde
    std::string takeMoney(std::string idClient);
    // Méthoe pour vendre les tous les actifs du client et recuperer tout l'argent dans le solde du client
    std::string takeAllMoney(std::string idClient);
    // Méthode pour acheter des Cryptos
    int buyCrypto(const std::string& id, const std::string& crypto, double q);
    // Méthode pour vendre des Cryptos
    int sellCrypto(const std::string& id, const std::string& crypto, double q);
    //=========================================================

    // Gérer une requête d'achat
    std::string handleBuy(const std::string &request, const std::string &clientId);

    // Gérer une requête de vente
    std::string handleSell(const std::string &request, const std::string &clientId);

    // Fonction qui gere les Bots dans le serveur
    std::string serverUseBot(const std::string id, const int a);

    // Gérer une connexion client
    void HandleClient(SSL *ssl);    

public:

    /* Méthodes publiques du serveur */
    // Constructeur
    Server(int prt, const std::string &uFile, const std::string &lFile);

    // Destructeur (comportement normal)
    ~Server()=default;

    // Fonction principale pour démarrer le serveur
    void StartServer(const std::string &certFile, const std::string &keyFile);
};

#endif // SERVER_H