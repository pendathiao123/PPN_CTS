// --- Implémentation de la classe Server ---

#include "../headers/ClientAuthenticator.h"  // Le Server utilise ClientAuthenticator (dépendance logique)
#include "../headers/Server.h"
#include "../headers/ServerConnection.h"     // Le Server crée des ServerConnection
#include "../headers/ClientSession.h"        // Le Server crée et gère des ClientSession
#include "../headers/Transaction.h"          // Pour la méthode statique loadCounter/saveCounter
#include "../headers/TransactionQueue.h"     // Le Server interagit avec la TQ globale
#include "../headers/Global.h"               // Le Server interagit avec le module Global (prix, etc.)
#include "../headers/Logger.h"               // Pour le logging
#include "../headers/OpenSSLDeleters.h"      // Pour UniqueSSLCTX, UniqueSSL (gestion RAII)
#include "../headers/Utils.h"                // Pour les fonctions utilitaires globales (GenerateToken, HashPasswordSecure, etc.)

// Includes pour les fonctionnalités standards et systèmes utilisées dans ce .cpp.
#include <openssl/ssl.h>       // Fonctions SSL (SSL_accept, SSL_set_fd, SSL_CTX_new etc.)
#include <openssl/err.h>       // Gestion des erreurs SSL (ERR_*)
#include <openssl/x509.h>      // X509_verify_cert_error_string, X509_V_OK

#include <iostream>            // std::cerr, std::endl
#include <sstream>             // std::stringstream, std::istringstream, std::ostringstream
#include <cstring>             // strerror, memset
#include <arpa/inet.h>         // inet_ntoa, inet_pton
#include <unistd.h>            // close
#include <fstream>             // std::ifstream, std::ofstream
#include <unordered_map>       // std::unordered_map
#include <ctime>               // time_t
#include <chrono>              // std::chrono
#include <thread>              // std::thread
#include <filesystem>          // std::filesystem (pour create_directories)
#include <memory>              // std::shared_ptr, std::unique_ptr, std::make_shared
#include <system_error>        // std::error_code
#include <functional>          // std::function
#include <vector>              // std::vector
#include <cerrno>              // errno
#include <fcntl.h>             // fcntl
#include <algorithm>           // std::transform
#include <cctype>              // ::tolower


// Définition de l'instance globale de la TransactionQueue (déclarée extern dans TransactionQueue.h)
// C'est le SEUL endroit où cette instance est définie. Placé dans main_serv.cpp selon notre discussion.
extern TransactionQueue txQueue;


// --- Fonctions Utilitaires Globales ---
// Les implémentations de GenerateRandomHex, GenerateRandomId, GenerateToken, HashPasswordSecure, VerifyPasswordSecure
// sont maintenant dans Utils.cpp. Leurs déclarations sont dans Utils.h.


// --- Implémentation des méthodes membres de la classe Server ---

// --- Constructeur Server ---
Server::Server(int p, const std::string& certF, const std::string& keyF, const std::string& usersF,
    const std::string& transactionCounterF, const std::string& transactionHistoryF,
    const std::string& walletsD)
: port(p),
certFile_path(certF),
keyFile_path(keyF),
usersFile_path(usersF),
transactionCounterFile_path(transactionCounterF),
transactionHistoryFile_path(transactionHistoryF),
wallets_dir_path(walletsD),
serverSocket(-1),
ctx(nullptr),
acceptingConnections(false)
{
    LOG("Server::Server INFO : Objet Server créé avec port " + std::to_string(this->port) + " et chemins de configuration.", "INFO");
}

// --- Destructeur Server ---
Server::~Server() {
    LOG("Server::~Server INFO : Destructeur appelé. Début de l'arrêt propre...", "INFO");
    StopServer();
    LOG("Server::~Server INFO : Destructeur terminé.", "INFO");
}


