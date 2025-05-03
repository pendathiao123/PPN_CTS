#include "../headers/ServerConnection.h"
#include "../headers/OpenSSLDeleters.h" 
#include "../headers/Logger.h"          

#include <iostream>    
#include <string>
#include <vector>
#include <sstream>     
#include <iomanip>    
#include <limits>
#include <cmath>      
#include <stdexcept>   
#include <memory>     
#include <mutex>        
#include <cerrno>       
#include <cstring>     
#include <algorithm>   
#include <cctype>       
#include <ctime>        
#include <cstdio>       
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <unistd.h>     
#include <openssl/ssl.h> 
#include <openssl/err.h> 


// --- Constructeur ---
// Crée un ServerConnection pour une connexion acceptée. Prend possession du SSL* via UniqueSSL.
ServerConnection::ServerConnection(int socket_fd, SSL* ssl_ptr)
    // Liste d'initialisation : dans l'ordre de déclaration dans ServerConnection.h
    : clientSocket(socket_fd),
      clientId(), // Valeur par défaut
      token(),    // Valeur par défaut
      m_markedForClose(false),
      // Initialise le unique_ptr 'ssl' avec le pointeur brut. UniqueSSL doit avoir le bon deleter (SSL_free).
      ssl(ssl_ptr)
{
    // Validation : le pointeur SSL ne devrait pas être null si le handshake a réussi.
    if (ssl_ptr) {
         LOG("ServerConnection::ServerConnection INFO : Objet créé avec socket (" + std::to_string(clientSocket) + ") et SSL existants.", "INFO");
    } else {
        // Cas d'erreur : socket accepté mais handshake SSL échoué.
        LOG("ServerConnection::ServerConnection ERROR : Objet créé avec socket valide (" + std::to_string(clientSocket) + ") mais SSL null. Tentative de fermeture du socket.", "ERROR");
        if (clientSocket != -1) {
             ::close(clientSocket); // Utilise la fonction système close() explicitement
             clientSocket = -1; // Marque le socket comme fermé
             LOG("ServerConnection::ServerConnection ERROR : Socket fermé car SSL était null.", "ERROR");
        }
        // Le unique_ptr 'ssl' est déjà nullptr car initialisé avec ssl_ptr qui était null.
    }
    // Les membres string clientId et token sont vides par défaut, définis après authentification.
    // m_markedForClose est false par défaut.
    // commMutex est initialisé par défaut.
}

// --- Destructeur ---
// Assure la fermeture propre de la connexion et la libération des ressources.
ServerConnection::~ServerConnection() {
    closeConnection(); // Appelle la méthode principale de nettoyage.
    // Le unique_ptr 'ssl' sera automatiquement détruit ici, appelant son deleter (SSL_free) si non-null.
    // Le unique_ptr 'ctx' (s'il existait dans cette classe) serait aussi détruit ici.
}

// --- Getters ---
int ServerConnection::getSocketFD() const { return clientSocket; }
const std::string& ServerConnection::getClientId() const { return clientId; }
const std::string& ServerConnection::getToken() const { return token; }
bool ServerConnection::isMarkedForClose() const { return m_markedForClose; }

// Vérifie si la connexion est active et utilisable.
bool ServerConnection::isConnected() const {
    // La connexion est active si le socket est valide, l'objet SSL* existe, ET n'est PAS marquée pour fermeture.
    return clientSocket != -1 && ssl != nullptr && !m_markedForClose;
}


// --- Setters ---
void ServerConnection::setClientId(const std::string& id) { this->clientId = id; }
void ServerConnection::setToken(const std::string& tok) { this->token = tok; }


// --- Méthodes de Communication Réseau ---

