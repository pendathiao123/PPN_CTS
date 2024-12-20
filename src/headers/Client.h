#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <openssl/ssl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

SSL_CTX* InitClientCTX();
SSL* ConnectSSL(SSL_CTX* ctx, int clientSocket);

class Client {
public : 
    Client();
    ~Client();
    void StartClient(const std::string& serverAddress, int port, const std::string& clientId, const std::string& clientToken);
    void buy(const std::string& currency, double percentage);
    void sell(const std::string& currency, double percentage);

    std::string receiveResponse();
    void sendRequest(const std::string& request);
    void closeConnection();
private : 
    int clientSocket;
    struct sockaddr_in serverAddr;
    SSL_CTX* ctx;
    SSL* ssl;
};

#endif // CLIENT_H
