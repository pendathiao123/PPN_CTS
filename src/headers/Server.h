#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <unordered_map> 
#include "Market.h"
#include "Crypto.h"

class Server {
private:
    int port;
    std::string ipAddress;  // Adresse IP du serveur
    std::unordered_map<std::string, double> balances = {
    {"SRD-BTC", 100.0}, 
    {"DOLLARS", 90.0}
    };

public:
    Server(const std::string& ipAddress, int port, const std::string& configFile);
    
    void start();
};

#endif  // SERVER_H
