#include "../headers/Client.h"
#include "../headers/Logger.h" 
#include "../headers/OpenSSLDeleters.h" 

#include <iostream>
#include <unistd.h>
#include <stdexcept>
#include <cstring> 
#include <arpa/inet.h> 
#include <sys/socket.h> 

// Constructeur : Initialise avec un socket FD et un pointeur SSL RAW déjà établis
// Ce constructeur est utilisé par le Server::HandleClient après accept et SSL_accept, il prend possession du socket et de l'objet SSL
Client::Client(int socket_fd, SSL* ssl_ptr)
    : clientSocket(socket_fd),
      ctx(nullptr), // Le contexte SSL appartient généralement au serveur, pas au client dans ce cas.
      ssl(ssl_ptr) // Le unique_ptr prend possession du pointeur RAW SSL*.
{
    memset(&serverAddr, 0, sizeof(serverAddr));
    if (ssl_ptr) {
         LOG("[Client] Objet Client créé avec socket et SSL existants. Socket FD: " + std::to_string(clientSocket), "INFO");
        // La possession du SSL* est transféré au unique_ptr 'ssl'.
        // Le socket clientSocket est également géré par cet objet (fermeture dans le destructeur).
    } else {
        // Cas où SSL_accept a échoué juste avant l'appel à ce constructeur
        // Le socket devrait avoir été fermé par AcceptSSLConnection(). Ou une erreur logique si ssl_ptr est null ici.
        LOG("[Client] Objet Client créé avec socket existant mais SSL null. Socket FD: " + std::to_string(clientSocket), "ERROR");
        // Assurer la fermeture si le socket est valide mais SSL null
        if (clientSocket != -1) {
             close(clientSocket);
             clientSocket = -1;
             LOG("[Client] Socket fermé car SSL était null.", "ERROR");
        }
    }
}


// Destructeur : Appelle closeConnection pour libérer les ressources
Client::~Client() {
    closeConnection(); // Le destructeur assure la fermeture et la libération SSL/socket
    LOG("[Client] Destructeur Client appelé. Socket FD final: " + std::to_string(clientSocket), "DEBUG");
}

