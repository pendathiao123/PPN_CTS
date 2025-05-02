// src/headers/ServerConnection.h - En-tête de la classe ServerConnection

#ifndef SERVER_CONNECTION_H
#define SERVER_CONNECTION_H

// Includes standards et système nécessaires
#include <string>
#include <stdexcept>
#include <sys/socket.h>    // socket FD
#include <netinet/in.h>    // sockaddr_in (si membre, ici non)
#include <arpa/inet.h>     // inet_ntoa (si utilisé)
#include <unistd.h>        // close
#include <openssl/ssl.h>   // SSL
#include <openssl/err.h>   // Erreurs OpenSSL
#include <memory>          // unique_ptr
#include <mutex>           // std::mutex (si besoin pour E/S multi-threadées sur le même objet)

// Includes spécifiques au projet
#include "../headers/OpenSSLDeleters.h" // Gestion RAII OpenSSL (pour UniqueSSL)
#include "../headers/Logger.h"          // Logging


// --- Classe ServerConnection : Encapsule une connexion réseau SSL/TLS Côté Serveur ---
/**
 * Représente une connexion cliente acceptée par le serveur (socket + objet SSL).
 * Gère l'envoi/réception de données sur cette connexion et son cycle de vie.
 * Conçue pour être utilisée par ClientSession.
 */
class ServerConnection {
private:
    int clientSocket = -1; // Descripteur de fichier du socket

    // Identifiants après authentification (stockés ici après validation)
    std::string clientId;
    std::string token;

    bool m_markedForClose = false; // Flag pour marquer la connexion à fermer

    // Objet SSL (la connexion sécurisée). Géré par unique_ptr pour RAII.
    UniqueSSL ssl = nullptr; // unique_ptr prend possession de SSL*

    // Buffer d'accumulation pour receiveLine (si receive ne lit pas une ligne complète en un coup)
    std::string receive_buffer;

    // Mutex optionnel pour protéger les E/S (send/receive) si plusieurs threads
    // devaient appeler ces méthodes sur la *même* instance ServerConnection simultanément.
    // Dans l'architecture ClientSession-par-thread, ce n'est pas nécessaire.
    // mutable std::mutex commMutex;


public:
    // --- Constructeur ---
    // Crée un ServerConnection à partir d'un socket FD et d'un objet SSL RAW déjà acceptés/établis par le serveur.
    // Le ServerConnection prend possession du SSL* (via UniqueSSL).
    ServerConnection(int socket_fd, SSL* ssl_ptr);

    // --- Destructeur ---
    // Ferme la connexion (socket + SSL) et libère les ressources proprement.
    ~ServerConnection();

    // --- Getters (accès aux informations de la connexion/client) ---
    int getSocketFD() const;
    const std::string& getClientId() const; // Valide après auth
    const std::string& getToken() const;    // Valide après auth
    bool isMarkedForClose() const;
    bool isConnected() const; // Vérifie si la connexion est active/utilisable

    // --- Setters (pour les identifiants après authentification) ---
    void setClientId(const std::string& id);
    void setToken(const std::string& tok);

    // --- Méthodes de Communication Réseau (Thread-Safe si le SSL* l'est, typiquement oui pour send/receive d'un même socket, mais attention aux appels SIMULTANÉS sur LA MÊME INSTANCE SANS mutex interne) ---
    // Envoyer des données brutes
    int send(const char* data, int size);
    // Recevoir des données brutes
    int receive(char* buffer, int size);
    // Utilitaire pour envoyer une string
    int send(const std::string& data);

    // --- Méthodes de Gestion de la Connexion ---
    void markForClose();    // Marque la connexion à fermer (état interne)
    void closeConnection(); // Ferme proprement le socket et SSL

    // Nouvelle méthode pour lire un message complet terminé par newline
    // Peut lancer une exception en cas d'erreur (connexion fermée, erreur SSL/socket).
    std::string receiveLine();

    // Rendre le mutex interne public ou fournir une méthode pour le locker/unlocker
    // serait nécessaire si ClientSession avait besoin de le gérer (ex: pour sync avec un bot envoyant).
    // Dans le modèle actuel (un thread de session fait toutes les E/S), ce n'est pas requis.
    // Si nécessaire :
    // std::mutex& getCommMutex() { return commMutex; }
};

#endif // SERVER_CONNECTION_H