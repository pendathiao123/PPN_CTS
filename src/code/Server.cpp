#include "../headers/Server.h"
#include "../headers/Bot.h" 
#include "../headers/Client.h" 
#include "../headers/Transaction.h" 
#include "../headers/TransactionQueue.h" 
#include "../headers/Global.h"          
#include "../headers/Logger.h"          
#include "../headers/OpenSSLDeleters.h"

#include <openssl/hmac.h>  
#include <openssl/evp.h>    
#include <openssl/rand.h>   
#include <openssl/ssl.h>   
#include <openssl/err.h>    

#include <iomanip>      
#include <iostream>     
#include <random>       
#include <sstream>      
#include <cstring>      
#include <arpa/inet.h>  
#include <unistd.h>     
#include <fstream>      
#include <unordered_map> 
#include <ctime>        
#include <chrono>       
#include <thread>       
#include <filesystem>   
#include <memory>       
#include <system_error> 
#include <functional>   
#include <vector>       
#include <cerrno>


// Déclaration de la file de transactions globale
extern TransactionQueue txQueue;

// --- Fonction utilitaire pour convertir l'énumération AuthOutcome en chaîne de caractères ---
// Utile pour les messages de log
std::string authOutcomeToString(AuthOutcome outcome) {
    switch (outcome) {
        case AuthOutcome::FAIL: return "FAIL";    // L'authentification a échoué.
        case AuthOutcome::SUCCESS: return "SUCCESS"; // L'utilisateur a été authentifié avec succès (utilisateur existant).
        case AuthOutcome::NEW: return "NEW";      // Un nouvel utilisateur a été créé et authentifié.
        default: return "UNKNOWN_OUTCOME"; // Cas inattendu
    }
}

// --- Implémentation de la callback de debug OpenSSL ---
// Peut rester telle quelle, s'assurer qu'elle utilise le Logger correctement.
void openssl_debug_callback(const SSL* ssl, int where, int ret)
{
    // ... (implementation) ...
     const char* str;
    int err = SSL_get_error(ssl, ret); // Obtenir l'erreur SSL

    if (where & SSL_CB_LOOP) str = "LOOP";
    else if (where & SSL_CB_EXIT) str = "EXIT";
    else if (where & SSL_CB_READ) str = "READ";
    else if (where & SSL_CB_WRITE) str = "WRITE";
    else if (where & SSL_CB_ALERT) str = "ALERT";
    else str = "OTHER";

    // Utiliser le logger (avec string literals pour le niveau)
    LOG("[OpenSSL DEBUG] [" + std::string(str) + "] " + std::string(SSL_state_string_long(ssl)), "DEBUG");

    if (where & SSL_CB_ALERT) {
        const char* direction = (where & SSL_CB_READ) ? "read" : "write";
        LOG("  >> Alert (" + std::string(direction) + "): " +
            std::string(SSL_alert_type_string_long(ret)) + " - " +
            std::string(SSL_alert_desc_string_long(ret)), "WARNING");
    }
     // Logguer les erreurs importantes
    if (ret <= 0 && err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE && err != SSL_ERROR_ZERO_RETURN) {
        std::string err_str = ERR_error_string(ERR_get_error(), nullptr);
        LOG("[OpenSSL DEBUG] SSL Error: " + err_str, "ERROR");
    }
}


// --- Implémentations des fonctions utilitaires (GenerateRandomString, GenerateRandomId, GenerateToken) ---

// Génère une chaîne de caractères aléatoire de longueur spécifiée
std::string GenerateRandomString(size_t length) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string result;
    result.reserve(length); // Préallouer la mémoire

    // Utilise std::vector<unsigned char> au lieu d'un VLA (Correction VLA)
    std::vector<unsigned char> rand_bytes(length);

    // Génère des octets aléatoires en utilisant la fonction RAND_bytes d'OpenSSL
    // RAND_bytes est thread-safe (selon la doc OpenSSL moderne si RNG est initialisé globalement, ce qui est fait dans main).
    if (RAND_bytes(rand_bytes.data(), length) != 1) { // rand_bytes.data() donne un pointeur vers les données du vecteur
        LOG("Erreur de génération aléatoire RAND_bytes", "ERROR"); // Utilisation correcte de LOG
        // Gérer l'erreur, peut-être lancer une exception ou retourner une chaîne vide
        return "";
    }

    // Construit la chaîne résultat en choisissant des caractères du charset basé sur les octets aléatoires
    for (size_t i = 0; i < length; ++i) {
        result += charset[rand_bytes[i] % (sizeof(charset) - 1)];
    }
    return result;
}

// Génère un ID client/bot aléatoire (simple)
std::string GenerateRandomId() {
    std::random_device rd; // Source d'entropie (thread-safe depuis C++11)
    std::mt19937 gen(rd()); // Moteur de nombres aléatoires (peut nécessiter un mutex si gen est partagé, mais ici il est local au thread)
    std::uniform_int_distribution<> dis(10000, 99999); // Distribution pour générer un nombre dans une plage

    return std::to_string(dis(gen)); // Génère un nombre aléatoire et le convertit en string
}

// Génère un token aléatoire (utilise HMAC-SHA256)
std::string GenerateToken() {
    std::string key = GenerateRandomString(32); // Clé aléatoire
    std::string message = GenerateRandomString(16); // Message aléatoire à hasher

    // Vérifier que la génération aléatoire a réussi
    if (key.empty() || message.empty()) {
         LOG("Impossible de générer le token car clé ou message aléatoire est vide.", "ERROR");
         return ""; // Retourner une chaîne vide en cas d'échec
    }

    unsigned char hash[EVP_MAX_MD_SIZE]; // Buffer pour stocker le hash
    unsigned int hash_len; // Longueur du hash

    // Calculer le HMAC-SHA256
    // HMAC est thread-safe si les contextes sont créés localement au thread (ici oui)
    if (HMAC(EVP_sha256(),         // Fonction de hash (SHA256)
             key.c_str(), key.size(), // Clé HMAC
             reinterpret_cast<const unsigned char*>(message.c_str()), message.size(), // Données à hasher
             hash, &hash_len) == nullptr) // Buffer de sortie et sa longueur
    {
        LOG("Erreur lors du calcul HMAC.", "ERROR");
        return ""; // Retourner une chaîne vide en cas d'erreur
    }

    // Convertir le hash binaire en chaîne hexadécimale
    std::ostringstream oss;
    oss << std::hex << std::setfill('0'); // Formater en hexadécimal avec remplissage par '0'
    for (unsigned int i = 0; i < hash_len; ++i) {
        oss << std::setw(2) << (int)hash[i]; // Écrire chaque octet en 2 caractères hex
    }

    return oss.str(); // Retourner la chaîne hexadécimale
}


// --- Implémentations LoadUsers et SaveUsers ---

// Charge les utilisateurs et leurs tokens depuis un fichier (simple format ID Token par ligne)
void Server::LoadUsers(const std::string& filename) {
    std::lock_guard<std::mutex> lock(usersMutex); // Protège l'accès à 'users' pendant le chargement

    std::ifstream file(filename); // Ouvre le fichier passé en paramètre pour lecture
    if (file.is_open()) {
        users.clear(); // Nettoyer la map MEMBRE 'this->users' avant de charger
        std::string line; // Lire ligne par ligne pour un parsing plus robuste
        while (std::getline(file, line)) { // Lit une ligne complète
            if (line.empty() || line[0] == '#') continue; // Sauter les lignes vides ou commentées

            std::istringstream stream(line);
            std::string id;
            std::string token;

            // Lit l'ID jusqu'au premier espace
            if (std::getline(stream, id, ' ')) {
                // Lit le reste de la ligne pour le token
                std::getline(stream, token);

                // Supprimer les espaces blancs autour de l'ID et du token
                id.erase(0, id.find_first_not_of(" \t\n\r\f\v"));
                id.erase(id.find_last_not_of(" \t\n\r\f\v") + 1);
                token.erase(0, token.find_first_not_of(" \t\n\r\f\v"));
                token.erase(token.find_last_not_of(" \t\n\r\f\v") + 1);

                if (!id.empty()) {
                     users[id] = token; // Popule la map MEMBRE 'this->users'
                } else {
                     LOG("Ligne invalide trouvée dans le fichier utilisateurs (ID vide): '" + line + "'.", "WARNING");
                }
            } else {
                 LOG("Ligne invalide trouvée dans le fichier utilisateurs (format incorrect): '" + line + "'.", "WARNING");
            }
        }
        file.close();
        LOG("Utilisateurs chargés depuis " + filename + " (" + std::to_string(users.size()) + " entrées).", "INFO");
    } else {
        LOG("Fichier utilisateurs non trouvé : " + filename + ". Démarrage avec une liste vide.", "WARNING");
        users.clear(); // Assurer que la map MEMBRE est vide même si le fichier n'est pas trouvé
    }
    // Le verrou sur usersMutex est relâché automatiquement ici.
}

