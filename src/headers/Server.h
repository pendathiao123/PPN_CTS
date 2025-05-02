// src/headers/Server.h - En-tête de la classe Server

#ifndef SERVER_H
#define SERVER_H

#include "ClientAuthenticator.h" // Le Server utilise ClientAuthenticator (dépendance logique)

// --- Includes nécessaires pour les déclarations dans cet en-tête ---
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <memory> // shared_ptr, unique_ptr, enable_shared_from_this
#include <thread>
#include <atomic>
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

// Inclure les headers des composants utilisés ou gérés par le Server :

#include "OpenSSLDeleters.h"   // Pour UniqueSSLCTX et UniqueSSL
#include "ServerConnection.h"  // Le Server crée des ServerConnection
#include "ClientSession.h"     // Le Server crée et gère des ClientSession
#include "TransactionQueue.h"  // Le Server interagit avec la TQ globale
#include "Global.h"            // Le Server interagit avec le module global (prix, etc.)
#include "Logger.h"            // Pour le logging (macro LOG)
#include "Utils.h"             // Pour les fonctions utilitaires globales (GenerateToken, etc.)

// Déclaration de la file de transactions globale (définie ailleurs, typiquement main_serv.cpp)
extern TransactionQueue txQueue;

/*
 * @brief La classe principale du serveur.
 *
 * Gère l'initialisation réseau/SSL, l'acceptation des connexions,
 * la gestion centrale des utilisateurs et l'orchestration des sessions clientes,
 * en déléguant l'authentification (protocole) et la logique de session post-authentification à d'autres classes.
 */
class Server : public std::enable_shared_from_this<Server> { // Hérite pour pouvoir utiliser shared_from_this()
public:
    /**
     * @brief Constructeur du serveur.
     * @param p Port d'écoute.
     * @param certF Chemin du fichier de certificat SSL.
     * @param keyF Chemin du fichier de clé privée SSL.
     * @param usersFile Chemin du fichier de persistance des utilisateurs.
     * @param transactionCounterFile Chemin du fichier pour le compteur de transactions.
     * @param transactionHistoryFile Chemin du fichier pour l'historique global des transactions.
     * @param walletsDir Chemin du répertoire des portefeuilles clients.
     */
    Server(int p, const std::string& certF, const std::string& keyF, const std::string& usersFile,
           const std::string& transactionCounterFile, const std::string& transactionHistoryFile,
           const std::string& walletsDir);

    /**
     * @brief Destructeur du serveur.
     *
     * S'assure que le serveur s'arrête proprement (sauvegarde des utilisateurs, arrêt des threads et sessions).
     */
    ~Server();

    /**
     * @brief Méthode principale pour démarrer le serveur.
     *
     * Initialisation réseau/SSL, chargement des données, démarrage de la TransactionQueue,
     * démarrage du thread de prix Global, et boucle d'acceptation des connexions.
     */
    void StartServer();

    /**
     * @brief Méthode pour demander l'arrêt ordonné du serveur.
     *
     * Signale l'arrêt, ferme le socket d'écoute, et attend la fin des threads et sessions.
     */
    void StopServer();

    /**
     * @brief Méthode appelée par une ClientSession lorsqu'elle s'arrête pour se retirer de la liste des sessions actives. Thread-safe.
     * @param clientId L'ID du client dont la session s'arrête.
     */
    void unregisterSession(const std::string& clientId);

    // Déclare ClientAuthenticator comme une classe amie pour qu'elle puisse accéder aux membres privés/protégés de Server.
    friend class ClientAuthenticator;

    // --- Nouvelles méthodes publiques pour permettre à ClientAuthenticator d'interagir avec la gestion utilisateur ---
    // (Alternative: les rendre privées et passer le Server *this* à AuthenticateClient)
    // Rendre publiques ces méthodes permet à l'authentificateur d'appeler directement la logique de gestion utilisateur du Server.
    // Le choix entre public/privé dépend du niveau d'encapsulation souhaité. Rendons-les privées et passons *this*.

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
    void LoadUsersInternal(const std::string& filename); // Logique de chargement réelle, *doit* être appelée avec usersMutex verrouillé. // <-- Renommé pour clarté
    void SaveUsers(const std::string& filename); // Sauvegarde les utilisateurs dans un fichier.
    void SaveUsersInternal(const std::string& filename); // Logique de sauvegarde réelle, *doit* être appelée avec usersMutex verrouillé. // <-- Renommé pour clarté

    // Méthode pour créer le fichier portefeuille sur disque pour un nouvel utilisateur.
    bool CreateWalletFile(const std::string& clientId);

    // Méthode interne pour retirer une session de la map activeSessions. Protégée par sessionsMutex.
    void removeSession(const std::string& clientId);

    // Méthode interne pour la boucle d'acceptation des connexions. Exécutée par acceptThread.
    void AcceptLoop();


    // --- Méthode privée pour la gestion de l'authentification et des utilisateurs par le Server ---
    // C'est la méthode appelée par ClientAuthenticator::AuthenticateClient.

    /**
     * @brief Traite une demande d'authentification (login ou enregistrement).
     * Vérifie les identifiants ou enregistre un nouvel utilisateur, gérant hachage, persistance, mutex.
     * @param userIdPlainText L'ID de l'utilisateur potentiel.
     * @param passwordPlain Le mot de passe en clair reçu.
     * @param authenticatedUserId [out] Si le retour est SUCCESS ou NEW, cet argument contiendra l'ID de l'utilisateur authentifié/enregistré.
     * @return Le résultat de l'authentification (AuthOutcome::SUCCESS, AuthOutcome::NEW, AuthOutcome::FAIL).
     */
    AuthOutcome processAuthRequest(const std::string& userIdPlainText, const std::string& passwordPlain, std::string& authenticatedUserId); // <-- DECLARATION CORRECTE

    // TODO: Ajouter d'autres méthodes de gestion utilisateur si nécessaire (ex: changer mot de passe, supprimer utilisateur).

}; // Fin de la classe Server

#endif //SERVER_H