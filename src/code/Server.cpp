#include "../headers/Server.h"
#include "../headers/Bot.h"
#include "../headers/Client.h"
#include "../headers/Transaction.h"
#include "../headers/Global.h"
#include "../headers/SRD_BTC.h"
#include "../headers/Crypto.h"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <openssl/rand.h>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <unordered_map>
#include <ctime>
#include <chrono>
#include <thread>
#include <filesystem>


Server::Server(int prt, const std::string &uFile, const std::string &lFile) : 
    PORT(prt), usersFile(uFile), logFile(lFile) {}

// Fonction pour générer une chaîne de caractères aléatoire
std::string GenerateRandomString(size_t length)
{
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string result;
    unsigned char rand_bytes[length];
    if (RAND_bytes(rand_bytes, length) != 1)
    {
        std::cerr << "Erreur de génération aléatoire" << std::endl;
        return "";
    }

    for (size_t i = 0; i < length; ++i)
    {
        result += charset[rand_bytes[i] % (sizeof(charset) - 1)];
    }

    return result;
}

// Fonction pour générer un ID aléatoire à 4 chiffres
std::string GenerateRandomId()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);

    int id = dis(gen);
    return std::to_string(id);
}

// Fonction pour générer un jeton avec HMAC
std::string GenerateToken()
{
    std::string key = GenerateRandomString(32);
    std::string message = GenerateRandomString(16);

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;

    HMAC(EVP_sha256(), key.c_str(), key.size(),
         reinterpret_cast<const unsigned char *>(message.c_str()), message.size(),
         hash, &hash_len);

    std::ostringstream oss;
    for (unsigned int i = 0; i < hash_len; ++i)
    {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }

    return oss.str();
}

// Charger les utilisateurs à partir d'un fichier
std::unordered_map<std::string, std::string> LoadUsers(const std::string &filename)
{
    std::unordered_map<std::string, std::string> users;
    std::ifstream file(filename);
    std::string id, token;
    while (file >> id >> token)
    {
        users[id] = token;
    }
    return users;
}

// Sauvegarder les utilisateurs dans un fichier
void SaveUsers(const std::string &filename, const std::unordered_map<std::string, std::string> &users)
{
    std::ofstream file(filename, std::ios::trunc);
    for (const auto &[id, token] : users)
    {
        file << id << " " << token << "\n";
    }
}

// Initialiser le contexte SSL pour le serveur
SSL_CTX *InitServerCTX(const std::string &certFile, const std::string &keyFile)
{
    SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx)
    {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_certificate_file(ctx, certFile.c_str(), SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, keyFile.c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        exit(EXIT_FAILURE);
    }

    return ctx;
}

// Accepter une connexion SSL
SSL *AcceptSSLConnection(SSL_CTX *ctx, int clientSocket)
{
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, clientSocket);
    if (SSL_accept(ssl) <= 0)
    {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(clientSocket);
        return nullptr;
    }
    return ssl;
}

// Gestion des affichages dans le terminal
void Server::affiche(std::string msg){
    std::cout << "Serveur: " << msg << std::endl;
}

// Gestion des affichage d'erreurs dans le terminal
void Server::afficheErr(std::string err){
    std::cerr << "! Serveur: " << err << std::endl;
}

// Fonction pour gérer la reception de requêtes
std::string Server::receiveRequest(SSL *ssl){
    char buffer[1024] = {0};
    int bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1);
    if (bytes <= 0){ // en cas d'erreur
        afficheErr("Erreur lors de la reception de la requête");
        ERR_print_errors_fp(stderr);
        return ""; // on retourne une chaîne vide
    }
    buffer[bytes] = '\0'; // on rajoute le symbole de fin de chaîne
    std::string request(buffer); // on passe d'un tableau de char à un std::string   
    return request;
}

// Fonction pour gérer l'envoie de reponses
int Server::sendResponse(SSL *ssl, const std::string &response){
    int bytesSent = SSL_write(ssl, response.c_str(), response.length());
    if (bytesSent <= 0){ // en cas d'erreur
        afficheErr("Erreur lors de l'envoi de la réponse");
        ERR_print_errors_fp(stderr);
        return 1;
    }
    return 0;
}