// Sauvegarde les utilisateurs et leurs tokens dans le fichier usersFile_path (membre de Server)
// Cette méthode est appelée pour persister l'état de la map 'users' (membre de Server).
// Elle DOIT protéger l'accès à 'users' en utilisant 'usersMutex' (membre de Server).
// Elle n'a besoin que du nom du fichier (ou peut utiliser directement le membre usersFile_path).
void Server::SaveUsers(const std::string& filename) {
    std::lock_guard<std::mutex> lock(usersMutex); // Protection avec usersMutex
    // Le verrou est acquis au début de la fonction et relâché automatiquement à la fin du scope.

    // Ouvre le fichier en mode troncation (écrase le contenu existant)
    std::ofstream file(filename, std::ios::trunc); // Utilise le nom du fichier passé en argument
    if (!file.is_open()) {
        LOG("Impossible d'écrire dans le fichier utilisateurs: " + filename, "ERROR");
        // Log l'erreur et retourne. Le verrou sera relâché automatiquement.
        return;
    }

    // Écrit chaque paire ID-Token depuis la map membre 'users'.
    // L'accès à 'users' est sûr car usersMutex est locké.
    for (const auto& pair : users) { // Utilise la map membre 'users' 
        // Format: ID Espace Token NouvelleLigne (Ton format existant)
        file << pair.first << " " << pair.second << "\n";
    }

    file.close(); // Ferme le fichier. Le verrou est toujours actif ici.

    LOG("Sauvegarde de " + std::to_string(users.size()) + " utilisateurs dans " + filename, "INFO");
    // Le verrou sur usersMutex est relâché automatiquement ici à la fin de la fonction.
}


