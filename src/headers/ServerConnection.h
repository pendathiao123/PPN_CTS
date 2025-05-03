#ifndef SERVER_CONNECTION_H
#define SERVER_CONNECTION_H

#include <string>
#include <stdexcept>
#include <sys/socket.h>    
#include <unistd.h>        
#include <openssl/ssl.h>   
#include <openssl/err.h>   
#include <memory>          
#include <mutex>

#include "../headers/OpenSSLDeleters.h" 
#include "../headers/Logger.h"          


// --- Classe ServerConnection ---
// Représente une connexion cliente acceptée par le serveur (socket + objet SSL).
// Gère l'envoi/réception de données sur cette connexion et son cycle de vie.
// Conçue pour être utilisée par ClientSession.
class ServerConnection {
private:
    int clientSocket = -1; // Descripteur de fichier du socket

    // Identifiants après authentification
    std::string clientId;
    std::string token;

    bool m_markedForClose = false; // Flag pour marquer la connexion à fermer

    // Objet SSL (la connexion sécurisée). Géré par unique_ptr pour RAII.
    UniqueSSL ssl = nullptr; // unique_ptr prend possession de SSL*

    // Buffer d'accumulation pour receiveLine
    std::string receive_buffer;

public:
    // --- Constructeur ---
    // Crée un ServerConnection à partir d'un socket FD et d'un objet SSL RAW déjà acceptés/établis.
    // Le ServerConnection prend possession du SSL* (via UniqueSSL).
    ServerConnection(int socket_fd, SSL* ssl_ptr);

    // --- Destructeur ---
    // Ferme la connexion (socket + SSL) et libère les ressources.
    ~ServerConnection();

    // --- Getters ---
    int getSocketFD() const;
    const std::string& getClientId() const; // Valide après auth
    const std::string& getToken() const;    // Valide après auth
    bool isMarkedForClose() const;
    bool isConnected() const; // Vérifie si la connexion est active/utilisable

    // --- Setters ---
    void setClientId(const std::string& id);
    void setToken(const std::string& tok);

    // --- Méthodes de Communication Réseau ---
    // Envoyer des données brutes
    int send(const char* data, int size);
    // Recevoir des données brutes
    int receive(char* buffer, int size);
    // Utilitaire pour envoyer une string
    int send(const std::string& data);

    // --- Méthodes de Gestion de la Connexion ---
    void markForClose();    // Marque la connexion à fermer
    void closeConnection(); // Ferme proprement le socket et SSL

    // Lire un message complet terminé par newline
    // Peut lancer une exception en cas d'erreur (connexion fermée, erreur SSL/socket).
    std::string receiveLine();
};

#endif