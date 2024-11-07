#ifndef CLIENT_H
#define CLIENT_H

#include <string>

class Client {
private:
    int clientSocket;
public:
    Client(const std::string& address, int port);
    void sendRequest(const std::string& request);
    std::string receiveResponse();  
    std::string getBalance(const std::string& currency);
    std::string getMarket();

    void buy(const std::string& currency, double amount);
    void sell(const std::string& currency, double amount);

    ~Client();  // Déclaration du destructeur
};

#endif  // CLIENT_H
