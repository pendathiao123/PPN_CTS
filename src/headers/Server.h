#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <unordered_map> 
#include <mutex>
#include <vector>      
#include <memory>      
#include <thread>      
#include <atomic>      
#include <stdexcept>   
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>  
#include <unistd.h>     
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "../headers/OpenSSLDeleters.h" 
#include "../headers/Client.h"          
#include "../headers/BotSession.h"      
#include "../headers/TransactionQueue.h" 
#include "../headers/Global.h"          
#include "../headers/Logger.h"       


enum class AuthOutcome {
    FAIL,    // Authentification échouée
    SUCCESS, // Authentification réussie pour un utilisateur existant
    NEW      // Authentification réussie pour un nouvel utilisateur (compte créé)
};

// Déclaration de la file de transactions globale
extern TransactionQueue txQueue;


class Server {
public:
    // Constructeur : initialise le serveur avec les configurations
    // Il DOIT prendre TOUS les chemins de fichiers/répertoires nécessaires.
    Server(int p, const std::string& certF, const std::string& keyF, const std::string& usersFile,
           const std::string& transactionCounterFile, const std::string& transactionHistoryFile,
           const std::string& walletsDir); // Chemin du répertoire des wallets

    // Destructeur : s'assure que le serveur s'arrête proprement
    ~Server();

    // Méthode pour démarrer le serveur principal (boucle d'acceptation)
    void StartServer();

    // Méthode pour arrêter le serveur (signaler l'arrêt, joindre les threads si nécessaire)
    void StopServer();

    // Méthode pour désenregistrer une session (appelée par la session elle-même à l'arrêt)
    // Les sessions s'enregistrent aussi auprès de la TransactionQueue si globale.
    void unregisterSession(const std::string& clientId);


private: // Membres et méthodes internes au serveur

    // --- Membres de Configuration (passés au constructeur et stockés) ---
    int port; // Port d'écoute du serveur
    std::string certFile_path; // Chemin du fichier de certificat SSL
    std::string keyFile_path;  // Chemin du fichier de clé privée SSL
    std::string usersFile_path; // Chemin du fichier de sauvegarde des utilisateurs
    std::string transactionCounterFile_path; // Chemin du fichier pour la persistance du compteur
    std::string transactionHistoryFile_path; // Chemin du fichier pour l'historique des transactions
    std::string wallets_dir_path; // Chemin du répertoire des wallets


    // --- Membres liés à l'état du serveur et à la gestion réseau ---
    int serverSocket = -1; // Socket d'écoute principal du serveur (-1 quand non initialisé)
    UniqueSSLCTX ctx = nullptr; // Contexte SSL du serveur, géré par pointeur unique


    // --- Membres liés aux utilisateurs et à la persistance ---
    // Map pour stocker les utilisateurs (ID -> Token). Utilisée par Load/Save/HandleClient.
    std::unordered_map<std::string, std::string> users;
    // Mutex pour protéger l'accès concurrentiel à la map 'users' depuis plusieurs threads.
    std::mutex usersMutex;


    // --- Membres liés à la gestion des sessions clientes ---
    // Map pour stocker les sessions BotSession actives (Client ID -> shared_ptr<BotSession>).
    // Le shared_ptr maintient la session en vie tant qu'elle est enregistrée ici.
    std::unordered_map<std::string, std::shared_ptr<BotSession>> activeSessions;
    // Mutex pour protéger l'accès concurrentiel à la map 'activeSessions'.
    std::mutex sessionsMutex;

    // --- Méthodes internes d'aide ---

    // Méthodes d'aide pour OpenSSL
    UniqueSSLCTX InitServerCTX(const std::string& certFile, const std::string& keyFile);
    UniqueSSL AcceptSSLConnection(SSL_CTX* ctx_raw, int clientSocket); // Utilise UniqueSSL pour le résultat


    // Méthode pour gérer une connexion cliente entrante (après accept et handshake SSL).
    // Exécutée dans un thread séparé pour chaque client.
    // Prend le socket FD et le pointeur SSL* raw initial (sera géré par Client object).
    void HandleClient(int clientSocket, SSL* ssl_ptr);


    // Gère la boucle de réception/traitement des requêtes APRÈS authentification.
    // APPELÉE par HandleClient si l'authentification réussit.
    // Prend le shared_ptr<BotSession> car la session gère le Bot et le Client object pour ce client.
    void HandleAuthenticatedClient(std::shared_ptr<BotSession> session, const std::string& clientId, AuthOutcome authOutcome);



    // Méthode pour charger les utilisateurs depuis le fichier usersFile_path.
    // Appelée au démarrage du serveur (ex: dans le constructeur ou StartServer).
    // Elle charge les données directement dans la map membre 'users'.
    // Elle DOIT utiliser usersMutex pour protéger l'accès à 'users'.
    void LoadUsers(const std::string& filename);


    // Méthode pour sauvegarder les utilisateurs dans le fichier usersFile_path.
    // Appelée après la création d'un nouvel utilisateur ou à l'arrêt du serveur.
    // Elle sauvegarde la map membre 'users'.
    // Elle DOIT utiliser usersMutex pour protéger l'accès à 'users'.
    void SaveUsers(const std::string& filename);


    // Méthode pour traiter une requête spécifique reçue d'un client authentifié.
    // Appelée depuis HandleAuthenticatedClient. Elle gère le parsing et l'envoi de la réponse.
    // Elle prend la session pour accéder au Bot et au Client.
    // Elle DOIT envoyer la réponse VIA session->getClient()->send(...).
    void ProcessRequest(std::shared_ptr<BotSession> session, const std::string& request);

    // Méthode pour gérer l'échange et la vérification de l'authentification client.
    // Reçoit le message d'auth, parse, vérifie users map, gère AUTH FAIL réponse.
    // Retourne le résultat (FAIL, SUCCESS, NEW) et set clientId_out si succès.
    AuthOutcome AuthenticateClient(std::shared_ptr<Client> client_comm, std::string* clientId_out);

    // Méthode pour créer le fichier portefeuille pour un nouvel utilisateur.
    // Retourne true si succès, false si échec.
    bool CreateWalletFile(const std::string& clientId);

    void removeSession(const std::string& clientId);
};

#endif