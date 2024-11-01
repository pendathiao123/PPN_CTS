#ifndef SERVER_H
#define SERVER_H

#include <string>
#include "Crypto.h"

class Server {
private:
    int port;
    std::string ipAddress;  // Adresse IP du serveur
    std::vector<Crypto> cryptos;  // Vecteur contenant les objets Crypto
    std::string configFile; // chemin vers  fichier variation des cryptos
    // ...
    /*
    std::string logPath; // chemin vers le dossier gerant les comptes des clients
    */

public:
    // Constructeur
    Server(const std::string& ipAddress, int port, const std::string& configFile);
    // Fonction pour charger les cryptomonnaies depuis un fichier
    void setCryptos(const std::string& configFile);  
    // Getter pour obtenir le vecteur de cryptomonnaies
    const std::vector<Crypto>& getCryptos() const;
    // Méthode pour enregistrer la transaction dans le fichier CSV (arguments à rajouter ?)
    static void logTransactionToCSV(const std::string& filename);
    // ...

    void start();
};

#endif  // SERVER_H
