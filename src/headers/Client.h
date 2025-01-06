#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <memory>
#include <openssl/ssl.h>
#include <unordered_map>
#include <netinet/in.h>

class Bot;

class Client
{
public:
    Client();
    ~Client();
    void StartClient(const std::string &serverAddress, int port, const std::string &clientId, const std::string &clientToken);
    void buy(const std::string &currency, double percentage);
    void sell(const std::string &currency, double percentage);
    void closeConnection();
    bool isConnected() const;

private:
    int clientSocket;
    SSL_CTX *ctx;
    SSL *ssl;
    struct sockaddr_in serverAddr;
    std::shared_ptr<Bot> tradingBot;
    SSL_CTX *InitClientCTX();
    SSL *ConnectSSL(SSL_CTX *ctx, int clientSocket);
    void sendRequest(const std::string &request);
    std::string receiveResponse();
};

#endif // CLIENT_H