// Créer un nouveau compte client:
std::string Server::newConnection(const std::string idClient){
    /** Il s'agit d'une nouvelle connexion
     * On va alors generer un token asocié à l'id que nous a envoyé le client */
    
    //id = GenerateRandomId(); // pas besoin !
    affiche("Demande de creation de compte pour l'ID: " + idClient);
    std::string new_accnt = "NEW_ACCOUNT:";

    // On vérifie que l'id ne se trouve pas déjà dans la base des données !
    if(users.find(idClient) == users.end()){
        // generation d'un token
        std::string token = GenerateToken();
        affiche("Token generé: " + token);

        // asociation de l'identifiant du client au token generé
        users[idClient] = token;
        // Enregistrement du nouveau compte client dans la base des données
        SaveUsers(usersFile, users);
        affiche("Nouvel ID: " + idClient + ", Nouveau Token: " + token);

        // On retourne l'information au client sur son nouvel identifiant
        return new_accnt + idClient + "," + token;
    }else{
        // On va refuser cette demande de creatin de compte, car on est sur un cas d'usurpation de compte
        return new_accnt + "DENIED";
    }
}

// Gérer les connexions des clients
int Server::Connection(SSL *ssl, const std::string idClient, std::string msgClient){

    std::string response, token;
    std::string frst = ",FIRST_CONNECTION";
    std::string prefix_token = "TOKEN:";
    int ex;

    // Demande de creation de compte
    if(msgClient.find(frst) != std::string::npos){

        response = newConnection(idClient);
        // envoie reponse au Client
        if(sendResponse(ssl,response) != 0){
            // Erreur au niveau de l'envoie de la reponse
            afficheErr("Erreur lors de l'envoi de la reponse au Client");
            ERR_print_errors_fp(stderr);
            return 0; //exit(EXIT_FAILURE);
        }

        // reception reponse du Client
        msgClient = receiveRequest(ssl);
        if(msgClient.empty()){ // ici il y a eu une erreur au niveau de la lecture du msg
            afficheErr("Erreur lors de la reception du message (vide).");
            return 0;
        }
    }

    // Demande de connexion
    if(msgClient.find(prefix_token) != std::string::npos){

        // extraction du token envoyé par le client
        size_t token_start = msgClient.find(prefix_token) + prefix_token.length();
        token = msgClient.substr(token_start);
        affiche("ID: " + idClient + " a envoyé comme Token: " + token);

        // verification des données envoyés par rapport à la base des données
        auto it = users.find(idClient);
        if (it != users.end() && it->second == token)
        {
            // les information concordent
            response = "CONNECTED";
            ex = 1;
        }
        else
        {
            // mauvais token ou identifiant
            response = "CONNECTION_DENIED";
            ex = 0;
        }

        // envoie reponse au Client
        if(sendResponse(ssl,response) != 0){
            // Erreur au niveau de l'envoie de la reponse
            afficheErr("Erreur lors de l'envoi de la reponse au Client");
            ERR_print_errors_fp(stderr);
            return 0; //exit(EXIT_FAILURE);
        }
    }
    return ex;
}

// Gérer les deconnxions des clients
std::string Server::DeConnection(const std::string idClient){
    /** Pour l'instant toute demande de deconnexion est accepté sans aucune vérification.
     * Si on a le temps on pourra faire qqch.
     * Mais ceci a bcp plus de sens dans un systéme avec plusieurs clients ... !
     */
    return "DISCONNECTED";
}

// Gérer la commande d'achat
std::string Server::handleBuy(const std::string &request, const std::string &clientId)
{
    // Extraire la paire de crypto et le pourcentage de la requête
    std::istringstream iss(request);
    std::string action, currency;
    double percentage;
    if (!(iss >> action >> currency >> percentage))
    {
        afficheErr("Erreur: Format de commande invalide");
        return "Erreur: Format de commande invalide\n";
    }
    if (percentage <= 0 || percentage > 100)
    {
        afficheErr("Erreur: Pourcentage invalide");
        return "Erreur: Pourcentage invalide\n";
    }
    affiche("Achat de " + std::to_string(percentage) + "% de " + currency + " réussi");

    // Créer une transaction et l'enregistrer dans le fichier
    Transaction transaction(clientId, "buy", currency, percentage, 30000); // Exemple de prix unitaire, à modifier si nécessaire
    affiche("Création de la transaction: " + transaction.getId());
    transaction.logTransactionToCSV(logFile);

    return "Achat de " + std::to_string(percentage) + "% de " + currency + " réussi\n";
}