// --- Implémentation InitServerCTX ---
// Initialise le contexte SSL serveur et charge certificat/clé privée
UniqueSSLCTX Server::InitServerCTX(const std::string& certFile, const std::string& keyFile) {
    // Crée un nouveau contexte SSL serveur en utilisant TLS_server_method (pour toutes versions TLS serveur)
    // Utilise le pointeur unique pour gérer la libération automatique du contexte.
    UniqueSSLCTX context(SSL_CTX_new(TLS_server_method()));
    if (!context) {
        LOG("Impossible de créer le contexte SSL serveur.", "ERROR");
        return nullptr; // Retourne un pointeur nul unique_ptr en cas d'échec
    }

    // Configure une callback de debug OpenSSL pour ce contexte (optionnel)
    SSL_CTX_set_info_callback(context.get(), openssl_debug_callback);
    // Configure la version minimale du protocole TLS (TLS 1.2 est recommandé)
    SSL_CTX_set_min_proto_version(context.get(), TLS1_2_VERSION);

    // Charge le certificat serveur
    if (SSL_CTX_use_certificate_file(context.get(), certFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
         // ERR_print_errors_fp(stderr);
        LOG("Échec du chargement du certificat serveur : " + certFile, "ERROR");
        return nullptr; // Retourne un pointeur nul unique_ptr en cas d'échec
    }

    // Charge la clé privée serveur
    if (SSL_CTX_use_PrivateKey_file(context.get(), keyFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
        LOG("Échec du chargement de la clé privée serveur : " + keyFile, "ERROR");
        return nullptr; // Retourne un pointeur nul unique_ptr en cas d'échec
    }

     // Vérifie la cohérence de la clé privée et du certificat
     if (!SSL_CTX_check_private_key(context.get())) {
         LOG("La clé privée ne correspond pas au certificat public !", "ERROR");
         return nullptr; // Retourne un pointeur nul unique_ptr si non cohérent
     }

     LOG("Contexte SSL serveur initialisé avec succès.", "INFO");
    return context; // Retourne le pointeur unique qui gère le contexte
}

// --- Implémentation AcceptSSLConnection ---
// Effectue le handshake SSL sur un socket client connecté.
UniqueSSL Server::AcceptSSLConnection(SSL_CTX* ctx_raw, int clientSocket) {
    // Crée un nouvel objet SSL pour cette connexion à partir du contexte.
    // Utilise le pointeur unique pour gérer la libération automatique de l'objet SSL.
    UniqueSSL ssl_ptr(SSL_new(ctx_raw));
    if (!ssl_ptr) {
        LOG("[SSL] Erreur SSL_new().", "ERROR");
        close(clientSocket); // Ferme le socket en cas d'échec de création SSL
        return nullptr; // Retourne un pointeur nul unique_ptr
    }

    // Associe l'objet SSL au socket client
    SSL_set_fd(ssl_ptr.get(), clientSocket);

    // Effectue le handshake SSL. Cette fonction peut bloquer si le socket est bloquant.
    // Elle peut aussi retourner 0 ou -1 en cas d'erreur ou si plus de données sont attendues (non-bloquant).
    int ssl_accept_ret = SSL_accept(ssl_ptr.get());
    if (ssl_accept_ret <= 0) {
        int ssl_err = SSL_get_error(ssl_ptr.get(), ssl_accept_ret); // Obtient le code d'erreur SSL détaillé
        LOG("[SSL] Échec de SSL_accept(). Code: " + std::to_string(ssl_err), "ERROR");
        // Pour un socket bloquant, ssl_accept_ret <= 0 indique généralement une erreur ou que la connexion a été fermée.
        // Le unique_ptr ssl_ptr libérera automatiquement l'objet SSL en sortant.
        close(clientSocket); // Ferme le socket en cas d'échec du handshake
        return nullptr; // Retourne un pointeur nul unique_ptr
    }

    LOG("[SSL] SSL_accept réussi. Client connecté SSL.", "INFO");
    return ssl_ptr; // Retourne le pointeur unique qui gère l'objet SSL
}


// --- Constructeur du Server ---
Server::Server(int p,
    const std::string& certF,
    const std::string& keyF,
    const std::string& usersF, // usersFile param
    const std::string& transactionCounterF, // transactionCounterFile param
    const std::string& transactionHistoryF, // transactionHistoryFile param
    const std::string& walletsDir) // walletsDir param 
// --- Liste d'initialisation : Initialise les membres avec les paramètres ---
: port(p), // Initialise le membre port
certFile_path(certF), // Initialise le membre certFile_path
keyFile_path(keyF), // Initialise le membre keyFile_path
usersFile_path(usersF), // Initialise le membre usersFile_path avec le paramètre usersF
transactionCounterFile_path(transactionCounterF), // Initialise le membre transactionCounterFile_path
transactionHistoryFile_path(transactionHistoryF), // Initialise le membre transactionHistoryFile_path
wallets_dir_path(walletsDir), // Initialise le membre wallets_dir_path avec le paramètre walletsDir
serverSocket(-1), // Initialise le membre serverSocket à -1 par défaut
ctx(nullptr) // Initialise le membre ctx à nullptr par défaut
// Les mutex (usersMutex, sessionsMutex) et les maps (users, activeSessions)
// sont automatiquement initialisés avec leurs constructeurs par défaut.
// priceGeneratorThread et stopPriceGeneration si membres, doivent aussi être initialisés si besoin.
{
LOG("Constructeur Server appelé. Chemins des fichiers stockés.", "DEBUG");

// Charger les utilisateurs au démarrage. Utilise le membre de la classe pour le chemin.
this->LoadUsers(usersFile_path); // Utilise le membre usersFile_path
}

// --- Destructeur du Server ---
// Appelé lorsque l'objet Server est détruit (typiquement à la fin de main).
Server::~Server() {
     LOG("Destructeur Server appelé. Début de l'arrêt...", "INFO");
    StopServer(); // Appeler la méthode d'arrêt propre pour fermer sockets, threads, sauvegarder.
     LOG("Destructeur Server terminé.", "DEBUG");
}

// --- Méthode principale pour démarrer le serveur ---
void Server::StartServer() {
    LOG("Démarrage du serveur...", "INFO");

    // Charger le contexte SSL du serveur
    ctx = InitServerCTX(certFile_path, keyFile_path); // Appel à la fonction InitServerCTX
    if (!ctx) {
        LOG("Échec de l'initialisation du contexte SSL serveur. Arrêt.", "ERROR");
        // En cas d'échec fatal ici, nous ne démarrons pas le serveur.
        // Il faut s'assurer que les nettoyages globaux sont faits dans main.
        return; // Quitte la méthode StartServer
    }

    // Charger les utilisateurs depuis le fichier
    this->LoadUsers(usersFile_path);

    // Démarrer le thread de génération des prix SRD-BTC
    // Appel à la méthode statique de Global pour qu'elle démarre son propre thread interne.
    Global::startPriceGenerationThread();


    // Configuration du socket serveur principal (écoute des connexions entrantes)
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0); // Crée un socket TCP/IPv4
    if (serverSocket == -1) {
        LOG("Échec de la création de la socket serveur. Erreur: " + std::string(strerror(errno)), "ERROR");
        // En cas d'échec fatal ici, nous devons nous arrêter proprement.
        StopServer(); // Appeler StopServer pour arrêter les composants déjà démarrés (Global, Queue)
        return; // Quitte la méthode StartServer
    }

    // Configure l'option SO_REUSEADDR pour pouvoir redémarrer rapidement après une fermeture
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG("setsockopt(SO_REUSEADDR) a échoué.", "WARNING");
        // Ce n'est qu'un avertissement, le serveur peut continuer.
    }

    // Configure l'adresse et le port d'écoute du serveur
    sockaddr_in serverAddr{}; // Structure pour l'adresse du serveur
    serverAddr.sin_family = AF_INET; // Famille d'adresses : IPv4
    serverAddr.sin_addr.s_addr = INADDR_ANY; // Écoute sur toutes les interfaces réseau disponibles (0.0.0.0)
    serverAddr.sin_port = htons(port);      // Port spécifié (conversion au format réseau)

    // Lie l'adresse et le port à la socket serveur
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        LOG("Échec de la liaison (bind) de la socket serveur. Port : " + std::to_string(port) + ". Erreur: " + std::string(strerror(errno)), "ERROR");
        close(serverSocket); // Ferme la socket créée
        // En cas d'échec fatal ici.
        StopServer(); // Appeler StopServer pour arrêter les composants déjà démarrés
        return; // Quitte la méthode StartServer
    }

    // Commence à écouter les connexions entrantes
    if (listen(serverSocket, 10) < 0) { // 10 est la taille maximale de la queue de connexions en attente
        LOG("Échec de l'écoute (listen) sur la socket serveur. Erreur: " + std::string(strerror(errno)), "ERROR");
        close(serverSocket); // Ferme la socket liée
        // En cas d'échec fatal ici.
        StopServer(); // Appeler StopServer
        return; // Quitte la méthode StartServer
    }

    LOG("Serveur démarré et en écoute sur le port " + std::to_string(port), "INFO");

    // Boucle principale d'acceptation des connexions clientes
    // Cette boucle tourne tant que le flag global server_running est true.
    // server_running est défini dans main_Serv.cpp et modifié par le signal handler.
    // Une façon simple est de rendre le flag server_running global et de le vérifier ici :
    extern std::atomic<bool> server_running; // Déclaration car il est défini dans main_Serv.cpp

    while (server_running.load()) { // La boucle tourne tant que le serveur doit être actif
        sockaddr_in clientAddr{}; // Structure pour l'adresse du client connecté
        socklen_t clientLen = sizeof(clientAddr); // Taille de la structure

        // Accepte une nouvelle connexion cliente. Ceci est une fonction bloquante.
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);

        // Si server_running devient false pendant accept() (signal reçu), accept() peut retourner -1 avec errno=EINTR.
        // On vérifie le flag après accept() pour sortir de la boucle si l'arrêt est demandé.
        if (!server_running.load()) {
             LOG("Signal d'arrêt détecté après accept(). Sortie de la boucle principale.", "INFO"); 
             break; // Sortir de la boucle si l'arrêt est demandé
        }


        if (clientSocket < 0) {
            // Gérer l'erreur d'acceptation. Si errno est EINTR (interrompu par un signal),
            // cela peut se produire si le signal handler ne gère pas la socket serveur.
            // On gère déjà le cas !server_running.load() au-dessus, mais si c'est une autre erreur :
            if (errno == EINTR) { // Interrompu par un signal (si server_running n'est pas encore false)
                 LOG("Appel accept() interrompu par un signal (EINTR). Continuation...", "WARNING"); 
                 continue; // Continue la boucle pour essayer à nouveau
            }
            // Autre erreur grave d'accept().
            LOG("Échec de l'acceptation d'une connexion cliente. Erreur: " + std::string(strerror(errno)), "ERROR");
            continue; // Pour l'exemple, on continue après une erreur non-EINTR
        }

        // Si accept() a réussi
        LOG("Nouvelle connexion acceptée. Socket FD: " + std::to_string(clientSocket) + ", IP: " + inet_ntoa(clientAddr.sin_addr), "INFO");


        // Tenter le handshake SSL pour la nouvelle connexion cliente
        // Accepte la connexion SSL sur le socket client. Peut bloquer.
        UniqueSSL client_ssl = AcceptSSLConnection(ctx.get(), clientSocket); // Appel à la fonction AcceptSSLConnection

        if (!client_ssl) {
            LOG("Échec du handshake SSL pour la nouvelle connexion. Socket FD: " + std::to_string(clientSocket) + ". La connexion est fermée.", "ERROR");
            // Le socket est déjà fermé par AcceptSSLConnection en cas d'échec.
            continue; // Passer à la prochaine connexion dans la boucle d'acceptation
        }

        LOG("Handshake SSL réussi pour le socket FD: " + std::to_string(clientSocket), "INFO");

        // Lancer un nouveau thread pour gérer cette connexion cliente authentifiée et sa session.
        // La fonction HandleClient prendra possession du socket et de l'objet SSL.
        // On utilise std::thread pour créer et lancer un nouveau thread.
        // On passe le pointeur raw de l'objet SSL (.release()) car le Client dans HandleClient prendra possession.
        std::thread clientThread(&Server::HandleClient, this, clientSocket, client_ssl.release());
        // Détacher le thread. Il s'exécutera indépendamment.
        // Note : Les threads détachés ne peuvent PAS être joints (wait).
        // Leur nettoyage est automatique à la fin de leur exécution.
        // Cela rend l'arrêt propre (attendre que tous les clients aient fini) plus complexe.
        clientThread.detach(); // Thread détaché


        LOG("Thread de gestion client détaché pour socket FD: " + std::to_string(clientSocket), "INFO");
    } // Fin de la boucle while(server_running.load())

    // Le programme atteint ce point lorsque la boucle d'acceptation se termine
    // (soit par un signal d'arrêt modifiant server_running, soit par une erreur accept() grave).

    // La socket serveur doit être fermée.
    close(serverSocket); // Fermer la socket serveur principale
    LOG("Socket serveur principal fermé.", "INFO");
}