// Méthode d'initialisation du contexte SSL côté client (utile si ce Client initie la connexion)
UniqueSSLCTX Client::InitClientCTX() {
    UniqueSSLCTX context(SSL_CTX_new(TLS_client_method()));
    if (!context) {
        ERR_print_errors_fp(stderr);
        LOG("[Client] Impossible de créer le contexte SSL client.", "ERROR");
        throw std::runtime_error("Erreur lors de la création du contexte SSL client");
    }
    SSL_CTX_set_min_proto_version(context.get(), TLS1_2_VERSION);

    // Note : Ces chemins sont hardcodés
    if (SSL_CTX_use_certificate_file(context.get(), "../client.crt", SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(context.get(), "../client.key", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        LOG("[Client] Échec du chargement du certificat ou de la clé privée client.", "ERROR");
        throw std::runtime_error("Erreur lors du chargement du certificat ou de la clé privée client");
    }

    if (SSL_CTX_load_verify_locations(context.get(), "../rootCA.crt", nullptr) != 1) {
        ERR_print_errors_fp(stderr);
        LOG("[Client] Échec du chargement des CA de confiance client.", "ERROR");
        throw std::runtime_error("Erreur lors du chargement des CA de confiance client");
    }

    SSL_CTX_set_verify(context.get(), SSL_VERIFY_PEER, nullptr); // Vérifier le certificat du serveur

    LOG("[Client] Contexte SSL client initialisé avec succès.", "DEBUG");
    return context; // Retourne le pointeur unique
}

// Méthode pour connecter SSL côté client (utile si ce Client initie la connexion)
UniqueSSL Client::ConnectSSL(SSL_CTX* ctx_raw, int clientSocket_fd) {
    UniqueSSL ssl_ptr(SSL_new(ctx_raw));
    if (!ssl_ptr) {
        ERR_print_errors_fp(stderr);
        LOG("[Client] Erreur lors de la création de l'objet SSL pour connexion.", "ERROR");
        //close(clientSocket_fd); // Le socket est géré par l'appelant si erreur ici
        return nullptr;
    }
    SSL_set_fd(ssl_ptr.get(), clientSocket_fd);

    int ssl_connect_ret = SSL_connect(ssl_ptr.get());
    if (ssl_connect_ret <= 0) {
        int ssl_err = SSL_get_error(ssl_ptr.get(), ssl_connect_ret);
        LOG("[Client] Échec de la connexion SSL. Code: " + std::to_string(ssl_err), "ERROR");
        ERR_print_errors_fp(stderr);
        // Le unique_ptr libérera ssl_ptr
        // close(clientSocket_fd); // Le socket est géré par l'appelant
        return nullptr;
    }
    LOG("[Client] Connexion SSL établie avec succès.", "INFO");
    return ssl_ptr; // Retourne le pointeur unique
}

// Méthodes d'envoi et de réception via SSL
// Thread-safety : Si un même objet Client peut être appelé par plusieurs threads
int Client::send(const char* data, int size) {
    if (!ssl || clientSocket == -1) {
        LOG("[Client::send] Erreur : Connexion non initialisée.", "ERROR");
        return -1;
    }
    // std::lock_guard<std::mutex> lock(commMutex); // Décommenter si thread-safe nécessaire

    int bytesSent = SSL_write(ssl.get(), data, size);
    if (bytesSent <= 0) {
        int error = SSL_get_error(ssl.get(), bytesSent);
        LOG("[Client::send] Erreur lors de l'envoi SSL. Code: " + std::to_string(error), "ERROR");
        // Détecter les erreurs de connexion et marquer comme non connecté
        if (error == SSL_ERROR_SYSCALL || error == SSL_ERROR_ZERO_RETURN) {
            LOG("[Client::send] Détection de déconnexion ou erreur système.", "INFO");
            closeConnection();
        }
    }
    return bytesSent;
}

int Client::receive(char* buffer, int size) {
    if (!ssl || clientSocket == -1) {
        LOG("[Client::receive] Erreur : Connexion non initialisée.", "ERROR");
        // Assurer le nettoyage si l'état est invalide
         if (clientSocket != -1) { close(clientSocket); clientSocket = -1; }
         ssl.reset();
        return -1;
    }
    // std::lock_guard<std::mutex> lock(commMutex); // Décommenter si thread-safe nécessaire

    int bytesRead = SSL_read(ssl.get(), buffer, size);
    if (bytesRead <= 0) {
        int error = SSL_get_error(ssl.get(), bytesRead);
        if (error == SSL_ERROR_ZERO_RETURN) {
            LOG("[Client::receive] Connexion fermée proprement par le pair.", "INFO");
        } else if (error == SSL_ERROR_SYSCALL) {
             if (errno == 0) { // Connexion fermée sans erreur errno
                 LOG("[Client::receive] Connexion fermée brutalement par le pair (SSL_ERROR_SYSCALL, errno 0).", "INFO");
             } else {
                 LOG("[Client::receive] Erreur système lors de la réception SSL (SSL_ERROR_SYSCALL). errno: " + std::string(strerror(errno)), "ERROR");
             }
        } else if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) {
             // Non bloquant, rien à lire pour l'instant.
             LOG("[Client::receive] SSL_ERROR_WANT_READ/WRITE (socket bloquant?). Code: " + std::to_string(error), "WARNING");
             return 0; // Rien lu
        }
        else {
            LOG("[Client::receive] Erreur lors de la réception SSL non gérée. Code: " + std::to_string(error), "ERROR");
            ERR_print_errors_fp(stderr); // Afficher les détails de l'erreur SSL
        }
        // Dans tous les cas d'erreur ou de déconnexion propre, marquer la connexion comme fermée
        closeConnection();
        // Retourne 0 pour déconnexion propre (SSL_ERROR_ZERO_RETURN), < 0 pour erreur
    }
    // LOG("[Client::receive] Reçu " + std::to_string(bytesRead) + " octets.", "DEBUG");
    return bytesRead;
}

// Méthode utilitaire pour envoyer une string
int Client::send(const std::string& data) {
    return send(data.c_str(), data.size());
}


// Ferme la connexion socket et libère l'objet SSL.
void Client::closeConnection() {
    // Libère l'objet SSL*. Le unique_ptr 'ssl' s'en charge.
    if (ssl) {
        // Tenter une fermeture propre de la connexion SSL
        // En bloquant, 0 signifie que le pair a initié la fermeture, 1 succès, <0 erreur.
        int shutdown_ret = SSL_shutdown(ssl.get());
        if (shutdown_ret == 0) {
            // Le pair a initié la fermeture. On doit terminer le handshake de fermeture.
            std::cerr << "[DEBUG Client] SSL_shutdown étape 1/2. Attente réponse pair..." << std::endl;
        }
        if (shutdown_ret < 0) {
             //std::cerr << "[DEBUG Client] Erreur lors du SSL_shutdown()." << std::endl;
             ERR_print_errors_fp(stderr); // Logguer les erreurs SSL
        }
        // Le unique_ptr 'ssl' libère l'objet SSL* ici.
        ssl.reset(); // Rend le unique_ptr nul après libération
         LOG("[Client] Objet SSL libéré.", "DEBUG");
    }

    // Ferme le socket TCP
    if (clientSocket != -1) {
        std::cerr << "[DEBUG Client] Fermeture du socket FD: " << clientSocket << std::endl;
        if (close(clientSocket) == -1) {
            LOG("[Client] Erreur lors de la fermeture du socket FD: " + std::to_string(clientSocket) + ". Erreur: " + std::string(strerror(errno)), "ERROR");
        } else {
             LOG("[Client] Socket FD: " + std::to_string(clientSocket) + " fermé.", "INFO");
        }
        clientSocket = -1; // Marquer comme fermé
    }

    // Libère le contexte SSL côté client (si ce Client l'a créé)
    // Dans l'architecture serveur, le CTX est géré par le Server.
    if (ctx) {
        ctx.reset(); // Le unique_ptr 'ctx' libère le contexte si existant
         LOG("[Client] Contexte SSL Client libéré.", "DEBUG");
    }

    // LOG("[Client] closeConnection terminé.", "DEBUG");
}

// Vérifie si la connexion est considérée comme active
bool Client::isConnected() const {
    // La connexion est active si le socket est valide et l'objet SSL* existe.
    // Les erreurs de send/receive mettent ssl à nullptr via closeConnection.
    return clientSocket != -1 && ssl != nullptr;
}

// Implémentation de getSocketFD
int Client::getSocketFD() const {
    return clientSocket; // Retourne la valeur du membre socketFD
}