// Envoie des données via la connexion SSL.
// Retourne le nombre d'octets envoyés (>0), 0 si SSL_write retourne 0, < 0 en cas d'erreur fatale.
// Gère les erreurs SSL_ERROR_WANT_READ/WRITE en mode bloquant en réessayant.
int ServerConnection::send(const char* data, int size) {
    // Note Thread-Safety : Dans l'architecture ClientSession-par-thread, l'accès est sérialisé par le thread de session.

    if (!isConnected()) { // Vérifie l'état valide de la connexion.
        LOG("ServerConnection::send ERROR : Connexion non valide ou marquée pour fermeture. Socket FD: " + std::to_string(clientSocket), "ERROR");
        return -1; // Indique une erreur.
    }

    int bytesSent = 0;
    int totalBytesSent = 0;
    int remaining = size;
    const char* current_data = data;

    // Boucle pour gérer les écritures partielles et les erreurs WANT_WRITE en mode bloquant.
    while (remaining > 0) {
        bytesSent = SSL_write(ssl.get(), current_data, remaining);

        if (bytesSent > 0) {
            // Succès (partiel ou complet).
            totalBytesSent += bytesSent;
            current_data += bytesSent;
            remaining -= bytesSent;
            // Si remaining > 0, la boucle continue.

        } else if (bytesSent == 0) {
             // Peut indiquer une déconnexion propre (rare sur write).
             int error = SSL_get_error(ssl.get(), bytesSent);
             if (error == SSL_ERROR_ZERO_RETURN) {
                  LOG("ServerConnection::send INFO : SSL_write retourné 0 (SSL_ERROR_ZERO_RETURN).", "INFO");
             } else {
                  LOG("ServerConnection::send WARNING : SSL_write retourné 0. Erreur code: " + std::to_string(error) + ". Socket FD: " + std::to_string(clientSocket), "WARNING");
                  ERR_print_errors_fp(stderr);
             }
             markForClose(); // La connexion n'est plus bonne.
             return -1; // Indique un échec.

        } else { // bytesSent < 0 (erreur)
            int error = SSL_get_error(ssl.get(), bytesSent);

            if (error == SSL_ERROR_WANT_WRITE) {
                 LOG("ServerConnection::send WARNING : SSL_ERROR_WANT_WRITE en mode bloquant. Réessai... Socket FD: " + std::to_string(clientSocket), "WARNING");
                 continue; // Recommence la boucle.
            } else if (error == SSL_ERROR_WANT_READ) {
                 LOG("ServerConnection::send WARNING : SSL_ERROR_WANT_READ sur SSL_write en mode bloquant. Traité comme erreur fatale. Socket FD: " + std::to_string(clientSocket), "WARNING");
                 markForClose();
                 return -1;

            } else if (error == SSL_ERROR_SYSCALL) {
                 LOG("ServerConnection::send ERROR : Erreur système SSL_write (SSL_ERROR_SYSCALL). errno: " + std::string(strerror(errno)) + ". Socket FD: " + std::to_string(clientSocket), "ERROR");
                 ERR_print_errors_fp(stderr);
                 markForClose();
                 return -1;

            } else {
                LOG("ServerConnection::send ERROR : Erreur SSL non gérée lors de l'envoi. Code: " + std::to_string(error) + ". Socket FD: " + std::to_string(clientSocket), "ERROR");
                ERR_print_errors_fp(stderr);
                markForClose();
                return -1;
            }
        }
    }

    return totalBytesSent; // Retourne le nombre total d'octets envoyés.
}

