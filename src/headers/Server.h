#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <vector>
#include "Crypto.h"
#include "Transaction.h"

class Server {
private:
    /* Gestion des cryptomonnaies */
    std::vector<Crypto> cryptos;  // Vecteur contenant les objets Crypto

    /* Configuration de la connexion avec les sockets */
    const int port;               // Port pour la liaison TCP
    const std::string ipAddress;  // Adresse IP du serveur
    int clientSocket;

public:
    // Constructeur
    Server(const std::string& ipAddress, int port, const std::string& configFile);

    // Charge les cryptomonnaies à partir d'un fichier de configuration
    void setCryptos(const std::string& configFile);

    // Retourne le vecteur de cryptomonnaies
    const std::vector<Crypto>& getCryptos() const;

    // Démarre le serveur
    void start();
    void Request(int clientSocket);
    void sendResponse(const std::string& response);

    std::string handleBuy(const std::string& request);
    std::string handleSell(const std::string& request);
};

#endif  // SERVER_H
