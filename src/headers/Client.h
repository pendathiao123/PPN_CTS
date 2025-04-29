#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <stdexcept>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>  
#include <unistd.h>     
#include <openssl/ssl.h> 
#include <openssl/err.h> 
#include <memory>       
#include <mutex>     

#include "../headers/OpenSSLDeleters.h" 
#include "../headers/Logger.h"          

class Client {
private:
    int clientSocket = -1;
    UniqueSSLCTX ctx = nullptr; // Utilisé si ce Client *initie* la connexion
    UniqueSSL ssl = nullptr;   // L'objet SSL pour la connexion
    sockaddr_in serverAddr{}; // Utilisé si ce Client *initie* la connexion

    // Méthodes d'initialisation internes (si ce Client initie la connexion)
    UniqueSSLCTX InitClientCTX();
    UniqueSSL ConnectSSL(SSL_CTX* ctx_raw, int clientSocket_fd);

public:

    /*Constructeur utilisé par le Serveur : Initialise avec un socket et SSL déjà établis.
    Le Serveur crée le socket et l'objet SSL via accept() et AcceptSSLConnection(), 
    puis passe les pointeurs bruts au Client qui en prend possession via unique_ptr.*/
    Client(int socket_fd, SSL* ssl_ptr);

    // Destructeur : Assure la libération des ressources
    ~Client();

    // Méthodes pour envoyer et recevoir des données sur la connexion SSL
    // Retourne le nombre d'octets envoyés/reçus, 0 pour déconnexion propre, < 0 pour erreur.
    int send(const char* data, int size);
    int receive(char* buffer, int size);

     // Méthode utilitaire pour envoyer une string
     int send(const std::string& data);
     // Méthode utilitaire pour recevoir dans une string (attention à la taille max)
     // std::string receive(int max_size = 1023); // Peut-être complexe à gérer (blocage, fin de message)

    // Ferme la connexion et libère les ressources associées
    void closeConnection();

    // Vérifie si la connexion est toujours active
    bool isConnected() const;

    /// Méthode pour obtenir le descripteur de fichier socket (utile pour le logging/debug)
    int getSocketFD() const;
};

#endif