// --- Implémentation de la méthode Server::AuthenticateClient ---
// Gère l'échange du message d'authentification avec le client, valide, vérifie les utilisateurs,
// et détermine le résultat (Échec, Succès, Nouveau).
// Envoie la réponse "AUTH FAIL:..." directement au client si l'authentification échoue.
// Si l'authentification réussit (Succès ou Nouveau), elle remplit le paramètre clientId_out.
AuthOutcome Server::AuthenticateClient(std::shared_ptr<Client> client_comm, std::string* clientId_out) {
    char buffer[1024]; // Buffer pour recevoir le message d'authentification. Taille raisonnable.
    
    // Tenter de recevoir le message d'authentification du client.
    // Cette fonction est bloquante pour une socket bloquante.
    // Elle retourne le nombre d'octets lus, 0 si le client déconnecte proprement, et <0 en cas d'erreur.
    int bytesRead = client_comm->receive(buffer, sizeof(buffer) - 1); 

    if (bytesRead <= 0) {
        // Gérer les cas où le client déconnecte ou s'il y a une erreur de réception AVANT même le message d'auth.
        if (bytesRead == 0) {
            LOG("Authentification - Client déconnecté proprement avant d'envoyer le message d'authentification. Socket FD: " + std::to_string(client_comm->getSocketFD()), "INFO");
        } else { // bytesRead < 0
            LOG("Authentification - Erreur de réception (" + std::to_string(bytesRead) + ") avant le message d'authentification. Socket FD: " + std::to_string(client_comm->getSocketFD()), "ERROR");
             // Afficher l'erreur OpenSSL si possible.
             unsigned long ssl_err = ERR_get_error(); 
             if (ssl_err != 0) {
                  char err_buf[256];
                  ERR_error_string_n(ssl_err, err_buf, sizeof(err_buf));
                  LOG("Erreur SSL pendant la réception d'authentification pour socket FD " + std::to_string(client_comm->getSocketFD()) + ": " + std::string(err_buf), "ERROR");
              }
        }
        // Dans ces cas, aucune réponse d'authentification ne peut être envoyée au client, 
        // car la connexion est déjà en train de se fermer ou une erreur s'est produite.
        return AuthOutcome::FAIL; // L'authentification a échoué (ou n'a pas pu être tentée).
    }
    
    // Null-terminer les données reçues pour pouvoir les manipuler comme une chaîne C++
    buffer[bytesRead] = '\0'; 
    std::string authMessage(buffer); // Convertir le buffer en std::string

    LOG("Authentification - Reçu de socket FD " + std::to_string(client_comm->getSocketFD()) + " : '" + authMessage + "'", "DEBUG");

    // Format attendu du message d'authentification : "ID:some_id,TOKEN:some_token"
    std::string id_prefix = "ID:";
    std::string token_prefix = ",TOKEN:";
    
    // Trouver la position des préfixes "ID:" et ",TOKEN:"
    size_t id_pos = authMessage.find(id_prefix);
    size_t token_pos = authMessage.find(token_prefix);

    // Valider le format de base du message reçu.
    // Les deux préfixes doivent être présents et le préfixe TOKEN doit venir après le préfixe ID.
    if (id_pos == std::string::npos || token_pos == std::string::npos || token_pos < id_pos + id_prefix.length()) {
        LOG("Authentification - Format de message invalide ou préfixes manquants dans : '" + authMessage + "'. Socket FD: " + std::to_string(client_comm->getSocketFD()), "WARNING");
        // Envoyer une réponse "AUTH FAIL: Invalid message format." au client.
        client_comm->send("AUTH FAIL: Invalid message format.", strlen("AUTH FAIL: Invalid message format."));
        return AuthOutcome::FAIL; // L'authentification échoue.
    }

    // Extraire les sous-chaînes correspondant à l'ID et au Token.
    // L'ID est entre "ID:" et ",TOKEN:". Le Token est après ",TOKEN:".
    std::string receivedId = authMessage.substr(id_pos + id_prefix.length(), token_pos - (id_pos + id_prefix.length()));
    std::string receivedToken = authMessage.substr(token_pos + token_prefix.length());

    // Supprimer les espaces blancs potentiels (espaces, tabulations, sauts de ligne, etc.)
    // au début et à la fin de l'ID et du Token extraits.
    receivedId.erase(0, receivedId.find_first_not_of(" \t\n\r\f\v"));
    receivedId.erase(receivedId.find_last_not_of(" \t\n\r\f\v") + 1);
    receivedToken.erase(0, receivedToken.find_first_not_of(" \t\n\r\f\v"));
    receivedToken.erase(receivedToken.find_last_not_of(" \t\n\r\f\v") + 1); // Correction de la ligne de suppression des espaces


    LOG("Authentification - ID extrait: '" + receivedId + "', Token extrait: '" + receivedToken + "'. Socket FD: " + std::to_string(client_comm->getSocketFD()), "DEBUG");

    // Vérifier si l'ID client extrait n'est pas vide après nettoyage.
    if (receivedId.empty()) {
        LOG("Authentification - ID client vide reçu après parsing. Socket FD: " + std::to_string(client_comm->getSocketFD()), "WARNING");
        client_comm->send("AUTH FAIL: Client ID cannot be empty.", strlen("AUTH FAIL: Client ID cannot be empty."));
        return AuthOutcome::FAIL; // L'authentification échoue.
    }

    // --- Vérifier si l'ID client existe dans la map des utilisateurs ---
    // Cette opération nécessite un accès thread-safe à la map 'users', car d'autres threads 
    // (comme HandleClient acceptant de nouvelles connexions) pourraient y accéder.
    { // Début du bloc pour limiter la portée du lock_guard
        std::lock_guard<std::mutex> lock(usersMutex); // Aquiert le verrou sur usersMutex. Libéré à la fin du bloc.
        
        // Chercher l'ID client reçu dans la map 'users'.
        auto it = users.find(receivedId);

        if (it != users.end()) {
            // --- Cas 1 : L'ID client a été trouvé dans la map 'users' (utilisateur existant). ---
            
            // Vérifier si le Token reçu correspond au Token enregistré dans la map pour cet ID.
            if (it->second == receivedToken) {
                // Le Token correspond - L'authentification est un SUCCÈS pour un utilisateur existant.
                *clientId_out = receivedId; // Remplir le paramètre de sortie clientId_out avec l'ID authentifié.
                LOG("Authentification - Succès pour ID existant : '" + receivedId + "'. Socket FD: " + std::to_string(client_comm->getSocketFD()), "INFO");
                // La réponse "AUTH SUCCESS" sera envoyée par HandleAuthenticatedClient plus tard.
                return AuthOutcome::SUCCESS; // Retourne le résultat Succès.
            } else {
                // Le Token reçu ne correspond pas au Token enregistré - L'authentification échoue.
                LOG("Authentification - Échec pour ID existant '" + receivedId + "' : Token invalide. Socket FD: " + std::to_string(client_comm->getSocketFD()), "WARNING");
                // Envoyer une réponse "AUTH FAIL: Invalid ID or Token." au client.
                client_comm->send("AUTH FAIL: Invalid ID or Token.", strlen("AUTH FAIL: Invalid ID or Token."));
                return AuthOutcome::FAIL; // Retourne le résultat Échec.
            }
        } else {
            // --- Cas 2 : L'ID client n'a PAS été trouvé dans la map 'users' (nouvel utilisateur potentiel). ---
            // Nous décidons de créer un nouvel utilisateur avec l'ID et le Token fournis.
            // Ajouter ce nouvel utilisateur à la map 'users' en mémoire.
            users[receivedId] = receivedToken; // Ajoute la nouvelle paire ID-Token à la map.
            *clientId_out = receivedId; // Remplir le paramètre de sortie clientId_out avec l'ID du nouvel utilisateur.
            LOG("Authentification - Nouvel ID reçu : '" + receivedId + "'. Ajouté à la liste des utilisateurs en mémoire. Socket FD: " + std::to_string(client_comm->getSocketFD()), "INFO");
            // Le fichier portefeuille pour ce nouvel utilisateur sera créé dans HandleClient après cet appel.
            // La réponse "AUTH NEW:..." sera envoyée par HandleAuthenticatedClient plus tard, incluant le Token.
            return AuthOutcome::NEW; // Retourne le résultat Nouveau.
        }
    } // Le verrou sur usersMutex est automatiquement libéré ici.

    // Cette partie du code ne devrait normalement jamais être atteinte si toutes les conditions sont couvertes.
    // Mais comme mesure de sécurité, si on arrive ici, considérons que l'authentification a échoué.
    LOG("Authentification - Condition inattendue atteinte. Retour par défaut à FAIL. Socket FD: " + std::to_string(client_comm->getSocketFD()), "ERROR");
    return AuthOutcome::FAIL; // Retourne le résultat Échec par défaut.
}