// Reçoit des données via la connexion SSL.
// Retourne le nombre d'octets reçus (>0), 0 pour déconnexion propre (SSL_ERROR_ZERO_RETURN), < 0 pour erreur fatale.
// Gère les erreurs SSL_ERROR_WANT_READ/WRITE en mode bloquant en retournant 0.
int ServerConnection::receive(char* buffer, int size) {
     // Note Thread-Safety : Voir commentaires dans send().

    if (!isConnected()) {
        LOG("ServerConnection::receive ERROR : Connexion non valide ou marquée pour fermeture. Socket FD: " + std::to_string(clientSocket), "ERROR");
        return -1;
    }

    int bytesRead = 0;

    // En mode bloquant, SSL_read devrait bloquer jusqu'à ce qu'il y ait des données OU une erreur.
    // L'appelant (ClientSession::run) est responsable de boucler sur receive() et de gérer la reconstruction des messages complets.
    bytesRead = SSL_read(ssl.get(), buffer, size);

    // --- Gestion du résultat de SSL_read ---
    if (bytesRead > 0) {
        // Lecture réussie.
        return bytesRead; // Retourne le nombre d'octets lus (> 0).

    } else if (bytesRead == 0) {
        // SSL_read retourne 0. Indique généralement une déconnexion propre.
        int error = SSL_get_error(ssl.get(), bytesRead);
        if (error == SSL_ERROR_ZERO_RETURN) {
            LOG("ServerConnection::receive INFO : Connexion fermée proprement par le pair (SSL_ERROR_ZERO_RETURN). Socket FD: " + std::to_string(clientSocket), "INFO");
        } else {
             LOG("ServerConnection::receive WARNING : SSL_read retourné 0. Erreur code: " + std::to_string(error) + ". Socket FD: " + std::to_string(clientSocket), "WARNING");
             ERR_print_errors_fp(stderr);
        }
        markForClose(); // La connexion est terminée.
        return 0; // Retourne 0 pour indiquer la déconnexion propre.

    } else { // bytesRead < 0 (erreur)
        int error = SSL_get_error(ssl.get(), bytesRead);

        if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) {
             // En mode bloquant, cela ne devrait pas arriver ou indique qu'aucune donnée n'est disponible POUR L'INSTANT.
             // Retourne 0 pour signaler à l'appelant qu'il faut réessayer (ou qu'il est bloqué en attendant).
             return 0; // Rien lu pour l'instant.

        } else if (error == SSL_ERROR_SYSCALL) {
             LOG("ServerConnection::receive ERROR : Erreur système SSL_read (SSL_ERROR_SYSCALL). errno: " + std::string(strerror(errno)) + ". Socket FD: " + std::to_string(clientSocket), "ERROR");
             ERR_print_errors_fp(stderr);
             markForClose();
             return -1;

        }
        else {
            LOG("ServerConnection::receive ERROR : Erreur SSL non gérée lors de la réception. Code: " + std::to_string(error) + ". Socket FD: " + std::to_string(clientSocket), "ERROR");
            ERR_print_errors_fp(stderr);
            markForClose();
            return -1;
        }
    }
}

// Méthode utilitaire pour envoyer une string.
int ServerConnection::send(const std::string& data) {
    if (data.empty()) {
        return 0;
    }
    // Appelle la version send(const char*, int).
    return send(data.c_str(), data.size());
}


// --- Méthodes de Gestion de la Connexion ---

// Marque la connexion pour fermeture interne.
void ServerConnection::markForClose() {
    // Un simple bool suffit si l'accès est mono-threadé par ClientSession.
    this->m_markedForClose = true;
}

// Ferme la connexion (socket et SSL) et libère les ressources.
void ServerConnection::closeConnection() {
    if (clientSocket == -1) {
        return;
    }

    LOG("ServerConnection::closeConnection INFO : Fermeture de la connexion pour socket FD: " + std::to_string(clientSocket), "INFO");

    // Tenter une fermeture propre de la connexion SSL.
    // SSL_shutdown envoie "close_notify".
    if (ssl) { // Vérifie si l'objet SSL est valide
        int shutdown_ret = SSL_shutdown(ssl.get());
        if (shutdown_ret == 0) {
             // Étape 1/2 terminée.
             // Optionnel : tenter le second appel, mais attention au blocage infini en mode bloquant.
             // shutdown_ret = SSL_shutdown(ssl.get());
        }
        if (shutdown_ret < 0) {
             LOG("ServerConnection::closeConnection ERROR : Erreur lors du SSL_shutdown(). Socket FD: " + std::to_string(clientSocket), "ERROR");
             ERR_print_errors_fp(stderr);
        }
        ssl.reset(); // Libère explicitement l'objet SSL (appelle deleter).
    }

    // Ferme le socket TCP sous-jacent.
    if (clientSocket != -1) {
        if (::close(clientSocket) == -1) {
            LOG("ServerConnection::closeConnection ERROR : Erreur lors de la fermeture du socket FD: " + std::to_string(clientSocket) + ". Erreur système: " + std::string(strerror(errno)), "ERROR");
        } else {
             LOG("ServerConnection::closeConnection INFO : Socket FD: " + std::to_string(clientSocket) + " fermé.", "INFO");
        }
        clientSocket = -1; // Marque le socket comme fermé.
    }

    // Marque la connexion comme fermée.
    m_markedForClose = true;
}

