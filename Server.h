#ifndef SERVER_H
#define SERVER_H

#include <string>
#include "Market.h"

class Server {
private:
    int port;
    std::string ipAddress;  // Adresse IP du serveur

public:
    Server(const std::string& ipAddress, int port, const std::string& configFile);
    void start();
};

#endif  // SERVER_H