// --- Implémentation de la méthode Server::CreateWalletFile ---
// Crée un nouveau fichier portefeuille pour l'ID client spécifié dans le répertoire wallets_dir_path.
// Initialise le fichier avec des soldes par défaut (par exemple, 10000 DOLLARS et 0 SRD-BTC).
// Retourne true si le fichier a été créé et écrit avec succès, false en cas d'erreur.
bool Server::CreateWalletFile(const std::string& clientId) {
    // Construire le chemin complet du fichier portefeuille. Utilise le membre de classe wallets_dir_path.
    std::string walletFilename = wallets_dir_path + "/" + clientId + ".wallet"; 

    // Assurer que le répertoire des portefeuilles existe.
    std::error_code ec; // Utiliser la version non-throwing de create_directories
    if (!std::filesystem::create_directories(wallets_dir_path, ec)) {
        // create_directories a retourné false. Vérifier le code d'erreur.
        if (ec) { // Un code d'erreur a été défini.
            // Comparer l'objet error_code 'ec' directement avec la valeur d'énumération std::errc::file_exists.
            if (ec == std::errc::file_exists) {
                // Le répertoire existait déjà. Ce n'est PAS une erreur fatale pour notre objectif.
                LOG("Répertoire des portefeuilles existait déjà : " + wallets_dir_path, "DEBUG");
                // Continuer l'exécution pour créer le fichier portefeuille.
            } else {
                // Une autre erreur s'est produite pendant la création du répertoire. C'est une erreur fatale.
                LOG("Erreur: Impossible de créer le répertoire des portefeuilles: " + wallets_dir_path + ". Erreur: " + ec.message(), "ERROR");
                return false; // Indiquer un échec réel.
            }
        } else {
            // create_directories a retourné false mais aucun code d'erreur n'est set ? État inattendu.
            LOG("Avertissement: create_directories a retourné false pour " + wallets_dir_path + " mais aucun code d'erreur n'est set.", "WARNING");
            // Traiter cela comme un échec par mesure de sécurité.
            return false;
        }
    }   

    // Tenter d'ouvrir le fichier pour écriture.
    // std::ios::out : ouvre en écriture. Si le fichier n'existe pas, il est créé. S'il existe, son contenu est effacé.
    std::ofstream walletFile(walletFilename, std::ios::out); 
    
    // Vérifier si l'ouverture du fichier a réussi.
    if (!walletFile.is_open()) {
        // Si l'ouverture échoue, logguer l'erreur (en utilisant errno pour plus de détails système).
        LOG("Erreur: Impossible de créer/ouvrir le fichier portefeuille pour l'ID client: " + clientId + " à " + walletFilename + ". Erreur système : " + std::string(strerror(errno)), "ERROR");
        return false; // Indique un échec.
    }

    // Écrire les soldes initiaux dans le fichier. Format : "CURRENCY AMOUNT\n".
    walletFile << "DOLLARS 10000.0\n"; // Par exemple, 10000 unités de la monnaie fiduciaire "DOLLARS".
    walletFile << "SRD-BTC 0.0\n";    // Et 0 unités de la monnaie crypto "SRD-BTC".

    // Vérifier si l'écriture dans le fichier a échoué.
    if (walletFile.fail()) {
        LOG("Erreur lors de l'écriture des soldes initiaux dans le fichier portefeuille: " + walletFilename, "ERROR");
        walletFile.close(); // Fermer le fichier (même en cas d'erreur d'écriture).
        return false; // Indique un échec.
    }

    // Fermer le fichier après écriture réussie.
    walletFile.close(); 

    // Le log de succès (ex: "Fichier portefeuille créé...") sera fait dans HandleClient après avoir appelé cette méthode avec succès.
    return true; // Indique que le fichier a été créé et initialisé avec succès.
}

// --- Implémentation de la méthode Server::HandleClient ---
// Appelée par Server::StartServer dans un thread dédié pour chaque nouvelle connexion cliente acceptée.
// Cette fonction gère la phase initiale de la connexion, l'authentification,
// puis transfère le contrôle à HandleAuthenticatedClient si l'authentification réussit.
// Elle prend possession du descripteur de fichier socket et du pointeur raw SSL* passés en arguments.
void Server::HandleClient(int clientSocket, SSL* raw_ssl_ptr) { // La signature DOIT correspondre à la déclaration dans Server.h
    LOG("Thread HandleClient démarré pour socket FD: " + std::to_string(clientSocket), "DEBUG");

    // Créer un shared_ptr<Client> pour gérer la durée de vie du socket et de l'objet SSL*.
    // Le constructeur de Client prend possession de raw_ssl_ptr et clientSocket, et leur nettoyage 
    // (close(socket), SSL_free(ssl)) doit être géré dans le destructeur de Client.
    auto client_comm = std::make_shared<Client>(clientSocket, raw_ssl_ptr); 

    // Variable pour stocker l'ID du client après une authentification réussie.
    std::string clientId; 

    // --- Étape d'authentification ---
    AuthOutcome auth_result = this->AuthenticateClient(client_comm, &clientId);

    // Vérifier le résultat retourné par AuthenticateClient.
    if (auth_result != AuthOutcome::FAIL) {
        // --- L'authentification a réussi (soit SUCCESS, soit NEW utilisateur) ---
        LOG("Client ID: " + clientId + " authentification réussie (" + authOutcomeToString(auth_result) + "). Socket FD: " + std::to_string(client_comm->getSocketFD()), "INFO");

        // Si le résultat est AuthOutcome::NEW (nouvel utilisateur), il faut créer son fichier portefeuille sur disque.
        // La map des utilisateurs en mémoire a déjà été mise à jour par AuthenticateClient.
        if (auth_result == AuthOutcome::NEW) {
            // Appeler la nouvelle méthode CreateWalletFile pour créer le fichier.
            if (!this->CreateWalletFile(clientId)) {
                 // --- Gérer l'échec de la création du fichier portefeuille pour un nouvel utilisateur ---
                 LOG("Erreur grave: Impossible de créer le fichier portefeuille pour le nouvel ID client: " + clientId + ". Socket FD: " + std::to_string(client_comm->getSocketFD()), "ERROR");
                 // Informer le client de l'échec de la création du portefeuille via un message AUTH FAIL.
                 // La connexion sera fermée par le destructeur de client_comm lorsque HandleClient se termine.
                 client_comm->send("AUTH FAIL: Wallet creation failed. Connection closed.", strlen("AUTH FAIL: Wallet creation failed. Connection closed.")); // Envoyer l'erreur au client
                 LOG("Authentification échouée (échec création portefeuille). Connexion fermée pour client ID: " + clientId + ". Socket FD: " + std::to_string(client_comm->getSocketFD()), "ERROR");
                 return; // Quitter ce thread HandleClient car on ne peut pas continuer sans portefeuille.
            }
            LOG("Fichier portefeuille créé pour le nouveau client ID: " + clientId + " à " + wallets_dir_path + "/" + clientId + ".wallet. Socket FD: " + std::to_string(client_comm->getSocketFD()), "INFO");

            // --- Tâche importante : Sauvegarder la map des utilisateurs sur disque ---
            // C'est crucial pour que les nouveaux utilisateurs ajoutés en mémoire par AuthenticateClient
            // soient sauvegardés de manière persistante.
            // Utilise la méthode SaveUsers que nous avons corrigée précédemment.
            this->SaveUsers(usersFile_path); // Appeler SaveUsers pour sauvegarder les utilisateurs mis à jour
            LOG("Liste des utilisateurs sauvegardée sur disque après ajout de l'ID: " + clientId, "INFO");
        }

        // --- Si nous arrivons ici, l'authentification a réussi (SUCCESS ou NEW) et la création du portefeuille (si NEW) a fonctionné. ---
        // Le client est maintenant authentifié et prêt pour la boucle principale de session.

        // Créer un objet BotSession pour cette connexion authentifiée.
        // L'objet BotSession va gérer le Bot et le Client (objet client_comm) spécifiques à cette session.
        // Il prend ownership du shared_ptr<Client> et du clientId.
        auto session = std::make_shared<BotSession>(client_comm, clientId); 

        // Ajouter cette session à la collection de sessions actives gérée par le Server.
        // Cela permet au serveur de suivre les sessions en cours. L'accès à activeSessions doit être thread-safe.
        { // Début du bloc pour limiter la portée du verrou.
            std::lock_guard<std::mutex> lock(sessionsMutex); // Protège l'accès à la map activeSessions.
            activeSessions[clientId] = session; // Ajouter le shared_ptr à la map, indexé par l'ID client.
            LOG("Session ajoutée à activeSessions pour client ID: " + clientId + ". Socket FD: " + std::to_string(client_comm->getSocketFD()), "DEBUG");
        } // Le mutex est automatiquement libéré ici.

        // Enregistrer cette session auprès de la TransactionQueue globale.
        // La file de transactions a besoin de connaître les sessions actives pour potentiellement les notifier.
        // Elle stocke généralement un weak_ptr pour éviter les cycles de dépendance et permettre aux sessions de se terminer.
        txQueue.registerSession(session); LOG("Session enregistrée auprès de la TransactionQueue pour client ID: " + clientId + ". Socket FD: " + std::to_string(client_comm->getSocketFD()), "DEBUG");

        // Démarrer le thread dédié de la BotSession.
        // La méthode BotSession::start() est responsable de créer et lancer le thread 
        // où s'exécutera la méthode BotSession::run(), qui contient la boucle principale de traitement des requêtes.
        session->start(); LOG("BotSession thread démarré pour client ID: " + clientId + ". Socket FD: " + std::to_string(client_comm->getSocketFD()), "DEBUG");


        // Transférer le contrôle à HandleAuthenticatedClient.
        // C'est la méthode qui contient la boucle principale de réception et de traitement des commandes APRÈS authentification.
        // Le thread HandleClient va bloquer ici en attendant que HandleAuthenticatedClient se termine 
        // (par exemple, si le client se déconnecte, envoie la commande EXIT, ou si une erreur survient dans la session).
        // Il faut passer le shared_ptr<BotSession>, l'ID client, ET le résultat de l'authentification (SUCCESS ou NEW)
        // pour que HandleAuthenticatedClient puisse envoyer la bonne réponse initiale.
        this->HandleAuthenticatedClient(session, clientId, auth_result);

        // --- HandleAuthenticatedClient est retourné ---
        // Cela signifie que la session client s'est terminée (client déconnecté, commande EXIT, ou erreur dans la session).
        LOG("Thread HandleClient pour client ID: " + clientId + " termine après que HandleAuthenticatedClient est retourné. Socket FD: " + std::to_string(client_comm->getSocketFD()), "INFO");

        // Cleanup : Retirer cette session de la map activeSessions du Server.
        // C'est important pour libérer les ressources et maintenir la liste des sessions actives à jour.
        // La méthode removeSession doit être implémentée dans la classe Server.
        this->removeSession(clientId); // Appeler une méthode pour retirer la session par son ID. (Implémentation de removeSession à ajouter).
        LOG("Session retirée de activeSessions pour client ID: " + clientId + ". Socket FD: " + std::to_string(client_comm->getSocketFD()), "DEBUG");


    } else { // Le résultat d'authentification était AuthOutcome::FAIL
        // --- L'authentification a échoué ---
        // La méthode AuthenticateClient a déjà envoyé le message "AUTH FAIL:..." au client.
        // Le thread HandleClient n'a plus rien à faire pour cette connexion.
        LOG("Authentification échouée pour client sur Socket FD: " + std::to_string(client_comm->getSocketFD()) + ". Connexion fermée.", "INFO");
        // Le client_comm shared_ptr sortira de portée à la fin de HandleClient, ce qui fermera la connexion.
        // Le thread HandleClient se termine.
        return; // Quitter ce thread HandleClient.
    }

    // Le shared_ptr<Client> client_comm (et potentiellement le shared_ptr<BotSession> 'session', 
    // si c'était la dernière référence) sort de portée ici. Leurs destructeurs sont appelés, 
    // ce qui devrait gérer la fermeture du socket et la libération des objets SSL et Bot.
}

