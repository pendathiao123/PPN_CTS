// src/code/ClientInitiator.cpp - Implémentation de la classe ClientInitiator

#include "../headers/ClientInitiator.h" // Inclut l'en-tête de la classe
#include "../headers/ServerConnection.h"  // Pour utiliser ServerConnection
#include "../headers/OpenSSLDeleters.h"    // Pour UniqueSSLCTX, UniqueSSL
#include "../headers/Logger.h"             // Pour le logging
#include "../headers/Utils.h"              // Pour openssl_debug_callback et autres utilitaires

#include <iostream>      // std::cerr
#include <stdexcept>     // std::runtime_error
#include <string>        // std::string
#include <memory>        // std::shared_ptr, std::unique_ptr
#include <sys/socket.h>  // socket, connect
#include <netinet/in.h>  // sockaddr_in, INADDR_ANY
#include <arpa/inet.h>   // inet_pton
#include <unistd.h>      // close
#include <openssl/ssl.h> // SSL_CTX_new, SSL_new, SSL_set_fd, SSL_connect, SSL_CTX_free (via UniqueSSLCTX)
#include <openssl/err.h> // ERR_print_errors_fp, ERR_get_error
#include <cstring>       // strerror
#include <cerrno>        // errno


// --- Implémentation du Constructeur ClientInitiator ---
ClientInitiator::ClientInitiator() {
    LOG("ClientInitiator::ClientInitiator DEBUG : Objet ClientInitiator créé.", "DEBUG");
    // L'initialisation globale OpenSSL est gérée dans main_cli.cpp
}


// --- Implémentation de la méthode ClientInitiator::InitClientCTX ---
UniqueSSLCTX ClientInitiator::InitClientCTX() {
    LOG("ClientInitiator::InitClientCTX DEBUG : Initialisation contexte SSL client...", "DEBUG");

    // Utilise TLS_client_method() pour un contexte client
    UniqueSSLCTX context(SSL_CTX_new(TLS_client_method()));
    if (!context) {
        LOG("ClientInitiator::InitClientCTX ERROR : Impossible de créer le contexte SSL client.", "ERROR");
        ERR_print_errors_fp(stderr); // Log OpenSSL error
        return nullptr; // Retourne un unique_ptr vide
    }
    LOG("ClientInitiator::InitClientCTX DEBUG : Contexte SSL client créé.", "DEBUG");

    // Configure le contexte client si nécessaire (ex: charger un certificat client, configurer des options)
    // SSL_CTX_set_info_callback(context.get(), openssl_debug_callback); // Peut être utile pour débug client aussi
    SSL_CTX_set_min_proto_version(context.get(), TLS1_2_VERSION); // Exemple: Minimum TLS 1.2
    SSL_CTX_set_options(context.get(), SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3); // Exemples d'options

    // Si le serveur demande l'authentification client par certificat (pas notre cas ici),
    // il faudrait charger le certificat et la clé privée du client ici :
    // SSL_CTX_use_certificate_file(context.get(), "client.pem", SSL_FILETYPE_PEM);
    // SSL_CTX_use_PrivateKey_file(context.get(), "client.key", SSL_FILETYPE_PEM);
    // if (!SSL_CTX_check_private_key(context.get())) { ... }

    LOG("ClientInitiator::InitClientCTX INFO : Contexte SSL client initialisé.", "INFO");
    return context; // Retourne le contexte dans un unique_ptr
}