// --- Implémentation de receiveLine ---
// Lit depuis la connexion jusqu'à trouver un newline ('\n'). Accumule les données si nécessaire.
// Peut lancer une exception en cas d'erreur (connexion fermée, erreur SSL/socket).
std::string ServerConnection::receiveLine() {
    char buffer[1024]; // Buffer temporaire pour les appels à receive
    size_t newline_pos;

    // Boucle tant qu'un newline n'est pas trouvé dans le buffer d'accumulation
    while ((newline_pos = receive_buffer.find('\n')) == std::string::npos) {
        // Le newline n'est pas dans le buffer d'accumulation. Lire plus de données.
        int bytes_read = 0;
        // Utilise la méthode receive existante pour lire depuis le socket SSL.
        try {
             // Lire dans le buffer temporaire.
             bytes_read = receive(buffer, sizeof(buffer) - 1); // Appelle this->receive(char*, int)

        } catch (const std::exception& e) {
             // Si la méthode receive (this->receive) lance une exception (ex: erreur SSL), la propager.
             LOG("ServerConnection ERROR : Exception propagée de receive() dans receiveLine() pour socket FD: " + std::to_string(clientSocket) + ". Exception: " + e.what(), "ERROR");
             closeConnection(); // S'assurer que l'état interne de la connexion est marqué comme fermée.
             throw; // Re-lance l'exception pour être gérée plus haut dans la pile (ex: dans main()).
        }


        if (bytes_read > 0) {
            // Des données ont été lues. Ajouter au buffer d'accumulation.
            buffer[bytes_read] = '\0'; // Null-terminer le buffer temporaire
            receive_buffer.append(buffer, bytes_read); // Ajouter les octets lus au buffer d'accumulation

             // Re-vérifier si le newline est maintenant dans le buffer d'accumulation après l'ajout.
             newline_pos = receive_buffer.find('\n');

        } else if (bytes_read == 0) {
            // receive retourne 0 lorsque le pair ferme la connexion proprement.
            // Si le buffer d'accumulation n'est pas vide, cela signifie que le pair s'est déconnecté au milieu d'une ligne.
            // Selon le protocole, cela peut être considéré comme une erreur.
            LOG("ServerConnection INFO : Connexion fermée par le pair pendant receiveLine() pour socket FD: " + std::to_string(clientSocket) + ". Buffer accumulation size: " + std::to_string(receive_buffer.size()) + ".", "INFO");
            closeConnection(); // Marquer la connexion comme fermée.
             if (!receive_buffer.empty()) {
                  LOG("ServerConnection WARNING : Déconnexion pair avec données partielles dans buffer: '" + receive_buffer + "'", "WARNING");
             }
            throw std::runtime_error("Connection closed by peer while reading a line."); // Lancer une exception.

        } else { // bytes_read < 0
            // receive retourne une valeur négative pour des erreurs fatales non gérées par receive en interne.
             unsigned long ssl_err = ERR_get_error(); // Obtenir l'erreur SSL si disponible
             std::string err_msg = "Unknown error";
             if (ssl_err != 0) {
                 char err_buf[256];
                 ERR_error_string_n(ssl_err, err_buf, sizeof(err_buf));
                 err_msg = "SSL error: " + std::string(err_buf);
             } else if (errno != 0) { // Si pas d'erreur SSL, vérifier l'erreur système (errno)
                  err_msg = "System error: " + std::string(strerror(errno));
             } else {
                  err_msg = "Unknown error code from receive: " + std::to_string(bytes_read);
             }

             LOG("ServerConnection ERROR : Erreur fatale (" + std::to_string(bytes_read) + ") pendant receiveLine() pour socket FD: " + std::to_string(clientSocket) + ". Erreur: " + err_msg, "ERROR");
             closeConnection(); // Marquer la connexion comme fermée.
             throw std::runtime_error("Fatal error during receiveLine(): " + err_msg); // Lancer une exception.

        }

        // Si newline_pos est toujours std::string::npos, la boucle while continue pour lire plus de données.
    }

    // Un newline a été trouvé dans receive_buffer.
    // Extraire la ligne complète (sans le newline final).
    std::string complete_line = receive_buffer.substr(0, newline_pos);

    // Retirer la ligne extraite (et le newline) du buffer d'accumulation
    // pour les lectures futures.
    receive_buffer.erase(0, newline_pos + 1); // +1 pour retirer le '\n'

    return complete_line; // Retourne la ligne complète lue (sans le '\n').
}