// --- Traiter les requêtes spécifiques reçues d'un client ---
// Cette méthode est appelée par HandleClient.
// Elle prend un shared_ptr vers la BotSession pour pouvoir interagir avec le Bot et le Client associés.
void Server::ProcessRequest(std::shared_ptr<BotSession> session, const std::string& request) {
    // Vérifications de sécurité (session valide, client_comm valide)
    // On vérifie si session est valide, puis si l'objet Client à l'intérieur l'est aussi.
    if (!session || !session->getClient()) {
        LOG("Erreur: ProcessRequest appelée avec session ou Client nul.", "ERROR");
        return; // Quitte la fonction si les préconditions ne sont pas remplies
    }

    // Accéder à l'ID client, au Bot, et à l'interface de communication Client via le shared_ptr BotSession
    std::string clientId = session->getId(); // Utilise getId() de BotSession
    auto bot = session->getBot(); // shared_ptr vers le Bot (si BotSession le fournit)
    auto client_comm = session->getClient(); // Utilise le membre 'client' renommé (getClient()) de BotSession


    LOG("Traitement de la requête de client ID: " + clientId + ": '" + request + "'", "DEBUG");

    // --- Logique de traitement des commandes ---
    std::string response = ""; // Initialise la réponse

    // Traiter la requête "SHOW WALLET"
    if (request == "SHOW WALLET") { // Utilise == pour comparaison exacte
        // Logique pour obtenir les infos du portefeuille (du Bot de cette session)
        // Utilise getBalance qui est thread-safe (grâce au mutex interne du Bot)
        double dollars = 0.0; // Valeurs par défaut en cas de problème
        double crypto = 0.0;
        if (bot) { // Vérifie que le bot est valide
            dollars = bot->getBalance("DOLLARS");
            crypto = bot->getBalance("SRD-BTC");
        } else {
             LOG("Erreur: Bot est nul lors du traitement SHOW WALLET pour client ID: " + clientId, "ERROR");
             response = "ERROR: Bot internal error.";
             // Pas de return ici, on enverra la réponse d'erreur.
        }

        if (response.empty()) { // Si pas déjà défini par une erreur du bot
            std::string walletInfo = "SRD=" + std::to_string(dollars) + ", BTC=" + std::to_string(crypto);
            response = "WALLET INFO:" + walletInfo;
        }


        // Envoyer la réponse au client via l'objet Client
        if (!client_comm->send(response.c_str(), response.size())) {
            LOG("Échec envoi réponse WALLET INFO ou ERREUR à client ID: " + clientId, "ERROR");
            // En cas d'erreur d'envoi, on pourrait signaler l'arrêt de la session
            session->stop(); // Signale l'arrêt si l'envoi échoue. HandleAuthenticatedClient gérera la sortie de boucle.
        } else {
            LOG("Réponse WALLET INFO ou ERREUR envoyée à client ID: " + clientId, "INFO");
        }

    }
    // Traiter la requête "SHOW TRANSACTION HISTORY"
    else if (request == "SHOW TRANSACTION HISTORY") { // Utilise == pour comparaison exacte
        // Utiliser le chemin du fichier historique passé au serveur (membre de la classe Server)
        // Le chemin est stocké dans transactionHistoryFile_path.
        std::ifstream file(transactionHistoryFile_path); // <<<--- Utilise le membre transactionHistoryFile_path

        std::string historyContent;
        if (file.is_open()) {
             std::stringstream ss;
             ss << file.rdbuf(); // Lire tout le contenu du fichier dans le stringstream
             historyContent = ss.str();
             file.close();
             LOG("Fichier historique transactions lu pour client ID: " + clientId, "INFO");
        } else {
             LOG("Erreur : Impossible d'ouvrir le fichier historique transactions " + transactionHistoryFile_path + " pour client ID: " + clientId, "ERROR");
             historyContent = "ERROR: Impossible de lire l'historique des transactions."; // Message d'erreur pour le client
        }

        response = "TRANSACTION HISTORY:" + historyContent; // Prépare la réponse

        // Envoyer la réponse au client
        if (!client_comm->send(response.c_str(), response.size())) {
             LOG("Échec envoi réponse HISTOIRE TRANSACTIONS ou ERREUR à client ID: " + clientId, "ERROR");
             session->stop(); // Signale l'arrêt si l'envoi échoue.
        } else {
             LOG("Réponse HISTOIRE TRANSACTIONS ou ERREUR envoyée à client ID: " + clientId, "INFO");
        }

    }
    // Traiter la commande "EXIT"
    else if (request == "EXIT") { // Utilise == pour comparaison exacte
         LOG("Requête EXIT reçue de client ID: " + clientId + ". Signal d'arrêt de la session.", "INFO");
         session->stop(); // <<<--- Signale à la session de s'arrêter. La boucle dans HandleAuthenticatedClient se terminera.
         response = "Goodbye!"; // Message de confirmation avant l'arrêt.
         // Envoyer le message de confirmation. Si l'envoi échoue, la session s'arrêtera de toute façon.
         client_comm->send(response.c_str(), response.size()); // Tentative d'envoi
         // Pas besoin de vérifier le retour ici, la session s'arrête juste après.

    }


    // Gérer les commandes inconnues (si elles ne sont pas gérées par les blocs if/else if ci-dessus)
    // Si 'response' est encore vide ici, c'est que la commande n'a pas été reconnue par les vérifications.
    if (response.empty()) {
        LOG("Requête non reconnue de client ID: " + clientId + ": '" + request + "'", "WARNING");
        response = "ERROR: Unknown command.";
         // Envoyer la réponse d'erreur pour commande inconnue
         if (!client_comm->send(response.c_str(), response.size())) {
              LOG("Échec envoi réponse 'commande inconnue' à client ID: " + clientId, "ERROR");
              session->stop(); // Signale l'arrêt si l'envoi échoue.
         } else {
             LOG("Réponse 'commande inconnue' envoyée à client ID: " + clientId, "INFO");
         }
    }


    // --- Fin Logique de traitement des commandes ---
    // La fonction se termine ici. Le thread HandleAuthenticatedClient attendra la prochaine requête dans sa boucle.
}


