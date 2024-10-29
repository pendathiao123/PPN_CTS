#ifndef CLIENT_H
#define CLIENT_H

#include <string>

class Client {
private:
    int clientSocket;
public:
    Client(const std::string& address, int port);
    void sendRequest(const std::string& request);
    std::string receiveResponse();  // Nouvelle méthode pour recevoir une réponse

    ~Client();  // Déclaration du destructeur
};

#endif  // CLIENT_H
