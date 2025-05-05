#ifndef SERVER_H
#define SERVER_H

#include "ClientAuthenticator.h" // Le Server utilise ClientAuthenticator (dépendance logique)

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
#include <functional>

// Inclure les headers des composants utilisés ou gérés par le Server :
#include "OpenSSLDeleters.h"  
#include "ServerConnection.h"  
#include "ClientSession.h"     
#include "TransactionQueue.h"  
#include "Global.h"          
#include "Logger.h"           
#include "Utils.h"             

// Déclaration de la file de transactions globale (définie ailleurs, typiquement main_serv.cpp)
extern TransactionQueue txQueue;

// --- Classe Server ---
// La classe principale du serveur.
// Gère l'initialisation réseau/SSL, l'acceptation des connexions,
// la gestion centrale des utilisateurs et l'orchestration des sessions clientes,
// en déléguant l'authentification (protocole) et la logique de session post-authentification à d'autres classes.
class Server : public std::enable_shared_from_this<Server> { // Hérite pour pouvoir utiliser shared_from_this()
public:
    // Constructeur du serveur.
    // Param p: Port d'écoute.
    // Param certF: Chemin du fichier de certificat SSL.
    // Param keyF: Chemin du fichier de clé privée SSL.
    // Param usersFile: Chemin du fichier de persistance des utilisateurs.
    // Param transactionCounterFile: Chemin du fichier pour le compteur de transactions.
    // Param transactionHistoryFile: Chemin du fichier pour l'historique global des transactions.
    // Param walletsDir: Chemin du répertoire des portefeuilles clients.
    Server(int p, const std::string& certF, const std::string& keyF, const std::string& usersFile,
           const std::string& transactionCounterFile, const std::string& transactionHistoryFile,
           const std::string& walletsDir);

    // Destructeur du serveur.
    // S'assure que le serveur s'arrête proprement (sauvegarde des utilisateurs, arrêt des threads et sessions).
    ~Server();

    // Méthode principale pour démarrer le serveur.
    // Initialisation réseau/SSL, chargement des données, démarrage de la TransactionQueue,
    // démarrage du thread de prix Global, et boucle d'acceptation des connexions.
    void StartServer();

    // Méthode pour demander l'arrêt ordonné du serveur.
    // Signale l'arrêt, ferme le socket d'écoute, et attend la fin des threads et sessions.
    void StopServer();

    // Méthode appelée par une ClientSession lorsqu'elle s'arrête pour se retirer de la liste des sessions actives. Thread-safe.
    // Param clientId: L'ID du client dont la session s'arrête.
    void unregisterSession(const std::string& clientId);

    // Déclare ClientAuthenticator comme une classe amie pour qu'elle puisse accéder aux membres privés/protégés de Server.
    friend class ClientAuthenticator;


private: // Membres et méthodes internes au serveur

    // --- Membres de Configuration ---
    int port;
    std::string certFile_path;
    std::string keyFile_path;
    std::string usersFile_path;
    std::string transactionCounterFile_path;
    std::string transactionHistoryFile_path;
    std::string wallets_dir_path;

    // --- Membres liés à l'état du serveur et réseau principal ---
    int serverSocket = -1;
    UniqueSSLCTX ctx = nullptr;
    std::atomic<bool> acceptingConnections; // Flag pour contrôler la boucle d'acceptation.

    // --- Membres liés à la gestion centrale des utilisateurs et à la persistance ---
    // Map stockant les identifiants clients et leurs mots de passe HASHÉS + sel. Protégée par usersMutex.
    // Le format exact dépend de HashPasswordSecure. Ex: "ID" -> "hash+sel"
    std::unordered_map<std::string, std::string> users; // Stocke ID -> HASHED_PASSWORD_WITH_SALT
    std::mutex usersMutex; // Mutex pour protéger l'accès concurrent à 'users'.

    // --- Membres liés à la gestion des sessions clientes actives ---
    std::unordered_map<std::string, std::shared_ptr<ClientSession>> activeSessions;
    std::mutex sessionsMutex; // Mutex pour protéger l'accès concurrent à 'activeSessions'.

    // --- Membres liés aux threads gérés par le Server ---
    std::thread acceptThread; // Le thread principal qui exécute la boucle d'acceptation.

    // --- Méthodes internes d'aide ---

    // Méthodes d'aide pour l'initialisation et l'acceptation SSL.
    UniqueSSLCTX InitServerCTX(const std::string& certFile, const std::string& keyFile);
    UniqueSSL AcceptSSLConnection(SSL_CTX* ctx_raw, int clientSocket);

    // Méthode pour gérer une nouvelle connexion cliente acceptée. Lancée dans un thread séparé.
    // Elle orchestrera l'authentification (appelant ClientAuthenticator) et le lancement de la ClientSession.
    void HandleClient(int clientSocket, SSL* ssl_ptr);

    // Méthodes pour la persistance des utilisateurs (chargement/sauvegarde de la map 'users').
    // Appellent LoadUsersInternal/SaveUsersInternal sous le mutex.
    void LoadUsers(const std::string& filename); // Charge les utilisateurs depuis un fichier.
    void LoadUsersInternal(const std::string& filename); // Logique de chargement réelle, doit être appelée avec usersMutex verrouillé.
    void SaveUsers(const std::string& filename); // Sauvegarde les utilisateurs dans un fichier.
    void SaveUsersInternal(const std::string& filename); // Logique de sauvegarde réelle, doit être appelée avec usersMutex verrouillé.

    // Méthode pour créer le fichier portefeuille sur disque pour un nouvel utilisateur.
    bool CreateWalletFile(const std::string& clientId);

    // Méthode interne pour retirer une session de la map activeSessions. Protégée par sessionsMutex.
    void removeSession(const std::string& clientId);

    // Méthode interne pour la boucle d'acceptation des connexions. Exécutée par acceptThread.
    void AcceptLoop();


    // --- Méthode privée pour la gestion de l'authentification et des utilisateurs par le Server ---
    // C'est la méthode appelée par ClientAuthenticator::AuthenticateClient.

    // Traite une demande d'authentification (login ou enregistrement).
    // Vérifie les identifiants ou enregistre un nouvel utilisateur, gérant hachage, persistance, mutex.
    // Param userIdPlainText: L'ID de l'utilisateur potentiel.
    // Param passwordPlain: Le mot de passe en clair reçu.
    // Param authenticatedUserId: Si le retour est SUCCESS ou NEW, cet argument contiendra l'ID de l'utilisateur authentifié/enregistré.
    // Retourne le résultat de l'authentification (AuthOutcome::SUCCESS, AuthOutcome::NEW, AuthOutcome::FAIL).
    AuthOutcome processAuthRequest(const std::string& userIdPlainText, const std::string& passwordPlain, std::string& authenticatedUserId);

    // --- Membres pour le pool de threads ---
    std::vector<std::thread> threadPool; // Pool de threads pour gérer les connexions
    std::queue<std::function<void()>> taskQueue; // File de tâches pour les connexions
    std::mutex taskQueueMutex; // Mutex pour protéger l'accès à la file de tâches
    std::condition_variable taskQueueCV; // Variable conditionnelle pour notifier les threads
    std::atomic<bool> stopThreadPool; // Flag pour arrêter le pool de threads

    // Méthode pour les threads du pool
    void threadPoolWorker();

};

#endif 