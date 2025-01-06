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

// Gérer une connexion client
void Server::HandleClient(SSL *ssl, std::unordered_map<std::string, std::string> &users, const std::string &usersFile, const std::string &logFile)
{
    char buffer[1024] = {0};
    int bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1);
    if (bytes <= 0)
    {
        ERR_print_errors_fp(stderr);
        return;
    }

    buffer[bytes] = '\0';
    std::string receivedMessage(buffer);
    std::cout << "Reçu: " << receivedMessage << std::endl;

    std::string response;
    std::string id, token;
    std::string prefix_id = "ID:";
    std::string prefix_token = "TOKEN:";

    if (receivedMessage.find(prefix_id) != std::string::npos && receivedMessage.find(prefix_token) != std::string::npos)
    {
        size_t id_start = receivedMessage.find(prefix_id) + prefix_id.length();
        size_t id_end = receivedMessage.find(",", id_start);
        id = receivedMessage.substr(id_start, id_end - id_start);

        size_t token_start = receivedMessage.find(prefix_token) + prefix_token.length();
        token = receivedMessage.substr(token_start);

        std::cout << "ID extrait: " << id << ", Token: " << token << std::endl;

        auto it = users.find(id);
        if (it != users.end() && it->second == token)
        {
            response = "AUTH OK";
        }
        else
        {
            response = "AUTH FAIL";
        }
    }
    else
    {
        id = GenerateRandomId();
        token = GenerateToken();
        users[id] = token;
        SaveUsers(usersFile, users);

        std::cout << "Nouvel ID: " << id << ", Nouveau Token: " << token << std::endl;
        response = "Identifiants: " + id + " " + token;
    }

    int bytesSent = SSL_write(ssl, response.c_str(), response.size());
    if (bytesSent <= 0)
    {
        std::cerr << "Erreur lors de l'envoi de la réponse au client" << std::endl;
        ERR_print_errors_fp(stderr);
    }

    // Initialiser le bot pour ce client
    Bot tradingBot("SRD-BTC");

    // Ajouter pour vérifier les requêtes suivantes
    char buffer2[1024] = {0};
    int bytes2 = SSL_read(ssl, buffer2, sizeof(buffer2) - 1);
    if (bytes2 <= 0)
    {
        ERR_print_errors_fp(stderr);
        return;
    }

    buffer2[bytes2] = '\0';
    std::string nextRequest(buffer2);
    std::cout << "Requête suivante reçue: " << nextRequest << std::endl;

    ProcessRequest(ssl, logFile, nextRequest, id);

    // Boucle pour appeler les méthodes d'investissement chaque seconde
    while (true)
    {
        std::cout << "Appel de la méthode d'investissement..." << std::endl;
        tradingBot.investing();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// Fonction principale pour démarrer le serveur
void Server::StartServer(int port, const std::string &certFile, const std::string &keyFile, const std::string &usersFile, const std::string &logFile)
{
    // Charger les valeurs quotidiennes du BTC
    const std::string filename = "../src/data/btc_data.csv";
    const std::string btcSecFilename = "../src/data/btc_sec_values.csv";

    std::filesystem::path file_path = std::filesystem::absolute(filename);
    std::cout << "Vérification du chemin absolu du fichier: " << file_path << "\n";

    if (!std::filesystem::exists(file_path))
    {
        std::cerr << "Erreur: Le fichier " << file_path << " n'existe pas.\n";
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

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("Échec du bind");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    if (listen(serverSocket, 5) < 0)
    {
        perror("Échec de l'écoute");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    SSL_CTX *ctx = InitServerCTX(certFile, keyFile);
    std::cout << "Serveur en écoute sur le port " << port << std::endl;

    auto users = LoadUsers(usersFile);

    while (true)
    {
        socklen_t addrLen = sizeof(serverAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr *)&serverAddr, &addrLen);

        if (clientSocket < 0)
        {
            perror("Échec de l'acceptation");
            continue;
        }

        SSL *ssl = AcceptSSLConnection(ctx, clientSocket);
        if (ssl)
        {
            HandleClient(ssl, users, usersFile, logFile);
            SSL_free(ssl);
        }
        else
        {
            close(clientSocket);
        }
    }

    close(serverSocket);
    SSL_CTX_free(ctx);
}

// Traiter la requête client
void Server::ProcessRequest(SSL *ssl, const std::string &logFile, const std::string &request, const std::string &clientId)
{
    try
    {
        std::cout << "Traitement de la requête: " << request << std::endl;

        std::string response;
        if (request.rfind("BUY", 0) == 0)
        {
            std::cout << "Entrée dans rfindBUY" << std::endl;
            // Gérer la commande d'achat
            response = handleBuy(request, logFile, clientId);
        }
        else if (request.rfind("SELL", 0) == 0)
        {
            std::cout << "Entrée dans rfindSELL" << std::endl;
            // Gérer la commande de vente
            response = handleSell(request, logFile, clientId);
        }
        else
        {
            response = "Commande inconnue\n";
        }

        if (!response.empty())
        {
            int bytesSent = SSL_write(ssl, response.c_str(), response.length());
            if (bytesSent <= 0)
            {
                std::cerr << "Erreur lors de l'envoi de la réponse au client" << std::endl;
                ERR_print_errors_fp(stderr);
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Erreur dans ProcessRequest: " << e.what() << std::endl;
    }
}

// Gérer la commande d'achat
std::string Server::handleBuy(const std::string &request, const std::string &logFile, const std::string &clientId)
{
    // Extraire la paire de crypto et le pourcentage de la requête
    std::istringstream iss(request);
    std::string action, currency;
    double percentage;
    if (!(iss >> action >> currency >> percentage))
    {
        std::cerr << "Erreur: Format de commande invalide\n";
        return "Erreur: Format de commande invalide\n";
    }
    if (percentage <= 0 || percentage > 100)
    {
        std::cerr << "Erreur: Pourcentage invalide\n";
        return "Erreur: Pourcentage invalide\n";
    }
    std::cout << "Achat de " << percentage << "% de " << currency << " réussi\n";

    // Créer une transaction et l'enregistrer dans le fichier
    Transaction transaction(clientId, "buy", currency, percentage, 30000); // Exemple de prix unitaire, à modifier si nécessaire
    std::cout << "Création de la transaction: " << transaction.getId() << "\n";
    transaction.logTransactionToCSV(logFile);

    return "Achat de " + std::to_string(percentage) + "% de " + currency + " réussi\n";
}

// Gérer la commande de vente
std::string Server::handleSell(const std::string &request, const std::string &logFile, const std::string &clientId)
{
    // Extraire la paire de crypto et le pourcentage de la requête
    std::istringstream iss(request);
    std::string action, currency;
    double percentage;
    if (!(iss >> action >> currency >> percentage))
    {
        std::cerr << "Erreur: Format de commande invalide\n";
        return "Erreur: Format de commande invalide\n";
    }
    if (percentage <= 0 || percentage > 100)
    {
        std::cerr << "Erreur: Pourcentage invalide\n";
        return "Erreur: Pourcentage invalide\n";
    }
    std::cout << "Vente de " << percentage << "% de " << currency << " réussie\n";

    // Créer une transaction et l'enregistrer dans le fichier
    Transaction transaction(clientId, "sell", currency, percentage, 30000); // Exemple de prix unitaire, à modifier si nécessaire
    std::cout << "Création de la transaction: " << transaction.getId() << "\n";
    transaction.logTransactionToCSV(logFile);

    return "Vente de " + std::to_string(percentage) + "% de " + currency + " réussie\n";
}