// --- Implémentation de la méthode Server::StartServer ---
void Server::StartServer() {
    LOG("Server::StartServer INFO : Démarrage du serveur sur le port " + std::to_string(this->port) + "...", "INFO");

    // 1. Initialiser les bibliothèques OpenSSL (si pas déjà fait dans main).
    // Idéalement, l'initialisation/nettoyage global OpenSSL est fait une seule fois dans main().
    // SSL_library_init();
    // SSL_load_error_strings();
    // OpenSSL_add_all_algorithms();


    // 2. Charger le contexte SSL du serveur.
    this->ctx = InitServerCTX(this->certFile_path, this->keyFile_path);
    if (!this->ctx) {
        LOG("Server::StartServer ERROR : Échec de l'initialisation du contexte SSL serveur. Arrêt.", "ERROR");
        return;
    }
    LOG("Server::StartServer INFO : Contexte SSL serveur initialisé avec succès.", "INFO");


    // 3. Charger les utilisateurs depuis le fichier. Utilise la méthode interne sécurisée.
    this->LoadUsers(this->usersFile_path);
    LOG("Server::StartServer INFO : Chargement des utilisateurs terminé (ou fichier non trouvé).", "INFO");


    // 4. Charger le compteur de transactions statique (géré par la classe Transaction).
    Transaction::loadCounter(this->transactionCounterFile_path);
    LOG("Server::StartServer INFO : Chargement du compteur de transactions terminé.", "INFO");


    // 5. Démarrer le thread de génération des prix SRD-BTC (module Global).
    Global::startPriceGenerationThread();
    LOG("Server::StartServer INFO : Thread de génération des prix SRD-BTC démarré (module Global).", "INFO");


    // 6. Démarrer le thread de traitement de la TransactionQueue globale.
    txQueue.start();
    LOG("Server::StartServer INFO : Thread de traitement de la TransactionQueue démarré.", "INFO");


    // 7. Configuration et liaison du socket serveur principal.
    this->serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (this->serverSocket == -1) {
        LOG("Server::StartServer ERROR : Échec de la création de la socket serveur. Erreur: " + std::string(strerror(errno)), "ERROR");
        StopServer(); // Nettoie ce qui a été démarré.
        return;
    }
    LOG("Server::StartServer DEBUG : Socket serveur créé avec FD: " + std::to_string(this->serverSocket), "DEBUG");

    // Configurer l'option SO_REUSEADDR.
    int opt = 1;
    if (setsockopt(this->serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG("Server::StartServer WARNING : setsockopt(SO_REUSEADDR) a échoué. Erreur: " + std::string(strerror(errno)), "WARNING");
    }

    // Configure l'adresse et le port d'écoute.
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(this->port);

    // Lie l'adresse et le port.
    if (bind(this->serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        LOG("Server::StartServer ERROR : Échec de la liaison (bind) de la socket serveur. Port : " + std::to_string(this->port) + ". Erreur: " + std::string(strerror(errno)), "ERROR");
        close(this->serverSocket); this->serverSocket = -1;
        StopServer();
        return;
    }
    LOG("Server::StartServer INFO : Socket serveur lié à l'adresse et au port " + std::to_string(this->port), "INFO");


    // Commence à écouter les connexions.
    if (listen(this->serverSocket, SOMAXCONN) < 0) {
        LOG("Server::StartServer ERROR : Échec de l'écoute (listen) sur la socket serveur. Erreur: " + std::string(strerror(errno)), "ERROR");
        close(this->serverSocket); this->serverSocket = -1;
        StopServer();
        return;
    }
    LOG("Server::StartServer INFO : Serveur en écoute sur le port " + std::to_string(this->port), "INFO");


    // 8. Lancer la boucle d'acceptation des connexions dans un thread séparé.
    this->acceptingConnections.store(true, std::memory_order_release);
    this->acceptThread = std::thread(&Server::AcceptLoop, this);
    LOG("Server::StartServer INFO : Thread d'acceptation des connexions démarré.", "INFO");

    // La méthode StartServer() devient bloquante en joignant le thread d'acceptation.
    LOG("Server::StartServer DEBUG : Attente de la fin du thread d'acceptation...", "DEBUG");
    this->acceptThread.join();
    LOG("Server::StartServer DEBUG : Thread d'acceptation joint dans StartServer.", "DEBUG");

    // StartServer se termine ici après l'arrêt propre de la boucle d'acceptation.
}


// --- Implémentation de la méthode Server::StopServer ---
void Server::StopServer() {
    static std::atomic<bool> stop_in_progress(false);
    bool expected = false;
    if (!stop_in_progress.compare_exchange_strong(expected, true)) {
        LOG("Server::StopServer INFO : Arrêt déjà en cours. Appel ignoré.", "INFO");
        return;
    }

    LOG("Server::StopServer INFO : Arrêt ordonné du serveur demandé...", "INFO");

    // 1. Signaler au thread d'acceptation de s'arrêter.
    this->acceptingConnections.store(false, std::memory_order_release);
    LOG("Server::StopServer DEBUG : acceptingConnections flag positionné à false.", "DEBUG");

    // 2. Débloquer la socket d'écoute.
    if (this->serverSocket != -1) {
        LOG("Server::StopServer DEBUG : Fermeture de la socket d'écoute (FD " + std::to_string(this->serverSocket) + ") pour débloquer accept().", "DEBUG");
        close(this->serverSocket);
        this->serverSocket = -1;
    } else {
         LOG("Server::StopServer DEBUG : Socket d'écoute déjà fermé (-1).", "DEBUG");
    }

    // 3. Attendre la fin du thread d'acceptation.
    if (this->acceptThread.joinable()) {
        this->acceptThread.join();
        LOG("Server::StopServer INFO : Thread d'acceptation joint.", "INFO");
    } else {
        LOG("Server::StopServer WARNING : Thread d'acceptation non joignable lors de l'arrêt.", "WARNING");
    }

    // 4. Signaler l'arrêt à toutes les sessions clientes actives et attendre leur fin.
    std::vector<std::shared_ptr<ClientSession>> sessions_to_stop;
    {
        std::lock_guard<std::mutex> lock(this->sessionsMutex);
        LOG("Server::StopServer DEBUG : Copie de " + std::to_string(this->activeSessions.size()) + " sessions actives pour arrêt.", "DEBUG");
        sessions_to_stop.reserve(this->activeSessions.size());
        for (auto const& [clientId, session] : this->activeSessions) {
            if (session) {
                sessions_to_stop.push_back(session);
            }
        }
    } // Le lock est libéré.

    LOG("Server::StopServer INFO : Signal d'arrêt envoyé à " + std::to_string(sessions_to_stop.size()) + " sessions clientes actives. Attente de leur fin...", "INFO");
    for (const auto& session : sessions_to_stop) {
         if (session) {
              session->stop(); // stop() de ClientSession doit joindre son thread interne.
         }
    }
    LOG("Server::StopServer DEBUG : Appel stop() terminé pour toutes les sessions copiées.", "DEBUG");

    // 5. Sauvegarder la liste des utilisateurs sur disque. Utilise la méthode interne sécurisée.
    this->SaveUsers(this->usersFile_path);
    LOG("Server::StopServer INFO : Liste des utilisateurs sauvegardée.", "INFO");

    // 6. Sauvegarder le compteur de transactions statique.
    Transaction::saveCounter(this->transactionCounterFile_path);
    LOG("Server::StopServer INFO : Compteur de transactions sauvegardé.", "INFO");

    // 7. Signaler l'arrêt au thread de génération des prix (Global) et attendre sa fin.
    Global::stopPriceGenerationThread();
    LOG("Server::StopServer INFO : Thread de génération des prix arrêté.", "INFO");

    // 8. Signaler l'arrêt au thread de traitement de la TransactionQueue et attendre sa fin.
    txQueue.stop();
    LOG("Server::StopServer INFO : Thread de traitement de la TransactionQueue arrêté.", "INFO");

    // 9. Nettoyage final des ressources globales OpenSSL si nécessaire (dans main).
    /*
    ERR_free_strings();
    EVP_cleanup();
    SSL_COMP_free_compression_methods();
    CRYPTO_cleanup_all_ex_data();
    */

    LOG("Server::StopServer INFO : Arrêt ordonné du serveur terminé.", "INFO");
    stop_in_progress.store(false, std::memory_order_release);
}


// --- Implémentation de la méthode Server::unregisterSession ---
void Server::unregisterSession(const std::string& clientId) {
    LOG("Server::unregisterSession INFO : Demande de désenregistrement session client ID: " + clientId + " reçue.", "INFO");
    removeSession(clientId); // Utilise la méthode interne thread-safe.
}


// --- Implémentation de la méthode Server::removeSession ---
void Server::removeSession(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(this->sessionsMutex);
    size_t removed_count = this->activeSessions.erase(clientId);
    if (removed_count > 0) {
        LOG("Server::removeSession DEBUG : Session client ID: " + clientId + " retirée de activeSessions.", "DEBUG");
    } else {
        LOG("Server::removeSession WARNING : Tentative de retirer session ID: " + clientId + ", non trouvée.", "WARNING");
    }
}


// --- Implémentation de la méthode Server::CreateWalletFile ---
bool Server::CreateWalletFile(const std::string& clientId) {
    std::string walletFilename = this->wallets_dir_path + "/" + clientId + ".wallet";
    LOG("Server::CreateWalletFile DEBUG : Tentative création fichier portefeuille: " + walletFilename, "DEBUG");

    std::error_code ec;
    bool dir_exists_or_created = std::filesystem::create_directories(this->wallets_dir_path, ec);

    if (!dir_exists_or_created && ec) {
        LOG("Server::CreateWalletFile ERROR : Impossible créer/vérifier répertoire portefeuilles: " + this->wallets_dir_path + ". Erreur: " + ec.message(), "ERROR");
        return false;
    } else if (dir_exists_or_created) {
         LOG("Server::CreateWalletFile DEBUG : Répertoire portefeuilles vérifié/créé : " + this->wallets_dir_path, "DEBUG");
    } else {
         LOG("Server::CreateWalletFile WARNING : std::filesystem::create_directories retourné false mais ec non set. État inconnu répertoire.", "WARNING");
    }

    std::ofstream walletFile(walletFilename); // Mode 'out' par défaut, troncature implicite.
    if (!walletFile.is_open()) {
        LOG("Server::CreateWalletFile ERROR : Impossible créer/ouvrir fichier portefeuille pour client ID: " + clientId + " chemin: " + walletFilename + ". Erreur: " + std::string(strerror(errno)), "ERROR");
        return false;
    }
    LOG("Server::CreateWalletFile DEBUG : Fichier portefeuille ouvert/créé : " + walletFilename, "DEBUG");

    walletFile << "USD 10000.0\n";
    walletFile << "SRD-BTC 0.0\n";

    if (walletFile.fail()) {
        LOG("Server::CreateWalletFile ERROR : Erreur écriture soldes initiaux fichier portefeuille: " + walletFilename + ". Erreur: " + std::string(strerror(errno)), "ERROR");
        walletFile.close();
        return false;
    }

    walletFile.close();
    LOG("Server::CreateWalletFile INFO : Fichier portefeuille vierge créé et initialisé : " + walletFilename, "INFO");

    return true;
}


// --- Implémentation de la méthode Server::LoadUsers ---
// Charge les utilisateurs depuis un fichier. Appelle LoadUsersInternal sous lock.
void Server::LoadUsers(const std::string& filename) {
    std::lock_guard<std::mutex> lock(this->usersMutex);
    LoadUsersInternal(filename);
}

// Logique de chargement des utilisateurs réelle. DOIT être appelée avec usersMutex déjà verrouillé.
void Server::LoadUsersInternal(const std::string& filename) {
    // ASSUMPTION: usersMutex est déjà verrouillé par l'appelant (LoadUsers).

    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG("Server::LoadUsersInternal WARNING : Fichier utilisateurs non trouvé ou inaccessible: " + filename + ". Map utilisateurs sera vide. Erreur système: " + std::string(strerror(errno)), "WARNING");
        this->users.clear(); // Assure que la map est vide si le fichier n'existe pas.
        return;
    }

    this->users.clear(); // Nettoie la map avant de charger.
    std::string line;
    size_t loaded_count = 0;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream stream(line);
        std::string id;
        std::string password_hash; // Nom plus précis pour un mdp hashé.

        if (std::getline(stream, id, ' ')) {
            std::getline(stream, password_hash); // Lit le reste de la ligne.

            id.erase(0, id.find_first_not_of(" \t\n\r\f\v"));
            id.erase(id.find_last_not_of(" \t\n\r\f\v") + 1);
            password_hash.erase(0, password_hash.find_first_not_of(" \t\n\r\f\v"));
            password_hash.erase(password_hash.find_last_not_of(" \t\n\r\f\v") + 1);

            if (!id.empty()) {
                 // Stocke l'ID et le HASH (qui est dans password_hash).
                 this->users[id] = password_hash;
                 loaded_count++;
            } else {
                 LOG("Server::LoadUsersInternal WARNING : Ligne invalide trouvée (ID vide) : '" + line + "'.", "WARNING");
            }
        } else {
             LOG("Server::LoadUsersInternal WARNING : Ligne invalide trouvée (format incorrect) : '" + line + "'.", "WARNING");
        }
    }

    file.close();
    LOG("Server::LoadUsersInternal INFO : Chargement utilisateurs terminé. " + std::to_string(loaded_count) + " entrées chargées depuis " + filename, "INFO");
}