// --- Implémentation de la méthode ClientInitiator::ConnectToServer ---
// Prend maintenant 3 arguments, y compris le contexte SSL client.
std::shared_ptr<ServerConnection> ClientInitiator::ConnectToServer(const std::string& host, int port, SSL_CTX* ctx_raw) {
    LOG("ClientInitiator::ConnectToServer INFO : Tentative de connexion à " + host + ":" + std::to_string(port), "INFO");

    if (!ctx_raw) {
         LOG("ClientInitiator::ConnectToServer ERROR : Contexte SSL client (ctx_raw) est null. Annulation de la connexion.", "ERROR");
         return nullptr; // Contexte nul, impossible de continuer.
    }

    // 1. Création de la socket TCP.
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        LOG("ClientInitiator::ConnectToServer ERROR : Échec de la création de la socket client. Erreur: " + std::string(strerror(errno)), "ERROR");
        return nullptr;
    }
    LOG("ClientInitiator::ConnectToServer DEBUG : Socket client créé avec FD: " + std::to_string(clientSocket), "DEBUG");

    // 2. Configuration de l'adresse du serveur.
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    // Convertir l'adresse IP (string) en format binaire.
    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        LOG("ClientInitiator::ConnectToServer ERROR : Adresse serveur invalide ou non supportée: '" + host + "'. Erreur: " + std::string(strerror(errno)), "ERROR");
        close(clientSocket); // Ferme la socket
        return nullptr;
    }
    LOG("ClientInitiator::ConnectToServer DEBUG : Adresse serveur configurée pour " + host, "DEBUG");

    // 3. Connexion au serveur.
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        LOG("ClientInitiator::ConnectToServer ERROR : Échec de la connexion TCP au serveur " + host + ":" + std::to_string(port) + ". Erreur: " + std::string(strerror(errno)), "ERROR");
        close(clientSocket); // Ferme la socket
        return nullptr;
    }
    LOG("ClientInitiator::ConnectToServer INFO : Connexion TCP établie au serveur " + host + ":" + std::to_string(port) + ". Socket FD: " + std::to_string(clientSocket), "INFO");

    // 4. Initiation de la session SSL.
    UniqueSSL ssl_ptr(SSL_new(ctx_raw)); // Utilise le contexte client passé en argument
    if (!ssl_ptr) {
        LOG("ClientInitiator::ConnectToServer ERROR : Impossible de créer l'objet SSL. Socket FD: " + std::to_string(clientSocket), "ERROR");
        ERR_print_errors_fp(stderr);
        close(clientSocket);
        return nullptr;
    }
    LOG("ClientInitiator::ConnectToServer DEBUG : Objet SSL créé pour socket FD: " + std::to_string(clientSocket), "DEBUG");


    // Associer la socket TCP à l'objet SSL.
    if (SSL_set_fd(ssl_ptr.get(), clientSocket) <= 0) {
        LOG("ClientInitiator::ConnectToServer ERROR : Échec de l'association socket/SSL_object (SSL_set_fd). Socket FD: " + std::to_string(clientSocket), "ERROR");
        ERR_print_errors_fp(stderr);
        close(clientSocket);
        return nullptr; // UniqueSSL gérera la libération de l'objet SSL.
    }
    LOG("ClientInitiator::ConnectToServer DEBUG : Socket FD " + std::to_string(clientSocket) + " associé à l'objet SSL.", "DEBUG");


    // 5. Lancement du handshake SSL (client side).
    int ssl_connect_ret = SSL_connect(ssl_ptr.get()); // Ceci démarre le handshake SSL

    if (ssl_connect_ret <= 0) {
        int ssl_err = SSL_get_error(ssl_ptr.get(), ssl_connect_ret);
        // Gérer les erreurs non bloquantes (WANT_READ/WRITE) si en mode non bloquant.
        // En mode bloquant, tout <= 0 avec une erreur différente est un échec.
        if (ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE) {
            LOG("ClientInitiator::ConnectToServer WARNING : SSL_connect() retourné WANT_READ/WRITE pour socket FD: " + std::to_string(clientSocket) + ". Erreur SSL: " + std::to_string(ssl_err) + ". Connexion fermée car handshake non immédiat en mode bloquant.", "WARNING");
            // La connexion sera fermée par la destruction de unique_ptr/ServerConnection
            return nullptr;
        } else {
            LOG("ClientInitiator::ConnectToServer ERROR : Échec du handshake SSL. Socket FD: " + std::to_string(clientSocket) + ". Erreur SSL: " + std::to_string(ssl_err), "ERROR");
            ERR_print_errors_fp(stderr); // Log OpenSSL error
            // La connexion sera fermée par la destruction de unique_ptr/ServerConnection
            return nullptr;
        }
    }

    LOG("ClientInitiator::ConnectToServer INFO : Handshake SSL réussi avec le serveur. Socket FD: " + std::to_string(clientSocket), "INFO");

    // 6. Créer l'objet ServerConnection pour gérer la communication post-handshake.
    // L'objet ServerConnection prend la propriété de la socket et de l'objet SSL.
    std::shared_ptr<ServerConnection> connection = std::make_shared<ServerConnection>(clientSocket, ssl_ptr.release()); // Transfert la propriété du SSL*

    if (!connection || !connection->isConnected()) {
         LOG("ClientInitiator::ConnectToServer ERROR : Échec de la création de l'objet ServerConnection.", "ERROR");
         // La socket et le SSL* devraient être nettoyés par le destructeur de ServerConnection s'il est créé mais non connecté.
         // Sinon, ils ont été fermés/libérés plus tôt.
         return nullptr;
    }

    LOG("ClientInitiator::ConnectToServer INFO : Objet ServerConnection créé et connecté.", "INFO");

    return connection; // Retourne le pointeur partagé vers l'objet de connexion.
}