// Gérer la commande de vente
std::string Server::handleSell(const std::string &request, const std::string &clientId)
{
    // Extraire la paire de crypto et le pourcentage de la requête
    std::istringstream iss(request);
    std::string action, currency;
    double percentage;
    if (!(iss >> action >> currency >> percentage))
    {
        afficheErr("Erreur: Format de commande invalide");
        return "Erreur: Format de commande invalide\n";
    }
    if (percentage <= 0 || percentage > 100)
    {
        afficheErr("Erreur: Pourcentage invalide");
        return "Erreur: Pourcentage invalide\n";
    }
    affiche("Vente de " + std::to_string(percentage) + "% de " + currency + " réussie");

    // Créer une transaction et l'enregistrer dans le fichier
    Transaction transaction(clientId, "sell", currency, percentage, 30000); // Exemple de prix unitaire, à modifier si nécessaire
    affiche("Création de la transaction: " + transaction.getId());
    transaction.logTransactionToCSV(logFile);

    return "Vente de " + std::to_string(percentage) + "% de " + currency + " réussie\n";
}

// Gérer une connexion client
void Server::HandleClient(SSL *ssl){

    /* Dans cette fonction toute erreur d'envoi ou de reception de message entraine une sortie precipité de la fonction. 
    (Autre option: avant de se sortir de la fonction, envoyer un msg d'erreur au client, pour qu'il ne reste pas bloqué) */

    // Lecture de la requête envoyé par un Client
    std::string receivedMessage = receiveRequest(ssl);
    if(receivedMessage.empty()){ // ici il y a eu une erreur au niveau de la lecture du msg
        afficheErr("Erreur lors de la reception du message (vide).");
        return;
    }
    affiche("Le Serveur à Reçu: " + receivedMessage);

    // Extraction de l'ID du Client:
    std::string prefix_id = "ID:";
    if(receivedMessage.find(prefix_id) != std::string::npos){
        /* On regarde ici si le message contient "ID:", mais il faudrait aussi vérifier qu'il est au début du message */
        size_t id_start = receivedMessage.find(prefix_id) + prefix_id.length();
        size_t id_end = receivedMessage.find(",", id_start);
        std::string id = receivedMessage.substr(id_start, id_end - id_start);
        /* Ici on devrait normalement vérifier que id est un nombre (entier) !
        */
        
        // Déclaration des varaibles:
        std::string response;
        std::string deco = ",DISCONNECT";
        std::string achat = "BUY";
        std::string vente = "SELL";

        // Authentification du Client
        if(Connection(ssl,id,receivedMessage)){ // Si le Client arrive à se connecter

            // lecture prochaine requête du Client
            receivedMessage = receiveRequest(ssl);
            if(receivedMessage.empty()){ // erreur lecture msg
                afficheErr("Erreur lors de la reception du message (vide).");
                return;
            }

            // Tant que on ne reçoit pas de demande de deconnexion
            while(receivedMessage.find(deco) == std::string::npos){
                if(receivedMessage.find(achat) != std::string::npos){ // Requête d'achat
                    // Gérer la commande d'achat
                    response = handleBuy(receivedMessage, id);
                }else if(receivedMessage.find(vente) != std::string::npos){ // Requête de vente
                    // Gérer la commande de vente
                    response = handleSell(receivedMessage, id);
                }else{ // Le Client n'a pas formulé un demande explicite
                    /* Si on veut faire adopter au Serveur un comportement restrictif,
                    ici on peut faire qqch */
                    response = "Commande inconnue\n";
                }

                // envoie de la reponse au Client
                if(sendResponse(ssl,response) != 0){
                    // Erreur au niveau de l'envoie de la reponse
                    afficheErr("Erreur lors de l'envoi de la reponse au Client");
                    ERR_print_errors_fp(stderr);
                    return;
                }

                // lecture prochaine requête du Client
                receivedMessage = receiveRequest(ssl);
                if(receivedMessage.empty()){ // erreur lecture msg
                    afficheErr("Erreur lors de la reception du message (vide).");
                    return;
                }
            }

            response = DeConnection(id);
            // Envoi de la reponse au client
            if(sendResponse(ssl,response) != 0){
                // Erreur au niveau de l'envoie de la reponse
                afficheErr("Erreur lors de l'envoi de la reponse au Client");
                ERR_print_errors_fp(stderr);
                return; //exit(EXIT_FAILURE);
            }
        }/* En cas d'erreur d'authentification, par mesure de securité on sort de la fonction,
         ce qui entraine la fermeture de la connexion socket */
    }
    /**
     * Si le message reçu par le serveur ne contient pas d'identifiant (de qui envie le message),
     * alors le message n'est pas traité, soit aucune réponse n'est envoyé.
     * En effet, on suppose ici que les clients respectent l'API. Ainsi tout messsage non conforme est ignoré.
     * 
     * Ce comportement pourrait être modifié pour diverses raisons ...
    */
    

    
    /* Intégration des Bots dans le Client ...

    // Initialiser le bot pour ce client
    Bot tradingBot("SRD-BTC");
    // Boucle pour appeler les méthodes d'investissement chaque seconde
    while (true)
    {
        std::cout << "Appel de la méthode d'investissement..." << std::endl;
        tradingBot.investing();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    */
}

