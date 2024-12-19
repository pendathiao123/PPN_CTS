#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <openssl/ssl.h>

SSL_CTX* InitClientCTX();
SSL* ConnectSSL(SSL_CTX* ctx, int clientSocket);
void StartClient(const std::string& serverAddress, int port, const std::string& clientId, const std::string& clientToken);

#endif // CLIENT_H
