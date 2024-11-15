#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <vector>
#include "Crypto.h"
#include "Server_Transaction.h"


class Server {
private:

    /* Gestion des cryptomonaies */
    std::string configFile; // chemin vers fichier variation des cryptos
    std::vector<Crypto> all_cryptos;  // Vecteur contenant les objets Crypto

    /* Pour la connexion avec sockets */
    const int port; //port de connexion pour liason TCP
    const std::string ipAddress;  // Adresse IP du serveur

    /* Communication avec le Serveur qui s'occupe des transactions */
    Server_Transaction servTr;
    /*  Les clients communiquent seulement avec le serveur principal. Ce-dernier transert
    les requetes au serveur qui s'occupe des transert, qui effectue des lectures écritures ... */

    /* Gestion des comptes (avancé)*/
    //const std::string LOGPATH = "/data/logs/"; // chemin vers le dossier gerant les comptes des clients
    
public:
    // Constructeur
    Server(const std::string& ipAddress, int port, const std::string& configFile, Server_Transaction serv);

    // Fonction pour charger la valeur des cryptomonnaies chaque jour depuis un fichier
    void setCryptos(const std::string& configFile);

    // Getter pour obtenir le vecteur de cryptomonnaies
    const std::vector<Crypto>& getCryptos() const;
    // ...

    void start();
};

#endif  // SERVER_H
