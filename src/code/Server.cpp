#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>  // Pour inet_addr
#include <unistd.h>     // Pour close()
#include <cstring>      // Pour memset
#include <fstream>
#include <thread>        // Pour gérer les connexions clients en threads
#include "../headers/Server.h"
#include "../headers/global.h"
#include <thread>
#include <atomic>
#include <sstream>

void updateBitcoinPrices();
Crypto crypto;

// Constructeur
Server::Server(const std::string& ipAddress, int port, const std::string& configFile) 
    : ipAddress(ipAddress), port(port) {
    std::cout << "Serveur initialisé à l'adresse " << ipAddress 
              << " sur le port " << port << " avec le fichier de configuration : " << configFile << std::endl;
    //setCryptos(configFile);  // Charger les cryptomonnaies à partir du fichier
}

//Chargement des cryptomonnaies
void Server::setCryptos(const std::string& configFile) {
    std::ifstream file(configFile);
    if (!file.is_open()) {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier " << configFile << std::endl;
        return;
    }

    std::string name;
    double price, changeRate;

    // Lecture ligne par ligne
    while (file >> name >> price >> changeRate) {
        cryptos.emplace_back(name, price, changeRate);  
        std::cout << "Crypto ajoutée : " << name << " | Prix : " << price 
                  << " | Taux de variation : " << changeRate << std::endl;
    }
    file.close();

    if (cryptos.empty()) {
        std::cerr << "Aucune cryptomonnaie chargée depuis le fichier de configuration.\n";
    }
}

// Retourne les cryptomonnaies
const std::vector<Crypto>& Server::getCryptos() const {
    return cryptos;
}

// Méthode principale du serveur
void Server::start() {
    // Création du socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Erreur : Impossible de créer le socket.\n";
        exit(EXIT_FAILURE);
    }

    // Configuration de l'adresse du serveur
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = inet_addr(ipAddress.c_str());

    // Liaison du socket
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "Erreur : Échec du bind sur " << ipAddress << ":" << port << "\n";
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    // Mise en écoute des connexions
    if (listen(serverSocket, 5) < 0) {
        std::cerr << "Erreur : Échec de listen().\n";
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    std::thread bitcoinPriceThread(updateBitcoinPrices);

    std::cout << "Serveur démarré et en attente de connexions sur " 
              << ipAddress << ":" << port << "...\n";


    while (true) {
         // Accepter une connexion client
    sockaddr_in clientAddress{};
    socklen_t clientAddressLen = sizeof(clientAddress);
    int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientAddressLen);
    if (clientSocket < 0) {
        std::cerr << "Erreur : Échec de l'acceptation d'une connexion.\n";
        continue;
    }
     // Afficher l'adresse du client
    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddress.sin_addr, clientIP, sizeof(clientIP));
    std::cout << "Nouvelle connexion acceptée de " << clientIP 
            << ":" << ntohs(clientAddress.sin_port) << "\n";
    std::cout << "Client connecté.\n";
    
    Request(clientSocket);
    close(clientSocket);
    }
    stopRequested = true;
    bitcoinPriceThread.join();
    close(serverSocket);
}
Crypto crypto1;

void Server::Request(int clientSocket) {
    while(true){
        char buffer[1024] = {0}; // Tampon pour stocker la réponse
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead < 0) {
            std::cerr << "Erreur de réception." << std::endl;
            break;
        }
        buffer[bytesRead] = '\0'; // Terminer la chaîne
        std::string request(buffer); // Convertir le tampon en std::string
        std::cout << "Requête reçue : " << request << std::endl;

        std::string response;
        if (request.rfind("BUY", 0) == 0) {
            std::cout << "Passage dans rfindBUY " << std::endl;
            // Traiter la commande BUY
            std::string response = handleBuy(request);
            send(clientSocket, response.c_str(), response.size(), 0);
        } else if (request.rfind("SELL", 0) == 0) {
            std::cout << "Passage dans rfindSELL " << std::endl;
            // Traiter la commande SELL
            std::string response = handleSell(request);
            send(clientSocket, response.c_str(), response.size(), 0);
        } else {
            response = "Commande inconnue\n";
        }
        send(clientSocket, response.c_str(), response.size(), 0);
    }
}


std::string Server::handleBuy(const std::string& request) {
// Extraire la paire de crypto et le montant de la requête
    std::istringstream iss(request);
    std::string action, currency;
    double percentage;
    if (!(iss >> action >> currency >> percentage)) {
        return "Erreur : Format de commande invalide\n";
    }
    if (percentage <=0 || percentage > 100) {
        return "Erreur : Pourcentage invalide\n";
    }
    crypto.buyCrypto(currency, percentage);
    return "Achat de " + std::to_string(percentage) + " " + currency + " réussi\n";
    }
std::string Server::handleSell(const std::string& request) {
    // Extraire la paire de crypto et le montant de la requête
    std::istringstream iss(request);
    std::string action, currency;
    double percentage;
    if (!(iss >> action >> currency >> percentage)) {
        return "Erreur : Format de commande invalide\n";
    }
    crypto.sellCrypto(currency, percentage);
    return "Vente de " + std::to_string(percentage) + "% " + currency + " réussi\n";
    }


// Fonction pour mettre à jour les prix de Bitcoin en continu et les enregistrer dans un fichier
void updateBitcoinPrices() {
    std::string filename = "SRD-BTC.dat";
    std::ofstream outFile(filename, std::ios::app);
    if (!outFile) {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier " << filename << ".\n";
        return;
    }
    Crypto crypto;
    int i = 0;
    while (!stopRequested) {
        double bitcoinPrice = crypto.getPrice("SRD-BTC");
        std::time_t timestamp = std::time(nullptr);
        outFile << timestamp << " " << bitcoinPrice << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(1)); // Pause d'une seconde
        if (++i >= 10000) {  // Arrêter après 10000 itérations
            stopRequested = true;
            std::cout << "Fin de la mise à jour des prix de Bitcoin.\n";
        }
    }
    }
