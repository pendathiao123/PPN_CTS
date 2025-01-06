#include "../headers/Client.h"
#include "../headers/Bot.h"
#include "../headers/Crypto.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

Client::Client() : clientSocket(-1), ctx(nullptr), ssl(nullptr), tradingBot(nullptr)
{
    memset(&serverAddr, 0, sizeof(serverAddr));
}

Client::~Client()
{
    closeConnection();
}

// Initialiser le contexte SSL pour le client
SSL_CTX *Client::InitClientCTX()
{
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());

    // Charger le certificat public et la clé privée du client
    if (!SSL_CTX_use_certificate_file(ctx, "../server.crt", SSL_FILETYPE_PEM) ||
        !SSL_CTX_use_PrivateKey_file(ctx, "../server.key", SSL_FILETYPE_PEM))
    {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}

// Établir une connexion SSL
SSL *Client::ConnectSSL(SSL_CTX *ctx, int clientSocket)
{
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, clientSocket);
    if (SSL_connect(ssl) <= 0)
    {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        return nullptr;
    }
    return ssl;
}

// Fonction principale pour démarrer le client
void Client::StartClient(const std::string &serverAddress, int port, const std::string &clientId, const std::string &clientToken)
{
    this->clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1)
    {
        perror("Échec de la création de la socket");
        exit(EXIT_FAILURE);
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, serverAddress.c_str(), &serverAddr.sin_addr) <= 0)
    {
        perror("Adresse invalide");
        close(clientSocket);
        exit(EXIT_FAILURE);
    }

    if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("Échec de la connexion");
        close(clientSocket);
        exit(EXIT_FAILURE);
    }

    this->ctx = InitClientCTX();
    this->ssl = ConnectSSL(ctx, this->clientSocket);
    if (!ssl)
    {
        close(clientSocket);
        SSL_CTX_free(ctx);
        std::cerr << "Échec de la connexion SSL" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Construire le message à envoyer
    std::string message;
    if (clientId.empty() || clientToken.empty())
    {
        message = "FIRST_CONNECTION";
    }
    else
    {
        message = "ID:" + clientId + ",TOKEN:" + clientToken;
    }

    // Envoyer le message au serveur
    int bytesSent = SSL_write(ssl, message.c_str(), message.size());
    if (bytesSent <= 0)
    {
        std::cerr << "Erreur lors de l'envoi du message d'authentification" << std::endl;
        ERR_print_errors_fp(stderr);
        closeConnection();
        exit(EXIT_FAILURE);
    }

    // Lire la réponse du serveur
    char buffer[1024] = {0};
    int bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1);
    if (bytes > 0)
    {
        buffer[bytes] = '\0';
        std::cout << "Réponse du serveur : " << buffer << std::endl;
    }
    else
    {
        std::cerr << "Erreur lors de la réception de la réponse du serveur" << std::endl;
        ERR_print_errors_fp(stderr);
        closeConnection();
        exit(EXIT_FAILURE);
    }
}

void Client::sendRequest(const std::string &request)
{
    if (!ssl)
    {
        std::cerr << "Erreur : SSL non initialisé" << std::endl;
        return;
    }

    std::cout << "Requête envoyée : " << request << std::endl;
    int bytesSent = SSL_write(ssl, request.c_str(), request.length());
    if (bytesSent <= 0)
    {
        std::cerr << "Erreur lors de l'envoi de la requête SSL" << std::endl;
        ERR_print_errors_fp(stderr);
    }
    else
    {
        std::cout << "Nombre d'octets envoyés : " << bytesSent << std::endl;
    }
}

std::string Client::receiveResponse()
{
    if (!ssl)
    {
        std::cerr << "Erreur : SSL non initialisé" << std::endl;
        return "";
    }

    char buffer[1024];
    int bytesRead = SSL_read(ssl, buffer, sizeof(buffer) - 1);

    if (bytesRead <= 0)
    {
        std::cerr << "Erreur : Réception SSL échouée" << std::endl;
        ERR_print_errors_fp(stderr);
        return "";
    }

    buffer[bytesRead] = '\0';
    std::string response(buffer);
    std::cout << "Réponse reçue : [" << response << "]" << std::endl;
    return response;
}

void Client::buy(const std::string &currency, double percentage)
{
    if (!tradingBot)
    {
        tradingBot = std::make_shared<Bot>(currency);
    }

    double solde_dollars = tradingBot->getBalance("DOLLARS");
    double val1 = solde_dollars * (percentage / 100.0);
    std::unordered_map<std::string, double> bot_balance = tradingBot->get_total_Balance();

    if (bot_balance["DOLLARS"] < val1)
    {
        std::cerr << "Erreur : Solde en dollars insuffisant pour acheter " << percentage << "% de " << currency << std::endl;
        return;
    }

    bot_balance["DOLLARS"] -= val1;
    Crypto crypto;
    double val2 = crypto.getPrice(currency);
    double quantite = val1 / val2;

    bot_balance[currency] += quantite;
    tradingBot->updateBalance(bot_balance);

    std::string request = "BUY " + currency + " " + std::to_string(percentage);
    sendRequest(request);
    std::string response = receiveResponse();
    std::cout << "Réponse à l'achat : " << response << std::endl;
}

void Client::sell(const std::string &currency, double percentage)
{
    if (!tradingBot)
    {
        tradingBot = std::make_shared<Bot>(currency);
    }

    double solde_crypto = tradingBot->getBalance(currency);
    double quantite = solde_crypto * (percentage / 100.0);
    std::unordered_map<std::string, double> bot_balance = tradingBot->get_total_Balance();

    if (bot_balance[currency] < quantite)
    {
        std::cerr << "Erreur : Solde insuffisant pour vendre " << quantite << " de " << currency << std::endl;
        return;
    }

    bot_balance[currency] -= quantite;
    Crypto crypto;
    double val2 = quantite * crypto.getPrice(currency);

    bot_balance["DOLLARS"] += val2;
    tradingBot->updateBalance(bot_balance);

    std::string request = "SELL " + currency + " " + std::to_string(percentage);
    sendRequest(request);
    std::string response = receiveResponse();
    std::cout << "Réponse à la vente : " << response << std::endl;
}

void Client::closeConnection()
{
    if (ssl)
    {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = nullptr;
    }
    if (clientSocket != -1)
    {
        close(clientSocket);
        clientSocket = -1;
    }
    if (ctx)
    {
        SSL_CTX_free(ctx);
        ctx = nullptr;
    }
}

bool Client::isConnected() const
{
    return ssl != nullptr;
}