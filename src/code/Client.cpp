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

Client::Client(int id) : clientSocket(-1), ctx(nullptr), ssl(nullptr), tradingBot(nullptr), ID(id)
{
    memset(&serverAddr, 0, sizeof(serverAddr));
}

Client::~Client()
{
    closeConnection();
}

bool Client::isConnected() const
{
    return ssl != nullptr;
}

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

// affichages dans le terminal
void Client::affiche(std::string msg){
    std::cout << "Client " << ID << ": " << msg << std::endl;
}

// affichage des erreurs dans le terminal
void Client::afficheErr(std::string err){
    std::cerr << "! Client " << ID << ": " << err << std::endl;
}

int Client::sendRequest(const std::string &request)
{
    // si le client n'est pas connecté
    if (!isConnected())
    {
        afficheErr("Erreur : SSL non initialisé");
        return 1; // permier cas d'erreur
    }

    affiche("Requête envoyée : " + request);
    int bytesSent = SSL_write(ssl, request.c_str(), request.length());
    if (bytesSent <= 0)
    {
        afficheErr("Erreur lors de l'envoi de la requête SSL");
        ERR_print_errors_fp(stderr);
        return 2; // deuxième cas d'erreur
    }
    else
    {
        affiche("Nombre d'octets envoyés : " + std::to_string(bytesSent));
    }
    return 0;
}

std::string Client::receiveResponse()
{
    if (!isConnected())
    {
        // Le SSl du client n'a pas été initialisé
        afficheErr("Erreur : SSL non initialisé");
        return "";
    }

    char buffer[1024];
    int bytesRead = SSL_read(ssl, buffer, sizeof(buffer) - 1);

    if (bytesRead <= 0)
    {
        afficheErr("Erreur : Réception SSL échouée");
        ERR_print_errors_fp(stderr);
        return "";
    }

    buffer[bytesRead] = '\0'; // on ajoute le caractère de fin de chaîne
    std::string response(buffer); // on passe d'un tableau de char à un std::string
    affiche("Réponse reçue : " + response);
    return response;
}

void Client::StartClient(const std::string &serverAddress, int port)
{
    // création du socket pour le client
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

    // initialisation du certificat pour la connexion SSL/TLS
    this->ctx = InitClientCTX();
    this->ssl = ConnectSSL(ctx, this->clientSocket);
    if (!ssl)
    {
        close(clientSocket);
        SSL_CTX_free(ctx);
        afficheErr("Échec de la connexion SSL");
        exit(EXIT_FAILURE);
    }

    std::string message, reponse;
    std::string new_acc = "NEW_ACCOUNT:";
    std::string dend = "DENIED";
    std::string conn = "CONNECTED";
    std::string id = std::to_string(ID);

    // Nouvelle connexion:
    if (TOKEN.empty()){ // Ici on demande alors une nouvelle connexion
        message = "ID:" + id + ",FIRST_CONNECTION";
        //message.append(",FIRST_CONNECTION");
        // Envoyer le message au serveur
        if(sendRequest(message) != 0){
            // Erreur au niveau de l'envoie
            afficheErr("Erreur lors de l'envoi du message de création de compte");
            ERR_print_errors_fp(stderr);
            closeConnection();
            exit(EXIT_FAILURE);
        }
        // Lire la réponse du serveur
        reponse = receiveResponse();
        // si la reponse est vide, il y a eu une erreur dans la reception
        if(reponse == ""){
            afficheErr("Erreur lors de la réception de la réponse du serveur");
            closeConnection();
            exit(EXIT_FAILURE); // ---------------------------------------------> Changer prototype de StartClient()
        }
        // Si le message contient NEW_ACCOUNT et ne contient pas DENIED
        if((reponse.find(new_acc) != std::string::npos) && (reponse.find(dend) == std::string::npos)){
            // on extrait l'id et le token
            size_t id_start = reponse.find(":") + 1;
            size_t token_start = reponse.find(",");
            // on vérifie que l'ID est bien le nôtre:
            if(stoi(reponse.substr(id_start,token_start - id_start)) != ID){
                // ATTENTION
                affiche("L'identifiant en reponse ne correspond pas au mien !");
            }
            token_start++;
            TOKEN = reponse.substr(token_start); // TOKEN extrait
        }else{ // La reponse ne correspond pas à l'acceptation de creation d'un nouveau compte
            affiche("Refus de création d'un nouveau compte");
            closeConnection();
            return;
        }
    }

    // Connexion normale:
    message = "ID:" + id + ",TOKEN:" + TOKEN;
    // Envoyer le message au serveur
    if(sendRequest(message) != 0){
        // Erreur au niveau de l'envoie
        afficheErr("Erreur lors de l'envoi du message d'authentification");
        ERR_print_errors_fp(stderr);
        closeConnection();
        exit(EXIT_FAILURE);
    }
    reponse = receiveResponse();
    // si la reponse est vide, il y a eu une erreur dans la reception
    if(reponse == ""){
        afficheErr("Erreur lors de la réception de la réponse du serveur");
        closeConnection();
        exit(EXIT_FAILURE);
    }
    // Si le message contient pas CONNECTED alors 
    if(reponse.find(conn) == std::string::npos){
        affiche("Demande d'authentification refusé");
        closeConnection();
        return;
    }
    // Si aucun cas d'erreur est detecté on sort de la fonction et le client est connecté
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
        afficheErr("Erreur : Solde en dollars insuffisant pour acheter " + std::to_string(percentage) + "% de " + currency);
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
    affiche("Réponse à l'achat : " + response);
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
        afficheErr("Erreur : Solde insuffisant pour vendre " + std::to_string(quantite) + " de " + currency);
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
    affiche("Réponse à la vente : " + response);
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

void Client::EndClient(){
    
    std::string message, reponse;
    std::string discon = "DISCONNECTED";
    std::string id = std::to_string(ID);

    message = "ID:" + id + ",DISCONNECT";
    //message = message + ;
    // Envoyer le message au serveur
    if(sendRequest(message) != 0){
        // Erreur au niveau de l'envoie
        afficheErr("Erreur lors de l'envoi du message de deconnexion");
        ERR_print_errors_fp(stderr);
        closeConnection();
        exit(EXIT_FAILURE);
    }
    reponse = receiveResponse();
    // si la reponse est vide, il y a eu une erreur dans la reception
    if(reponse == ""){
        afficheErr("Erreur lors de la réception de la réponse du serveur");
        closeConnection();
        exit(EXIT_FAILURE);
    }
    // Si le message contient DISCONNECTED c'est bon
    if(reponse.find(discon) != std::string::npos){
        affiche("Deconecté");
    }else{
        affiche("Demande de deconnexion refusé. Possibles erreurs à la prochaîne réconnexion");
    }
    // Pour l'instant quoi qu'il arrive on sort de la fonction.
}