// --- Implémentation de la méthode Server::SaveUsers ---
// Sauvegarde les utilisateurs dans un fichier. Appelle SaveUsersInternal sous lock.
void Server::SaveUsers(const std::string& filename) {
    std::lock_guard<std::mutex> lock(this->usersMutex);
    SaveUsersInternal(filename); // Appelle la logique interne sous lock.
}

// Logique de sauvegarde des utilisateurs réelle. DOIT être appelée avec usersMutex déjà verrouillé.
void Server::SaveUsersInternal(const std::string& filename) {
    // ASSUMPTION: usersMutex est déjà verrouillé par l'appelant (SaveUsers ou attemptRegistration).

    std::ofstream file(filename, std::ios::trunc);
    if (!file.is_open()) {
        LOG("Server::SaveUsersInternal ERROR : Impossible d'écrire fichier utilisateurs: " + filename + ". Erreur: " + std::string(strerror(errno)), "ERROR");
        return;
    }

    for (const auto& pair : this->users) {
        // Écrit l'ID et le HASH.
        file << pair.first << " " << pair.second << "\n";
    }

    file.close();
    LOG("Server::SaveUsersInternal INFO : Sauvegarde de " + std::to_string(this->users.size()) + " utilisateurs dans " + filename, "INFO");
}


// --- Implémentation de la NOUVELLE méthode privée Server::processAuthRequest ---
// C'est l'implémentation que le compilateur ne trouvait pas.
// Elle combine la logique de vérification et d'enregistrement.

