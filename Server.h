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
    Crypto cryptoInstance;

public:
    Server(const std::string& ipAddress, int port, const std::string& configFile);
    
    std::string handleMarket(const std::string& request);
    std::string handleBuy(const std::string& request);
    std::string handleSell(const std::string& request);
    std::string handleBalance(const std::string& currency);
    
    void start();
    void request(int clientSocket);
};

#endif  // SERVER_H