// --- Méthode pour arrêter le serveur proprement ---
// Appelée par le destructeur du Server ou en cas d'erreur fatale/signal d'arrêt.
void Server::StopServer() {
    LOG("Arrêt du serveur demandé...", "INFO");

    { // Utilise un bloc pour limiter la portée du lock_guard
        std::lock_guard<std::mutex> lock(sessionsMutex); // Protège l'accès à activeSessions
        for (auto const& [clientId, session] : activeSessions) {
            if (session) {
                session->stop(); // Appelle la méthode stop() de la BotSession
            }
        }
    } // Le verrou sur sessionsMutex est relâché.


    // Sauvegarder la liste des utilisateurs sur disque.
    // Utilise le membre usersFile_path pour le chemin.
    // La méthode SaveUsers elle-même gère le locking via usersMutex.
    this->SaveUsers(usersFile_path);

    // Sauvegarder le compteur de transactions statique sur disque.
    // Utilise le membre transactionCounterFile_path pour le chemin.
    // La méthode statique Transaction::saveCounter gère elle-même l'écriture et son mutex interne.
    Transaction::saveCounter(transactionCounterFile_path);

    // Le contexte SSL (ctx) sera libéré par son destructeur (UniqueSSLCTX) à la fin de vie de l'objet Server.

    LOG("Serveur arrêté.", "INFO");
}


// --- Implémentation de la méthode pour désenregistrer une session (appelée par BotSession à l'arrêt) ---
// Cette méthode est appelée par le thread HandleClient lorsqu'une session se termine
// ou par le destructeur de BotSession. Son rôle est de retirer la référence de la map du serveur.
void Server::unregisterSession(const std::string& clientId) {
    LOG("Demande de désenregistrement de session pour client ID: " + clientId + " reçue.", "INFO");
    // Protège l'accès à la map des sessions actives
    std::lock_guard<std::mutex> lock(sessionsMutex);
    auto it = activeSessions.find(clientId); // Cherche l'entrée pour cet ID client
    if (it != activeSessions.end()) {
        // Supprime l'entrée de la map. Le shared_ptr contenu sera détruit s'il n'est plus référencé ailleurs.
        activeSessions.erase(it);
        LOG("Session pour client ID: " + clientId + " retirée de activeSessions.", "INFO");
    } else {
        // L'entrée n'a pas été trouvée (déjà retirée ?).
        LOG("Session pour client ID: " + clientId + " non trouvée dans activeSessions (déjà retirée?).", "WARNING");
    }
}