AuthOutcome Server::processAuthRequest(const std::string& userIdPlainText, const std::string& passwordPlain, std::string& authenticatedUserId) {
    LOG("Server::processAuthRequest INFO : Tentative auth/enregistrement pour ID: '" + userIdPlainText + "'...", "INFO");

    // Initialise le paramètre de sortie pour s'assurer qu'il est vide par défaut si l'auth échoue.
    authenticatedUserId.clear();

    // Vérifications de base (peut être fait dans Authenticator, mais redondance n'est pas nuisible).
    if (userIdPlainText.empty() || passwordPlain.empty()) {
        LOG("Server::processAuthRequest WARNING : ID ou Mot de passe vide fourni pour ID: '" + userIdPlainText + "'.", "WARNING");
        return AuthOutcome::FAIL;
    }

    // Accède à la map users de manière thread-safe pour vérifier si l'ID existe.
    // Le lock_guard gère automatiquement le verrouillage/déverrouillage.
    std::lock_guard<std::mutex> lock(this->usersMutex);

    auto it = this->users.find(userIdPlainText); // Cherche l'ID dans la map.

    if (it != this->users.end()) {
        // --- Cas : Utilisateur existant ---
        LOG("Server::processAuthRequest DEBUG : ID existant trouvé : '" + userIdPlainText + "'. Vérification du mot de passe...", "DEBUG");

        // Vérifie le mot de passe en clair par rapport au HASH stocké (it->second).
        // !!! C'est ici que la VÉRIFICATION SÉCURISÉE DOIT avoir lieu !!!
        // Appelle la fonction sécurisée VerifyPasswordSecure (déclarée dans Utils.h, implémentée dans Utils.cpp).
        // bool password_match = VerifyPasswordSecure(passwordPlain, it->second);
        bool password_match = VerifyPasswordSecure(passwordPlain, it->second); // Placeholder INSECURE - IMPLÉMENTER SECURISÉ !

        if (password_match) {
            LOG("Server::processAuthRequest INFO : Authentification réussie pour ID existant : '" + userIdPlainText + "'.", "INFO");
            authenticatedUserId = userIdPlainText; // Retourne l'ID via le paramètre de sortie.
            return AuthOutcome::SUCCESS; // Authentification réussie.
        } else {
            LOG("Server::processAuthRequest WARNING : Mot de passe incorrect pour ID existant : '" + userIdPlainText + "'.", "WARNING");
            authenticatedUserId.clear(); // S'assurer qu'il est vide en cas d'échec.
            return AuthOutcome::FAIL; // Mot de passe incorrect.
        }

    } else {
        // --- Cas : Nouvel utilisateur ---
        LOG("Server::processAuthRequest DEBUG : ID non trouvé : '" + userIdPlainText + "'. Tentative d'enregistrement...", "DEBUG");

         // !!! C'est ici que le HACHAGE SÉCURISÉ DOIT avoir lieu pour le nouveau mot de passe !!!
        // Le mot de passe en clair (passwordPlain) doit être hashé avec un sel unique.
        // std::string hashedPasswordWithSalt = HashPasswordSecure(passwordPlain);
        std::string password_to_store = HashPasswordSecure(passwordPlain); // Placeholder INSECURE - IMPLÉMENTER SECURISÉ !

        if (password_to_store.empty()) {
             LOG("Server::processAuthRequest ERROR : Échec du hachage sécurisé du mot de passe pour nouvel ID: '" + userIdPlainText + "'. Annulation enregistrement.", "ERROR");
             authenticatedUserId.clear();
             return AuthOutcome::FAIL; // Échec du hachage.
        }

        // Ajouter le nouvel utilisateur à la map en mémoire.
        this->users[userIdPlainText] = password_to_store; // Stocke l'ID et le HASH + SEL.
        LOG("Server::processAuthRequest DEBUG : Nouvel utilisateur '" + userIdPlainText + "' ajouté à map en mémoire.", "DEBUG");

        // Sauvegarder la liste des utilisateurs sur disque immédiatement après un ajout.
        // La méthode SaveUsersInternal prend le lock et le libère, mais comme processAuthRequest
        // a déjà un lock sur usersMutex, il faut appeler SaveUsersInternal qui ne reprend pas le lock.
        // Note : Appeler SaveUsersInternal *pendant* que usersMutex est verrouillé est le bon comportement.
        SaveUsersInternal(this->usersFile_path); // Sauvegarde les utilisateurs (interne, sous lock)
        LOG("Server::processAuthRequest INFO : Liste utilisateurs sauvegardée sur disque après ajout ID: '" + userIdPlainText + "'.", "INFO");

        // Créer le fichier portefeuille pour le nouvel utilisateur.
        // CreateWalletFile gère ses propres logs et erreurs.
        if (!CreateWalletFile(userIdPlainText)) {
             LOG("Server::processAuthRequest ERROR : Impossible créer fichier portefeuille pour nouvel ID client: " + userIdPlainText + ". Annulation enregistrement.", "ERROR");
             // Si la création du portefeuille échoue, on devrait annuler l'enregistrement de l'utilisateur.
             this->users.erase(userIdPlainText); // Retire l'utilisateur de la map en mémoire.
             SaveUsersInternal(this->usersFile_path); // Resauvegarde la map après retrait.
             LOG("Server::processAuthRequest WARNING : Nouvel utilisateur '" + userIdPlainText + "' retiré de map suite échec création portefeuille.", "WARNING");
             authenticatedUserId.clear();
             return AuthOutcome::FAIL; // Échec de l'enregistrement.
        }
        LOG("Server::processAuthRequest INFO : Fichier portefeuille créé avec succès pour ID: '" + userIdPlainText + "'.", "INFO");

        LOG("Server::processAuthRequest INFO : Enregistrement nouvel utilisateur '" + userIdPlainText + "' réussi.", "INFO");
        authenticatedUserId = userIdPlainText; // Retourne l'ID via le paramètre de sortie.
        return AuthOutcome::NEW; // Enregistrement réussi.
    }
    // Le verrou sur usersMutex est automatiquement libéré ici.
}



