#ifndef CLIENT_INITIATOR_H
#define CLIENT_INITIATOR_H

#include <string>
#include <memory>
#include <stdexcept>
#include <openssl/ssl.h>


#include "ServerConnection.h"
#include "OpenSSLDeleters.h"
#include "Logger.h"
#include "Utils.h"

// --- Classe ClientInitiator ---
// Gère l'initiation des connexions sortantes et le handshake SSL côté client.
class ClientInitiator {
public:
    // Constructeur par défaut.
    ClientInitiator();

    // Initialise le contexte SSL client.
    // Retourne un UniqueSSLCTX gérant le contexte SSL client, ou nullptr en cas d'échec.
    UniqueSSLCTX InitClientCTX();

    // Établit une connexion TCP et effectue le handshake SSL avec un serveur.
    // Param host: L'adresse ou nom d'hôte du serveur.
    // Param port: Le port du serveur.
    // Param ctx_raw: Le pointeur brut vers le contexte SSL client initialisé.
    // Retourne un shared_ptr vers un objet ServerConnection si succès, nullptr sinon.
    std::shared_ptr<ServerConnection> ConnectToServer(const std::string& host, int port, SSL_CTX* ctx_raw);

    // Le destructeur par défaut est suffisant.

}; 

#endif 