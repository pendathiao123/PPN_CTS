#include <iostream>
#include <string>
#include <unordered_map>
#include <sstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h> 

#include "Crypto.h"
#include "ClientHandler.h"

ClientHandler::ClientHandler(int clientsocket, std::unordered_map<std::string, double>& balances)
    : clientSocket(clientsocket), balances(balances) {}


void ClientHandler::sendResponse(const std::string& response) {
    send(clientSocket, response.c_str(), response.size(), 0);
}

void ClientHandler::processRequest() {
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
        
        sendResponse(response);
        }      
}



std::string ClientHandler::handleMarket(const std::string& request) {
    // Traitement du market
    return "Valeur actuelle du SRD-BTC : " + std::to_string(cryptoInstance.getPrice("SRD-BTC")) + "\n";
}
std::string ClientHandler::handleBalance(const std::string& currency) {
    // Utilisation de find pour chercher la clé
    auto it = balances.find(currency); // Rechercher la monnaie dans balances
    
    // Vérification si la clé a été trouvée
    if (it != balances.end()) {
        if (currency == "DOLLARS") {
            return "Solde de " + currency + ": " + std::to_string(it->second) + " USD\n";
        } else {
            double quantity = it->second;
            double valueInUSD = quantity * cryptoInstance.getPrice(currency);
            return "Quantité de " + currency + ": " + std::to_string(quantity) + " " + "Solde de " + currency + ": " + std::to_string(valueInUSD) + " USD\n";
        }
    }
    return "Erreur : Monnaie inconnue\n";
}


std::string ClientHandler::handleBuy(const std::string& request) {
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
            balances["DOLLARS"] -= amount * cryptoInstance.getPrice(currency); 
            //Traitement de l'achat
            return "Achat de " + std::to_string(amount) + " " + currency + " réussi\n";
        } else {
            return "Erreur : Solde insuffisant pour acheter " + currency + "\n";
        }  
    } else {
        return "Erreur : Monnaie inconnue\n";
    }
}


std::string ClientHandler::handleSell(const std::string& request) {
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