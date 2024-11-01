#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>  // Pour inet_addr
#include <unistd.h>     // Pour close()
#include <cstring>      // Pour memset
#include "Server.h"


Server::Server(const std::string& ipAddress, int port, const std::string& configFile) 
    : ipAddress(ipAddress), port(port) {
    std::cout << "Serveur initialisé à l'adresse " << ipAddress 
              << " sur le port " << port << " avec " << configFile << std::endl;
}

// Constructeur qui charge les cryptomonnaies à partir d'un fichier de configuration
void Server::setCryptos(const std::string& configFile) {
    std::ifstream file(configFile);
    if (!file.is_open()) {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier " << configFile << std::endl;
        return;
    }

    // Logique pour lire le fichier et initialiser le vecteur cryptos
    // Exemple : lecture ligne par ligne et création d'objets Crypto
    std::string name;
    double price;
    double changeRate;

    while (file >> name >> price >> changeRate) {
        cryptos.emplace_back(name, price, changeRate);  // Supposons que Crypto a un constructeur qui prend un nom et un prix,et un taux de variation
    }
}

// Retourne le vecteur de cryptomonnaies
const std::vector<Crypto>& Server::getCryptos() const {
    return cryptos;
}

// Méthode pour enregistrer la transaction dans le fichier CSV (arguments à rajouter ?)
static void Server::logTransactionToCSV(const std::string& filename) {
    std::ofstream file(filename, std::ios::app);
    if (file.is_open()) {
        // Vérifie si le fichier est vide pour écrire l'en-tête
        if (file.tellp() == 0) {
            file << "ID,Type,CryptoName,Quantity,UnitPrice,TotalAmount,Timestamp\n";  // Écrire l'en-tête
        }

    
        // Convertir le timestamp en date lisible
        std::time_t timestame = transaction.getTimestamp();
        std::tm *tm = std::localtime(&timestame);
        char readableTimestamp[20];
        std::strftime(readableTimestamp, sizeof(readableTimestamp), "%Y-%m-%d %H:%M:%S", tm); // Formatage

        // Écrire les données de la transaction 
        file << transaction.getId() << ","
             << transaction.getType() << ","
             << transaction.getCryptoName() << ","
             << transaction.getQuantity() << ","
             << transaction.getUnitPrice() << ","
             << transaction.getTotalAmount() << ","
             << readableTimestamp << "\n";
        file.close();
    } else {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier " << filename << std::endl;
    }
}


void Server::start() {
    // Création du socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Erreur : Impossible de créer le socket.\n";
        exit(1);
    }

    // Configuration de l'adresse du serveur
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = inet_addr(ipAddress.c_str());  // Utilise l'IP spécifiée

    // Liaison du socket à l'adresse et au port
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "Erreur : Échec du bind.\n";
        close(serverSocket);
        exit(1);
    }

    // Mise en écoute des connexions
    if (listen(serverSocket, 5) < 0) {
        std::cerr << "Erreur : Échec de listen().\n";
        close(serverSocket);
        exit(1);
    }

    while (true) {
        // Accepter une nouvelle connexion
        sockaddr_in clientAddress{};
        socklen_t clientAddressLen = sizeof(clientAddress);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientAddressLen);
        if (clientSocket < 0) {
            std::cerr << "Erreur : Échec de l'acceptation de la connexion.\n";
            continue;  // Passer à la prochaine tentative d'acceptation
        }

        std::cout << "Serveur démarré et à l'écoute des connexions sur " 
                << ipAddress << ":" << port << "...\n";
        close(clientSocket);          
    }

    close(serverSocket);

}