// --- Implémentation de la méthode Server::HandleAuthenticatedClient ---
// Cette fonction est le cœur du thread dédié à une session client APRÈS authentification.
// Elle est appelée par HandleClient. Elle contient la boucle principale 
// qui reçoit les commandes du client et appelle ProcessRequest pour les traiter.
// Elle reçoit un shared_ptr vers la BotSession (qui contient le Client et le Bot) 
// et le résultat de l'authentification pour envoyer la réponse initiale.
void Server::HandleAuthenticatedClient(std::shared_ptr<BotSession> session, const std::string& clientId, AuthOutcome authOutcome) {
    // Logguer le démarrage de ce thread spécifique pour cette session.
    LOG("Thread HandleAuthenticatedClient démarré pour client ID: " + clientId + ". Socket FD: " + std::to_string(session->getClient()->getSocketFD()), "DEBUG");

    // Récupérer le shared_ptr vers l'objet Client depuis la session.
    // Cet objet encapsule le socket et l'objet SSL et sert à communiquer avec le client.
    auto client_comm = session->getClient(); 

    // Vérification basique : S'assurer que l'objet client_comm est valide.
    if (!client_comm) {
        LOG("Erreur critique: Objet Client nul dans HandleAuthenticatedClient pour ID: " + clientId + ". Sortie du thread.", "ERROR");
        session->stop(); // Signaler à la BotSession qu'elle doit s'arrêter si l'objet Client est manquant.
        return; // Quitter ce thread HandleAuthenticatedClient.
    }

    // --- Envoyer la réponse initiale d'authentification au client ---
    // C'est le premier message que le client attend après avoir envoyé son message d'auth.
    // Le format du message dépend si l'utilisateur était existant (SUCCESS) ou nouveau (NEW).
    std::string authResponseToClient;
    
    if (authOutcome == AuthOutcome::SUCCESS) {
        // Si l'authentification était un SUCCÈS (utilisateur existant), envoyer "AUTH SUCCESS".
        authResponseToClient = "AUTH SUCCESS";
        LOG("Envoi de la réponse 'AUTH SUCCESS' au client ID: " + clientId + ". Socket FD: " + std::to_string(client_comm->getSocketFD()), "INFO");

    } else if (authOutcome == AuthOutcome::NEW) {
        // Si l'authentification était NEW (nouvel utilisateur créé par le serveur),
        // il faut envoyer "AUTH NEW:assignedId,assignedToken".
        // L'ID assigné est simplement le 'clientId' de cette session.
        // Il faut récupérer le Token assigné qui a été stocké dans la map users par AuthenticateClient.
        std::string token;
        { // Début du bloc pour limiter la portée du verrou sur la map users.
            std::lock_guard<std::mutex> lock(usersMutex); // Aquiert le verrou pour accéder à la map users.
            auto it = users.find(clientId); // Chercher l'utilisateur par son ID.
            if (it != users.end()) {
                token = it->second; // Récupérer le Token associé à cet ID.
                // Construire la réponse au format "AUTH NEW:assignedId,assignedToken".
                authResponseToClient = "AUTH NEW:" + clientId + "," + token; 
                LOG("Envoi de la réponse 'AUTH NEW' (avec ID et Token) au client ID: " + clientId + ". Socket FD: " + std::to_string(client_comm->getSocketFD()), "INFO");
            } else {
                // --- Cas d'erreur inattendu : L'utilisateur n'est pas trouvé dans la map users ---
                // Cela ne devrait pas arriver si AuthenticateClient a correctement ajouté le nouvel utilisateur 
                // et que le AuthOutcome est NEW. Logguer une erreur grave et signaler au client un problème interne.
                LOG("Erreur interne grave: Utilisateur '" + clientId + "' non trouvé dans la map users malgré AuthOutcome::NEW. Impossible d'envoyer la réponse 'AUTH NEW'. Socket FD: " + std::to_string(client_comm->getSocketFD()), "ERROR");
                // Envoyer une réponse indiquant une erreur interne au client.
                authResponseToClient = "AUTH FAIL: Internal server error during new account setup."; 
                session->stop(); // Signaler à la BotSession qu'elle doit s'arrêter à cause de cette erreur critique.
            }
        } // Le verrou sur usersMutex est automatiquement libéré ici.

    } else { // Cas d'erreur inattendu : HandleAuthenticatedClient a été appelé avec AuthOutcome::FAIL
         // Cela ne devrait pas arriver si HandleClient ne l'appelle que pour SUCCESS ou NEW.
         // Comme mesure de sécurité, logguer une erreur et signaler l'arrêt de la session.
         LOG("Erreur interne grave: HandleAuthenticatedClient appelé avec AuthOutcome::FAIL pour ID: " + clientId + ". Socket FD: " + std::to_string(client_comm->getSocketFD()), "ERROR");
         authResponseToClient = "AUTH FAIL: Internal server error (bad authentication state).";
         session->stop(); // Signaler à la session qu'elle doit s'arrêter.
    }

    // Envoyer la réponse d'authentification construite (AUTH SUCCESS/NEW ou erreur interne) au client.
    // On ne tente l'envoi que si la session est toujours marquée comme 'running' et si on a un message à envoyer.
    if (session->isRunning() && !authResponseToClient.empty()) { 
        if (!client_comm->send(authResponseToClient.c_str(), authResponseToClient.size())) {
            // Si l'envoi de la réponse d'authentification échoue, c'est que la connexion a un problème.
            LOG("Échec envoi réponse d'authentification initiale ('" + authResponseToClient + "') à client ID: " + clientId + ". Socket FD: " + std::to_string(client_comm->getSocketFD()), "ERROR");
            session->stop(); // Signaler à la session qu'elle doit s'arrêter suite à l'échec d'envoi.
        } else {
             // Le log de succès (INFO level) a déjà été fait plus haut, pas besoin de répéter ici.
        }
    }

    // Si la session a été marquée comme 'arrêtée' (session->stop() a été appelé) 
    // à cause d'une erreur ci-dessus (objet Client nul, token non trouvé, mauvais AuthOutcome, échec envoi réponse),
    // la condition de la boucle while ci-dessous (session->isRunning()) sera fausse, et le thread sortira immédiatement.


    // --- Boucle principale pour recevoir et traiter les requêtes client APRÈS authentification ---
    // Cette boucle continue tant que la BotSession est marquée comme étant en cours d'exécution.
    // Le flag 'running' de la session est mis à false quand le client envoie la commande 'EXIT',
    // ou s'il y a une erreur critique de communication dans receive/send.
    while (session->isRunning()) {
        std::string request; // Variable pour stocker la requête client reçue sous forme de chaîne C++.
        char requestBuffer[4096]; // Buffer pour stocker les données brutes reçues de la socket. Ajuster la taille au besoin.
        int bytesRead = 0; // Pour stocker le nombre d'octets reçus par client_comm->receive.

        // --- Recevoir une requête du client ---
        // C'est un appel bloquant. Le thread va attendre ici jusqu'à ce que des données soient reçues,
        // que la connexion soit fermée par le client, ou qu'une erreur survienne.
        // L'implémentation de client_comm->receive doit gérer la traduction des appels SSL_read.
        // La méthode session->stop() (appelée par exemple sur réception de 'EXIT' ou par le Server lors de l'arrêt global)
        // doit signaler à ce receive bloquant de retourner (par exemple, en fermant la socket sous-jacente).
        bytesRead = client_comm->receive(requestBuffer, sizeof(requestBuffer) - 1);

        // --- Vérifier si la session est toujours marquée comme running après que receive() est retourné ---
        // C'est important si le flag 'running' de la session a été mis à false 
        // pendant que ce thread était bloqué dans l'appel receive (par ex. signal d'arrêt global du serveur, 
        // ou appel externe à session->stop()). Si ce n'est plus running, on sort.
        if (!session->isRunning()) {
             LOG("Session pour client ID: " + clientId + " arrêtée pendant/après la réception de requête. Sortie de la boucle de traitement des requêtes. Socket FD: " + std::to_string(client_comm->getSocketFD()), "INFO");
             break; // Sortir de la boucle principale de traitement des requêtes.
        }

        // --- Vérifier le résultat de l'opération de réception ---
        if (bytesRead <= 0) {
            // Un retour de 0 octets lus indique généralement une déconnexion propre par le client (il a fermé sa socket).
            // Un retour inférieur à 0 indique une erreur lors de la réception.
            if (bytesRead == 0) {
                 LOG("Client ID: " + clientId + " déconnecté proprement. Socket FD: " + std::to_string(client_comm->getSocketFD()), "INFO");
            } else { // bytesRead < 0 (Erreur de réception)
                 LOG("Client ID: " + clientId + " erreur de réception (" + std::to_string(bytesRead) + "). Déconnexion. Socket FD: " + std::to_string(client_comm->getSocketFD()), "ERROR");
                 // Logguer plus de détails sur l'erreur SSL si possible.
                 unsigned long ssl_err = ERR_get_error(); 
                 if (ssl_err != 0) {
                      char err_buf[256];
                      // Utilise ERR_error_string_n si ERR_error_string_r est indéfini dans ta version OpenSSL.
                      ERR_error_string_n(ssl_err, err_buf, sizeof(err_buf)); 
                      LOG("Erreur SSL lors de la réception pour ID " + clientId + ", Socket FD " + std::to_string(client_comm->getSocketFD()) + ": " + std::string(err_buf), "ERROR");
                 }
            }
            session->stop(); // Signaler à la BotSession qu'elle doit s'arrêter si le client s'est déconnecté ou si une erreur est survenue.
            break; // Sortir de la boucle principale de traitement des requêtes, car la connexion n'est plus viable.
        }

        // --- Si bytesRead > 0, c'est qu'une requête a été reçue avec succès ---
        requestBuffer[bytesRead] = '\0'; // Null-terminer les données reçues pour les traiter comme une chaîne C-style.
        request = requestBuffer; // Convertir les données reçues du buffer en un std::string.

        LOG("Client ID: " + clientId + ", Socket FD: " + std::to_string(client_comm->getSocketFD()) + ", Requête reçue: '" + request + "'", "INFO");

        // Appeler la méthode ProcessRequest pour gérer la commande client.
        // ProcessRequest est responsable d'analyser la commande (SHOW WALLET, MAKE TRANSACTION, etc.),
        // d'interagir avec le Bot/Session au besoin, et d'envoyer la réponse appropriée 
        // de retour au client en utilisant session->getClient()->send().
        // ProcessRequest est aussi responsable d'appeler session->stop() si la commande est par exemple "EXIT".
        this->ProcessRequest(session, request); 

        // La boucle continue à la prochaine itération pour recevoir la requête suivante,
        // SAUF si l'appel à ProcessRequest a entraîné l'appel de session->stop().
        // Dans ce cas, la condition 'while(session->isRunning())' au début de la prochaine itération 
        // sera fausse, et la boucle se terminera.

    } // --- Fin de la boucle while (session->isRunning()) ---

    // La boucle se termine lorsque session->isRunning() devient false.
    // Ce thread HandleAuthenticatedClient est maintenant sur le point de se terminer.
    LOG("Thread HandleAuthenticatedClient termine pour client ID: " + clientId + ". Socket FD: " + std::to_string(client_comm->getSocketFD()) + ". La session est arrêtée.", "INFO");

    // Le shared_ptr<BotSession> 'session' passé à cette fonction sortira de portée à la fin de l'exécution de ce thread.
    // Son destructeur sera appelé, ce qui devrait déclencher la destruction de l'objet Bot et du shared_ptr<Client> qu'il contient.
    // La destruction de l'objet Client devrait finalement fermer la socket et libérer l'objet SSL.
}

// --- Implémentation de la méthode Server::removeSession ---
// Retire une session de la map activeSessions en utilisant l'ID client comme clé.
// Cette méthode est appelée lorsque le thread HandleClient se termine pour une session donnée.
void Server::removeSession(const std::string& clientId) {
    // Accéder à la map activeSessions de manière thread-safe.
    std::lock_guard<std::mutex> lock(sessionsMutex); // Protège l'accès à la map.
    
    // Effacer l'entrée correspondant à l'ID client.
    // activeSessions.erase() retourne le nombre d'éléments supprimés (0 ou 1).
    size_t removed_count = activeSessions.erase(clientId); 

    if (removed_count > 0) {
        LOG("Session pour client ID: " + clientId + " retirée de activeSessions.", "DEBUG");
    } else {
        // Cela peut arriver si une session n'a jamais été ajoutée (ex: échec auth très précoce)
        // ou si elle a déjà été retirée (moins probable avec cette structure).
        LOG("Avertissement: Tentative de retirer la session pour client ID: " + clientId + " de activeSessions, mais elle n'a pas été trouvée dans la map.", "WARNING");
    }
    // Le verrou est libéré automatiquement ici.
}