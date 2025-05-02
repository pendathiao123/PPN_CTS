// src/headers/ClientInitiator.h - En-tête de la classe ClientInitiator

#ifndef CLIENT_INITIATOR_H
#define CLIENT_INITIATOR_H

// --- Includes nécessaires pour les déclarations ---
#include <string>
#include <memory> // shared_ptr, unique_ptr
#include <stdexcept>

// Inclure les headers des composants avec lesquels ClientInitiator interagit :
#include "ServerConnection.h"   // ConnectToServer retourne un shared_ptr<ServerConnection>
#include "OpenSSLDeleters.h"    // Pour UniqueSSLCTX, UniqueSSL (gestion RAII)
#include "Logger.h"             // Pour le logging
#include "Utils.h"              // Pour openssl_debug_callback (si utilisé dans ce header, sinon juste dans le .cpp)

#include <openssl/ssl.h> // Nécessaire pour SSL_CTX


// --- Classe ClientInitiator : Gère l'initiation des connexions clientes ---
/**
 * @brief Gère l'initiation des connexions sortantes et le handshake SSL côté client.
 */
class ClientInitiator {
public:
    /**
     * @brief Constructeur par défaut.
     */
    ClientInitiator();

    /**
     * @brief Initialise le contexte SSL client.
     * @return Un UniqueSSLCTX gérant le contexte SSL client, ou nullptr en cas d'échec.
     */
    UniqueSSLCTX InitClientCTX(); // <-- NOUVELLE DÉCLARATION

    /**
     * @brief Établit une connexion TCP et effectue le handshake SSL avec un serveur.
     * @param host L'adresse ou nom d'hôte du serveur.
     * @param port Le port du serveur.
     * @param ctx_raw Le pointeur brut vers le contexte SSL client initialisé. <-- 3ème ARGUMENT
     * @return Un shared_ptr vers un objet ServerConnection si succès, nullptr sinon.
     */
    std::shared_ptr<ServerConnection> ConnectToServer(const std::string& host, int port, SSL_CTX* ctx_raw); // <-- SIGNATURE AVEC 3 ARGUMENTS

    // Le destructeur par défaut est suffisant.

}; // Fin de la classe ClientInitiator

#endif // CLIENT_INITIATOR_H