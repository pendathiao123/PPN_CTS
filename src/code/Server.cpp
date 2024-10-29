#include "Server.h"
#include "Crypto.h"
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>  // Pour inet_addr
#include <unistd.h>     // Pour close()
#include <cstring>      // Pour memset
#include <sstream>
#include <unordered_map> 

Server::Server(const std::string& ipAddress, int port, const std::string& configFile) 
    : port(port), ipAddress(ipAddress) {
    std::cout << "Serveur initialisé à l'adresse " << ipAddress 
              << " sur le port " << port << " avec " << configFile << std::endl;
}

std::string Server::handleMarket(const std::string& request) {
    // Traitement du market
    return "Valeur actuelle du SRD-BTC : " + std::to_string(cryptoInstance.getPrice("SRD-BTC")) + "\n";
}
std::string Server::handleBalance(const std::string& currency) {
    // Utilisation de find pour chercher la clé
    auto it = balances.find(currency); // Rechercher la monnaie dans balances
    
    // Vérification si la clé a été trouvée
    if (it != balances.end()) {
        if (currency == "DOLLARS") {
            return "Solde de " + currency + ": " + std::to_string(it->second) + " USD\n";
        } else {
            double quantity = it->second;
            double valueInUSD = quantity * cryptoInstance.getPrice(currency);
            return "Quantité de " + currency + ": " + std::to_string(quantity) + " USD\n" + "Solde de " + currency + ": " + std::to_string(valueInUSD) + " USD\n";
        }
    }
    return "Erreur : Monnaie inconnue\n";
}


std::string Server::handleBuy(const std::string& request) {
    // Extraire la paire de crypto et le montant de la requête
    std::istringstream iss(request);
    std::string action, currency;
    double amount;

    if (!(iss >> action >> currency >> amount)) {
        return "Erreur : Format de commande invalide\n";
    }
    if (balances.find(currency) != balances.end()) {
        if (balances.find("DOLLARS")->second >= amount * cryptoInstance.getPrice(currency)) {
            balances[currency] += amount;
            balances["DOLLARS"] -= amount * cryptoInstance.getPrice(currency);; 
            //Traitement de l'achat
            return "Achat de " + std::to_string(amount) + " " + currency + " réussi\n";
        } else {
            return "Erreur : Solde insuffisant pour acheter " + currency + "\n";
        }  
    } else {
        return "Erreur : Monnaie inconnue\n";
    }
}
std::string Server::handleSell(const std::string& request) {
    // Extraire la paire de crypto et le montant de la requête
    std::istringstream iss(request);
    std::string action, currency;
    double amount;

    if (!(iss >> action >> currency >> amount)) {
        return "Erreur : Format de commande invalide\n";
    }
    if (balances.find(currency) != balances.end()) {
        if (balances.find(currency)->second >= amount) { 
            balances[currency] -= amount;
            balances["DOLLARS"] += amount * cryptoInstance.getPrice(currency); 
            //Traitement de la vente 
            return "Vente de " + std::to_string(amount) + " " + currency + " réussi\n";
        } else {
            return "Erreur : Solde insuffisant pour vendre " + currency + "\n";
        }
    } else {
        return "Erreur : Monnaie inconnue\n";
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

    std::cout << "Serveur démarré et à l'écoute des connexions sur " 
              << ipAddress << ":" << port << "...\n";

     while (true) {
        // Accepter une nouvelle connexion
        sockaddr_in clientAddress{};
        socklen_t clientAddressLen = sizeof(clientAddress);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientAddressLen);
        if (clientSocket < 0) {
            std::cerr << "Erreur : Échec de l'acceptation de la connexion.\n";
            continue;  // Passer à la prochaine tentative d'acceptation
        }

    std::cout << "Client connecté.\n";
    request(clientSocket); //Traitement des requetes du client
    close(clientSocket);          
}

 close(serverSocket);

}


void Server::request(int clientSocket) {
     while (true) {
    
        char buffer[1024] = {0}; // Tampon pour stocker la réponse
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead < 0) {
            std::cerr << "Erreur de réception." << std::endl;
            break;
        } else if (bytesRead == 0) { //Client déconnecté, permet de revenir à l'état d'attente de nouvelles connexions(Server::Start)
            std::cout << "Client déconnecté.\n"; //Affichage de la deconnexion du client sur le terminal Server
            break;
        }

        buffer[bytesRead] = '\0'; // Terminer la chaîne
        std::string request(buffer); // Convertir le tampon en std::string
        std::cout << "Requête reçue : " << request << std::endl; // Ajout de log


        std::string response;
        if (request.rfind("GET /market", 0) == 0) {
            std::string response = handleMarket(request);
            send(clientSocket, response.c_str(), response.size(), 0);
        } else if (request.rfind("BUY", 0) == 0) {
            // Traiter la commande BUY
            std::string response = handleBuy(request);
            send(clientSocket, response.c_str(), response.size(), 0);
        } else if (request.rfind("SELL", 0) == 0) {
            // Traiter la commande SELL
            std::string response = handleSell(request);
            send(clientSocket, response.c_str(), response.size(), 0);
        } else if (request.rfind("BALANCE", 0) == 0) {
            std::istringstream iss(request);
            std::string action, currency;
            iss >> action >> currency;
            response = handleBalance(currency);
        } else {
            response = "Commande inconnue\n";
        }

        send(clientSocket, response.c_str(), response.size(), 0);
        }      
}