// Fonction principale pour démarrer le serveur
void Server::StartServer(const std::string &certFile, const std::string &keyFile)
{
    // Charger les valeurs quotidiennes du BTC
    const std::string filename = "../src/data/btc_data.csv";
    const std::string btcSecFilename = "../src/data/btc_sec_values.csv";

    std::filesystem::path file_path = std::filesystem::absolute(filename);
    affiche("Vérification du chemin absolu du fichier: " + file_path.string());

    if (!std::filesystem::exists(file_path)) // erreur de lecture du fichier
    {
        afficheErr("Erreur: Le fichier " + file_path.string() + " n'existe pas.");
        return;
    }

    // Remplir les valeurs quotidiennes du BTC
    Global::populateBTCValuesFromCSV(file_path.string());

    // Compléter les valeurs du BTC à chaque seconde
    Global::Complete_BTC_value();

    // Lire les valeurs de BTC_sec_values à partir du fichier CSV
    Global::readBTCValuesFromCSV(btcSecFilename);

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
    {
        perror("Échec de la création de la socket");
        exit(EXIT_FAILURE);
    }

    // Initialisation du socket du Serveur
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    // Bind du Serveur
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("Échec du bind");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    // Mise en écoute du Serveur
    if (listen(serverSocket, 5) < 0)
    {
        perror("Échec de l'écoute");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    // Certificat pour la connexion SSL/TLS ave OpenSSL
    SSL_CTX *ctx = InitServerCTX(certFile, keyFile);
    affiche("en écoute sur le port " + std::to_string(PORT));

    // chargement des utilisateurs
    users = LoadUsers(usersFile);

    socklen_t addrLen;
    int clientSocket;
    SSL *ssl;

    while (true)
    {   
        // Accepter une connection du client
        addrLen = sizeof(serverAddr);
        clientSocket = accept(serverSocket, (struct sockaddr *)&serverAddr, &addrLen);

        if (clientSocket < 0)
        {
            perror("Échec de l'acceptation");
            continue;
        }

        // Acepter la connexion SSL
        ssl = AcceptSSLConnection(ctx, clientSocket);
        if (ssl)
        {
            // Traitement de la requête du client
            HandleClient(ssl);
            SSL_free(ssl);
        }
        else
        {
            // si le client ne s'est pas connecté avec SSL (?)
            close(clientSocket);
        }
    }
    close(serverSocket);
    SSL_CTX_free(ctx);
}