// --- Implémentation de la méthode Server::InitServerCTX ---
UniqueSSLCTX Server::InitServerCTX(const std::string& certFile, const std::string& keyFile) {
    LOG("Server::InitServerCTX DEBUG : Initialisation du contexte SSL serveur...", "DEBUG");
    UniqueSSLCTX context(SSL_CTX_new(TLS_server_method()));
    if (!context) {
        LOG("Server::InitServerCTX ERROR : Impossible de créer le contexte SSL serveur.", "ERROR");
        ERR_print_errors_fp(stderr);
        return nullptr;
    }
    LOG("Server::InitServerCTX DEBUG : Contexte SSL serveur créé.", "DEBUG");

    SSL_CTX_set_info_callback(context.get(), openssl_debug_callback);
    SSL_CTX_set_min_proto_version(context.get(), TLS1_2_VERSION);
    SSL_CTX_set_options(context.get(), SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_SINGLE_DH_USE);

    if (SSL_CTX_use_certificate_file(context.get(), certFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
        LOG("Server::InitServerCTX ERROR : Échec chargement certificat serveur : " + certFile, "ERROR");
        ERR_print_errors_fp(stderr);
        return nullptr;
    }
    LOG("Server::InitServerCTX DEBUG : Certificat serveur chargé : " + certFile, "DEBUG");

    if (SSL_CTX_use_PrivateKey_file(context.get(), keyFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
        LOG("Server::InitServerCTX ERROR : Échec chargement clé privée serveur : " + keyFile, "ERROR");
        ERR_print_errors_fp(stderr);
        return nullptr;
    }
    LOG("Server::InitServerCTX DEBUG : Clé privée serveur chargée : " + keyFile, "DEBUG");

     if (!SSL_CTX_check_private_key(context.get())) {
         LOG("Server::InitServerCTX ERROR : La clé privée ne correspond pas au certificat public !", "ERROR");
         ERR_print_errors_fp(stderr);
         return nullptr;
     }

     LOG("Server::InitServerCTX INFO : Contexte SSL serveur initialisé avec succès.", "INFO");
    return context;
}

// --- Implémentation de la méthode Server::AcceptSSLConnection ---
UniqueSSL Server::AcceptSSLConnection(SSL_CTX* ctx_raw, int clientSocket) {
    LOG("Server::AcceptSSLConnection DEBUG : Tentative handshake SSL pour socket FD: " + std::to_string(clientSocket), "DEBUG");
    if (!ctx_raw) {
        LOG("Server::AcceptSSLConnection ERROR : Contexte SSL raw est null. Fermeture socket FD: " + std::to_string(clientSocket), "ERROR");
        close(clientSocket);
        return nullptr;
    }

    UniqueSSL ssl_ptr(SSL_new(ctx_raw));
    if (!ssl_ptr) {
        LOG("Server::AcceptSSLConnection ERROR : Erreur SSL_new(). Socket FD: " + std::to_string(clientSocket), "ERROR");
        ERR_print_errors_fp(stderr);
        close(clientSocket);
        return nullptr;
    }
    LOG("Server::AcceptSSLConnection DEBUG : Objet SSL créé pour socket FD: " + std::to_string(clientSocket), "DEBUG");

    if (SSL_set_fd(ssl_ptr.get(), clientSocket) <= 0) {
        LOG("Server::AcceptSSLConnection ERROR : Erreur SSL_set_fd(). Socket FD: " + std::to_string(clientSocket), "ERROR");
        ERR_print_errors_fp(stderr);
        close(clientSocket);
        return nullptr;
    }

    int ssl_accept_ret = SSL_accept(ssl_ptr.get());

    if (ssl_accept_ret <= 0) {
        int ssl_err = SSL_get_error(ssl_ptr.get(), ssl_accept_ret);
        if (ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE) {
             LOG("Server::AcceptSSLConnection WARNING : SSL_accept() retourné WANT_READ/WRITE pour socket FD: " + std::to_string(clientSocket) + ". Erreur SSL: " + std::to_string(ssl_err) + ". Connexion fermée car handshake non immédiat en mode bloquant.", "WARNING");
             close(clientSocket);
             return nullptr;
        } else {
            LOG("Server::AcceptSSLConnection ERROR : Erreur fatale lors du handshake SSL pour socket FD: " + std::to_string(clientSocket) + ". Erreur SSL: " + std::to_string(ssl_err), "ERROR");
            ERR_print_errors_fp(stderr);
            close(clientSocket);
            return nullptr;
        }
    }

    LOG("Server::AcceptSSLConnection INFO : Handshake SSL réussi pour socket FD: " + std::to_string(clientSocket), "INFO");
    return ssl_ptr;
}


// --- Implémentation de la méthode Server::HandleClient ---
void Server::HandleClient(int clientSocket, SSL* raw_ssl_ptr) {
    // Ce thread gère toute la durée de vie d'une connexion cliente, de l'acceptation à la déconnexion.
    // Utilisez les variables 'clientSocket' et 'raw_ssl_ptr' qui sont passées et appartiennent à ce thread.
    LOG("Server::HandleClient INFO : Thread démarré pour socket FD: " + std::to_string(clientSocket), "INFO");

    // Déclarer client_conn et session au début du scope du try/catch principal
    // pour s'assurer que leur destruction (et donc le nettoyage de la connexion/session)
    // se fasse même si une exception survient APRES leur création.
    std::shared_ptr<ServerConnection> client_conn = nullptr;
    std::shared_ptr<ClientSession> session = nullptr; // Sera créé après l'authentification.
    std::string authenticated_clientId; // Variable pour stocker l'ID après auth réussie.

    try {
        // --- 1. Créer l'objet ServerConnection ---
        // Cet objet encapsule la socket et l'objet SSL. Il prend possession du socket et de l'objet SSL.
        // Le constructeur de ServerConnection devrait vérifier que raw_ssl_ptr est valide.
        LOG("Server::HandleClient DEBUG : Création de l'objet ServerConnection pour socket FD: " + std::to_string(clientSocket), "DEBUG");
        client_conn = std::make_shared<ServerConnection>(clientSocket, raw_ssl_ptr);
        // raw_ssl_ptr ne doit plus être utilisé directement après cette ligne si ServerConnection en prend possession.

        // Vérification basique si l'objet a été créé et est connecté.
        if (!client_conn || !client_conn->isConnected()) {
             LOG("Server::HandleClient ERROR : Objet ServerConnection invalide ou non connecté après création pour socket FD: " + std::to_string(clientSocket) + ". Arrêt du thread.", "ERROR");
             // Le destructeur de client_conn devrait fermer la socket/libérer SSL.
             return; // Quitte le thread de gestion client.
        }
        LOG("Server::HandleClient INFO : Objet ServerConnection créé et connecté pour socket FD: " + std::to_string(client_conn->getSocketFD()), "INFO");


        // --- 2. Authentification du client ---
        // ClientAuthenticator est une classe qui gère le protocole d'authentification sur la connexion.
        // Elle est locale à ce thread et temporaire.
        ClientAuthenticator authenticator;

        AuthOutcome authOutcome = AuthOutcome::FAIL;

        try {
            // Appelle la méthode d'authentification. Elle gère la réception du message auth,
            // le parsing, et DÉLÈGUE la logique de gestion utilisateur (vérif/enregistrement)
            // à Server::processAuthRequest. authenticated_clientId est un paramètre OUT.
            LOG("Server::HandleClient DEBUG : Lancement du processus AuthenticateClient...", "DEBUG");
            authOutcome = authenticator.AuthenticateClient(*client_conn, *this, authenticated_clientId);

            // --- Vérifier le résultat de l'authentification ---
            if (authOutcome == AuthOutcome::FAIL) {
                // Authentification a retourné FAIL. Authenticator a déjà envoyé message d'erreur au client et fermé la connexion.
                 LOG("Server::HandleClient INFO : Authentification échouée pour client sur Socket FD: " + std::to_string(client_conn->getSocketFD()) + ". Connexion fermée par Authenticator.", "INFO");
                 return; // Quitte le thread.
            }

            // Si succès (SUCCESS ou NEW), on s'assure que authenticated_clientId a bien été rempli par AuthenticateClient.
            if (authenticated_clientId.empty()) {
                 // Cela ne devrait pas arriver si AuthenticateClient fonctionne comme prévu, mais c'est une sécurité.
                 LOG("Server::HandleClient ERROR : Authentification réussie (" + authOutcomeToString(authOutcome) + ") mais authenticated_clientId est vide. Socket FD: " + std::to_string(client_conn->getSocketFD()) + ". Arrêt du thread.", "ERROR");
                 // Logique d'erreur grave interne : l'Authenticator a dit succès mais n'a pas fourni l'ID.
                 if(client_conn && client_conn->isConnected()) {
                      try { client_conn->send("AUTH FAIL: Server internal error after successful authentication.\n"); } catch(...) {}
                      client_conn->closeConnection();
                 }
                 return; // Quitte le thread.
            }

            LOG("Server::HandleClient INFO : Authentification/enregistrement réussi pour client ID: '" + authenticated_clientId + "' (" + authOutcomeToString(authOutcome) + "). Socket FD: " + std::to_string(client_conn->getSocketFD()), "INFO");


        } catch (const std::exception& e) {
             // Log toute exception survenue *pendant* l'appel à AuthenticateClient (parsing message, etc.).
             LOG("Server::HandleClient ERROR : Exception lors du processus d'authentification pour socket FD " + std::to_string(client_conn->getSocketFD()) + ". Exception: " + e.what() + ". Arrêt du thread.", "ERROR");
             // Authenticator aurait dû envoyer un message d'erreur et fermer la connexion en cas d'exception interne.
             // On ferme ici par sécurité si ce n'est pas le cas ou si l'exception vient d'ailleurs dans AuthenticateClient.
             if(client_conn && client_conn->isConnected()) {
                 try { client_conn->send("AUTH FAIL: Internal server error during authentication processing.\n"); } catch(...) {}
                 client_conn->closeConnection();
             }
             return; // Quitte le thread.
        }

        // --- 3. Authentification réussie (SUCCESS ou NEW) ---

        std::shared_ptr<ClientSession> session = nullptr; // Déclarer ici pour la portée


        // ========================================================================
        // Vérification d'une session active et création de la nouvelle session (SOUS LOCK)
        // ========================================================================
        { // Début du bloc pour le lock_guard protégeant activeSessions. Le verrou dure le temps de la vérification ET de l'ajout.
             std::lock_guard<std::mutex> lock(this->sessionsMutex);

             // Vérifie si une session pour cet authenticated_clientId est déjà active.
             if (this->activeSessions.count(authenticated_clientId)) {
                  // Une session existe déjà. Refuser la nouvelle connexion.
                  LOG("Server::HandleClient WARNING : Connexion refusée. Une session pour client ID: '" + authenticated_clientId + "' est déjà active. Socket FD: " + std::to_string(client_conn->getSocketFD()), "WARNING");
                  // Envoyer un message d'erreur spécifique au client.
                  if(client_conn && client_conn->isConnected()) {
                       try { client_conn->send("AUTH FAIL: Already connected with this ID.\n"); } catch(...) {} // ENVOYER LE FAIL ICI !
                       client_conn->closeConnection(); // Fermer la nouvelle connexion entrante.
                  }
                  return; // Quitte le thread de gestion client (HandleClient).
             }
             // Si on arrive ici : Authentification réussie ET PAS DE SESSION ACTIVE EXISTANTE.

             // La nouvelle connexion est autorisée. Créer la ClientSession.
             LOG("Server::HandleClient DEBUG : Lancement de la création de ClientSession pour client ID: " + authenticated_clientId + " (sous lock)...", "DEBUG");
             try {
                 // Créer l'objet ClientSession. Le constructeur de ClientSession crée l'objet Wallet.
                 session = std::make_shared<ClientSession>(client_conn, authenticated_clientId, shared_from_this(), this->wallets_dir_path);
                 // Vérifier si la création a échoué (ptr null).
                 if (!session) {
                      // Cas critique : make_shared a retourné nullptr.
                      LOG("Server::HandleClient CRITICAL ERROR : std::make_shared<ClientSession>() a retourné nullptr de manière inattendue pour client ID: " + authenticated_clientId + ". Socket FD: " + std::to_string(client_conn->getSocketFD()) + " (sous lock). Arrêt.", "CRITICAL");
                      // La connexion sera fermée après le bloc lock si session est null.
                      // Pas besoin d'envoyer un message ici, on le fera juste après le bloc lock.
                 } else {
                      // Objet ClientSession créé avec succès. L'ajouter à la map activeSessions pendant que le verrou est actif.
                      this->activeSessions[authenticated_clientId] = session;
                      LOG("Server::HandleClient INFO : Session enregistrée pour client ID: " + authenticated_clientId + ". Total sessions actives: " + std::to_string(this->activeSessions.size()) + " (sous lock).", "INFO");
                 }

             } catch (const std::exception& e) {
                  // Attrape exceptions lors de la création/initialisation de ClientSession/Wallet.
                  LOG("Server::HandleClient ERROR : Exception lors de la création/initialisation de ClientSession/Wallet pour client ID " + authenticated_clientId + ". Socket FD: " + std::to_string(client_conn->getSocketFD()) + ". Exception: " + e.what() + " (sous lock). Arrêt.", "ERROR");
                  // Pas besoin d'envoyer un message ici, on le fera juste après le bloc lock.
                  // La session sera nulle, le nettoyage se fera après le bloc lock.
             }
            // Le verrou (lock_guard) se termine ici, libérant le mutex activeSessions.
        } // Fin du bloc lock_guard


        // ============================================
        // Gérer les cas d'échec APRES le bloc du lock
        // ============================================
        // Si session est nullptr, c'est qu'il y a eu un échec lors de la création ou initialisation (capture par les catch{} ou vérification nullptr).
        if (!session) {
             // L'échec a déjà été loggué dans le bloc lock.
             // Nettoyage : fermer la connexion car la session n'a pas pu être initialisée ou enregistrée.
             // On n'a pas encore envoyé AUTH SUCCESS/NEW, donc on envoie un message d'erreur serveur.
             if(client_conn && client_conn->isConnected()) {
                  try { client_conn->send("ERROR: Server internal error initializing your session. Connection closing.\n"); } catch(...) {}
                  client_conn->closeConnection(); // Fermer la connexion.
             }
             return; // Quitte le thread.
        }
        // Si on arrive ici, la session a été créée et ajoutée à activeSessions avec succès.


        // ===================================================================
        // Envoyer la réponse d'authentification réussie au client (SANS LOCK)
        // ===================================================================
        // Maintenant que la connexion est validée et que la session a été créée et enregistrée.
        if (authOutcome == AuthOutcome::SUCCESS) {
             LOG("Server::HandleClient INFO : Authentification réussie pour client ID: '" + authenticated_clientId + "'. Envoi réponse AUTH SUCCESS.", "INFO");
             if(client_conn && client_conn->isConnected()) {
                  try { client_conn->send("AUTH SUCCESS\n"); } catch(...) {}
             }
        } else if (authOutcome == AuthOutcome::NEW) {
             LOG("Server::HandleClient INFO : Enregistrement et authentification réussis pour client ID: '" + authenticated_clientId + "'. Envoi réponse AUTH NEW.", "INFO");
              if(client_conn && client_conn->isConnected()) {
                  try { client_conn->send("AUTH NEW\n"); } catch(...) {}
             }
        }
        // Le client va lire cette réponse AUTH SUCCESS/NEW avec receiveLine() et passer à la boucle de commandes.


        // ========================================================================
        // Démarrer le thread de la ClientSession et finaliser la gestion (SANS LOCK)
        // ========================================================================
        // La session est déjà créée et enregistrée. Reste à la démarrer (ce qui lance son thread run()).

        LOG("Server::HandleClient DEBUG : Démarrage du thread ClientSession pour client ID: " + authenticated_clientId + "...", "DEBUG");
        try {
             // Le démarrage lance le thread run().
             session->start(); // TODO: Implement ClientSession::start() if not already done.
             // La méthode start() doit créer le thread 'sessionThread' et le lancer, puis potentiellement se détacher ou le rendre joignable.
             // Si start() lance une exception, elle sera attrapée ici.
        } catch (const std::exception& e) {
             LOG("Server::HandleClient ERROR : Exception lors du démarrage du thread ClientSession pour client ID " + authenticated_clientId + ". Socket FD: " + std::to_string(client_conn->getSocketFD()) + ". Exception: " + e.what() + ". Tentative de nettoyage.", "ERROR");
             // Si le démarrage du thread échoue, il faut nettoyer la session (retirer de activeSessions, fermer connexion).
             // Le destructeur de ServerConnection (via ClientSession) s'occupe de fermer la connexion.
             // Le retrait de activeSessions doit se faire proprement.
             { // Nouveau bloc pour le lock_guard pour retirer de activeSessions.
                  std::lock_guard<std::mutex> lock(this->sessionsMutex);
                  // Vérifier si la session existe toujours dans la map avant de tenter de la retirer.
                  if (this->activeSessions.count(authenticated_clientId)) {
                      this->activeSessions.erase(authenticated_clientId);
                      LOG("Server::HandleClient INFO : Session pour client ID: " + authenticated_clientId + " retirée de activeSessions suite à un échec de démarrage. Total sessions actives: " + std::to_string(this->activeSessions.size()) + ".", "INFO");
                  } else {
                       LOG("Server::HandleClient WARNING : Session pour client ID: " + authenticated_clientId + " n'était plus dans activeSessions lors du nettoyage après échec de démarrage.", "WARNING");
                  }
             } // Fin lock_guard pour retrait.

             // Envoyer un message d'erreur au client puisque AUTH SUCCESS/NEW a déjà été envoyé.
             if(client_conn && client_conn->isConnected()) {
                 try { client_conn->send("ERROR: Server internal error starting your session. Connection closing.\n"); } catch(...) {}
                 // La connexion sera fermée par le destructeur de ServerConnection qui sera appelé
                 // quand le shared_ptr 'session' et 'client_conn' dans la map activeSessions seront détruits.
                 // Ou si elle n'a pas été ajoutée à la map, quand le shared_ptr local 'session' et 'client_conn' seront détruits.
             }
             return; // Quitte le thread.
        }

        // Si on arrive ici, le thread ClientSession a été démarré avec succès.
        LOG("Server::HandleClient INFO : Thread ClientSession démarré pour client ID: " + authenticated_clientId + ". Le thread HandleClient va se terminer normalement.", "INFO");

        // --- Le thread HandleClient a terminé son travail ---
        // Il n'a plus besoin de pointer vers client_conn ou session.
        // Les shared_ptr détenus par le Server (via activeSessions) et la ClientSession elle-même
        // maintiennent les objets en vie tant que la session est active.
        // Les variables locales client_conn et session dans HandleClient sortiront de portée ici.



        // --- 4. Enregistrer et démarrer la session ---

        // Ajouter la session à la liste active des sessions gérées par le serveur. Thread-safe.
        // Cela assure que l'objet ClientSession (et donc ServerConnection et Wallet) reste en vie.
        LOG("Server::HandleClient DEBUG : Ajout de la session à activeSessions...", "DEBUG");
        {
            std::lock_guard<std::mutex> lock(this->sessionsMutex);
            this->activeSessions[authenticated_clientId] = session; // activeSessions garde un shared_ptr
        }
        LOG("Server::HandleClient INFO : Session enregistrée pour client ID: " + authenticated_clientId + ". Total sessions actives: " + std::to_string(this->activeSessions.size()) + ".", "INFO");


        // Enregistrer la session auprès de la TransactionQueue globale. Thread-safe.
        // La TQ a besoin d'un pointeur vers la session pour lui envoyer des notifications (ex: transaction terminée).
        LOG("Server::HandleClient DEBUG : Enregistrement de la session auprès de la TransactionQueue...", "DEBUG");
        try {
            txQueue.registerSession(session); // txQueue est une variable globale (extern). registerSession attend un shared_ptr<ClientSession>.
            LOG("Server::HandleClient INFO : ClientSession pour client ID: " + authenticated_clientId + " enregistrée auprès de la TransactionQueue.", "INFO");
        } catch (const std::exception& e) {
            // Si l'enregistrement TQ échoue, on retire la session de activeSessions et on ferme la connexion.
            LOG("Server::HandleClient ERROR : Échec enregistrement ClientSession auprès de TQ pour client ID " + authenticated_clientId + ". Exception: " + e.what() + ". Arrêt du thread.", "ERROR");
            { // Bloc pour le lock_guard
                std::lock_guard<std::mutex> lock(this->sessionsMutex);
                this->activeSessions.erase(authenticated_clientId); // Retire la session pour éviter une référence pendante.
            }
             // Envoyer un message d'erreur au client et fermer la connexion.
             if(client_conn && client_conn->isConnected()) {
                 try { client_conn->send("ERROR: Server internal error initializing transaction processing for your session. Connection closing.\n"); } catch(...) {}
                 client_conn->closeConnection();
             }
            return; // Quitte le thread.
        }


        // Lancer le thread principal de la ClientSession.
        // C'est session->start() qui met le flag 'running' à true et démarre le thread de la session
        // qui va ensuite gérer la communication (receive messages, process commands).
        LOG("Server::HandleClient DEBUG : Démarrage du thread ClientSession...", "DEBUG");
        session->start(); // Cette méthode ne devrait pas bloquer le thread HandleClient, elle démarre un nouveau thread interne.
        LOG("Server::HandleClient INFO : Thread ClientSession démarré pour client ID: " + authenticated_clientId + ". Le thread HandleClient va se terminer normalement.", "INFO");

        // --- 5. Fin du thread HandleClient ---
        // Le thread HandleClient se termine ici.
        // Il a créé et configuré la ClientSession et démarré son thread interne.
        // Le travail de communication avec le client et de traitement des commandes
        // est maintenant entièrement pris en charge par le thread démarré dans session->start().

        // Les objets ClientSession, ServerConnection et Wallet sont maintenus en vie par les shared_ptr
        // dans activeSessions et la TransactionQueue tant qu'ils ne sont pas explicitement retirés.
        // Le thread Server::HandleClient est terminé et ses copies locales de shared_ptr sont détruites,
        // mais cela ne détruit pas les objets tant que d'autres shared_ptr existent.

        return; // Fin normale du thread HandleClient.

    } catch (const std::exception& e) {
        // --- Gestion des exceptions CRITIQUES ---
        // Ce try/catch principal attrape TOUTES les exceptions non gérées par les try/catch internes.
        // Cela inclut des erreurs lors de la création initiale de ServerConnection (si son ctor lance),
        // ou des exceptions lancées APRÈS la création de la session si elles ne sont pas gérées par la ClientSession elle-même.
        LOG("Server::HandleClient CRITICAL ERROR : Exception CRITIQUE non gérée dans thread HandleClient pour socket FD: " + std::to_string(clientSocket) + ". Client ID (si connu): " + (authenticated_clientId.empty() ? "Inconnu" : authenticated_clientId) + ". Exception: " + e.what(), "CRITICAL");

        // Tenter un nettoyage d'urgence en cas de crash.
        // 1. Fermer la connexion avec le client si elle est encore ouverte.
        if(client_conn && client_conn->isConnected()) {
             LOG("Server::HandleClient CRITICAL ERROR : Tentative de fermeture de client_conn (" + std::to_string(client_conn->getSocketFD()) + ") suite à exception.", "CRITICAL");
             try { client_conn->send("CRITICAL SERVER ERROR: Unhandled exception in your session thread. Connection closing.\n"); } catch(...) {}
             client_conn->closeConnection(); // Le destructeur ServerConnection s'assurera aussi du cleanup SSL/socket.
        } else {
            // Si client_conn n'a pas pu être créé ou connecté, il faut nettoyer raw_ssl_ptr et clientSocket manuellement.
             LOG("Server::HandleClient CRITICAL ERROR : client_conn n'était pas valide/connecté. Tentative de nettoyage manuel socket/SSL (FD: " + std::to_string(clientSocket) + ") suite à exception.", "CRITICAL");
            // Ssl_free gère les cas où le socket FD est déjà fermé. close gère les cas où FD est -1.
            if (raw_ssl_ptr) { SSL_free(raw_ssl_ptr); raw_ssl_ptr = nullptr; LOG("Server::HandleClient CRITICAL ERROR : raw_ssl_ptr libéré manuellement.", "CRITICAL"); }
            if (clientSocket != -1) { close(clientSocket); clientSocket = -1; LOG("Server::HandleClient CRITICAL ERROR : clientSocket " + std::to_string(clientSocket) + " fermé manuellement.", "CRITICAL"); }
        }

        // 2. Si la session a été créée avant l'exception, tenter de la retirer des listes globales.
        // Attention, si l'exception a corrompu l'objet session, cela peut être risqué.
        // Un mécanisme de surveillance/nettoyage des sessions zombies serait plus robuste.
        // Pour l'instant, on retire la session de activeSessions si elle y a été ajoutée.
        if (session && !authenticated_clientId.empty()) {
             LOG("Server::HandleClient CRITICAL ERROR : Tentative de retrait de la session '" + authenticated_clientId + "' de activeSessions.", "CRITICAL");
             std::lock_guard<std::mutex> lock(this->sessionsMutex);
             if (this->activeSessions.count(authenticated_clientId)) {
                  this->activeSessions.erase(authenticated_clientId);
                  LOG("Server::HandleClient CRITICAL ERROR : Session '" + authenticated_clientId + "' retirée de activeSessions.", "CRITICAL");
             }
             // Si la session a été enregistrée auprès de la TQ (après activeSessions),
             // il faudrait aussi la désenregistrer (si une méthode unregisterSession existe dans TQ).
             // txQueue.unregisterSession(session); // <-- Si cette méthode existe dans TransactionQueue
        }


        // Le thread HandleClient se termine suite à l'exception.
    }
    // Fin de la méthode Server::HandleClient. Le thread se termine ici.
    LOG("Server::HandleClient DEBUG : Fin du thread Server::HandleClient pour socket FD: " + std::to_string(clientSocket) + ".", "DEBUG");
}


// --- Implémentation de la méthode Server::AcceptLoop ---
void Server::AcceptLoop() {
    LOG("Server::AcceptLoop INFO : Thread démarré. En attente de connexions...", "INFO");

    while (this->acceptingConnections.load(std::memory_order_acquire)) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);

        int clientSocket = accept(this->serverSocket, (struct sockaddr*)&clientAddr, &clientLen);

        if (clientSocket < 0) {
            if (!this->acceptingConnections.load(std::memory_order_acquire)) {
                 LOG("Server::AcceptLoop INFO : Signal d'arrêt détecté via acceptingConnections après accept() (socket < 0). Sortie.", "INFO");
                 break;
            }
            if (errno == EINTR) {
                 LOG("Server::AcceptLoop WARNING : Appel accept() interrompu par signal (EINTR). Continuation...", "WARNING");
                 continue;
            }
            if (errno == EBADF || errno == EINVAL) {
                 LOG("Server::AcceptLoop INFO : Socket serveur invalide/fermé (" + std::to_string(errno) + ") pendant accept(). Probablement arrêt propre. Sortie.", "INFO");
                 break;
            }
            LOG("Server::AcceptLoop ERROR : Échec inattendu accept(). Erreur: " + std::string(strerror(errno)), "ERROR");
            continue;
        }

        if (!this->acceptingConnections.load(std::memory_order_acquire)) {
             LOG("Server::AcceptLoop INFO : Signal d'arrêt détecté juste après accept() réussi. Fermeture nouveau socket FD: " + std::to_string(clientSocket), "INFO");
             close(clientSocket);
             break;
        }

        std::stringstream log_accept_ss;
        log_accept_ss << "Server::AcceptLoop INFO : Nouvelle connexion acceptée. Socket FD: " << clientSocket << ", IP: " << inet_ntoa(clientAddr.sin_addr); // inet_ntoa non thread-safe, mais ici c'est juste pour un log rapide dans le thread d'accept. Pour un usage critique, utiliser inet_ntop.
        LOG(log_accept_ss.str(), "INFO");

        UniqueSSL client_ssl = AcceptSSLConnection(this->ctx.get(), clientSocket);

        if (!client_ssl) {
            LOG("Server::AcceptLoop ERROR : Échec handshake SSL pour connexion. Socket FD initial: " + std::to_string(clientSocket) + ". Connexion fermée par AcceptSSLConnection.", "ERROR");
            continue;
        }
        LOG("Server::AcceptLoop INFO : Handshake SSL réussi pour socket FD: " + std::to_string(clientSocket), "INFO");

        try {
             // Lance un nouveau thread HandleClient pour cette connexion.
             // Passe la référence au ServerConnection par pointeur brut (ou shared_ptr si HandleClient l'attendait),
             // et le pointeur RAW SSL (dont HandleClient/ServerConnection prend la propriété via release()).
             // On passe clientSocket et client_ssl.release() qui seront pris en charge par ServerConnection dans HandleClient.
             std::thread clientThread(&Server::HandleClient, this, clientSocket, client_ssl.release());
             clientThread.detach();
             LOG("Server::AcceptLoop INFO : Thread gestion client détaché pour socket FD: " + std::to_string(clientSocket), "INFO");
        } catch (const std::system_error& e) {
             LOG("Server::AcceptLoop CRITICAL ERROR : Échec création thread gestion client pour socket FD: " + std::to_string(clientSocket) + ". Erreur: " + e.what() + " - " + e.code().message(), "CRITICAL");
             // Si le thread ne peut pas être créé, il faut fermer la connexion manuellement.
             // client_ssl.release() n'a pas été appelé.
             if (client_ssl) { // Si l'objet SSL a été créé
                 SSL_free(client_ssl.release());
             }
             close(clientSocket);
             LOG("Server::AcceptLoop CRITICAL ERROR : Connexion fermée manuellement suite échec création thread.", "CRITICAL");
             continue;
        }
    } // Fin de la boucle while(this->acceptingConnections.load())

    LOG("Server::AcceptLoop INFO : Thread Server::AcceptLoop terminé.", "INFO");
}