#include "../headers/Bot.h"
#include "../headers/Global.h"
#include "../headers/TransactionQueue.h"
#include "../headers/Logger.h"

#include <iostream>
#include <unordered_map>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <limits>
#include <mutex>


// Constructeur : initialise l'ID, le mutex et charge le portefeuille
Bot::Bot(const std::string &clientId)
    : id(clientId)
    // balanceMutex est automatiquement initialisé
{
    LOG("Bot créé pour client ID: " + id, "DEBUG");
    loadWallet();
}

// Destructeur
Bot::~Bot() {
    LOG("Destructeur Bot appelé pour client ID: " + id, "DEBUG");
    // La sauvegarde est gérée par BotSession dans son destructeur.
}

std::string Bot::getId() const {
    return id;
}

void Bot::setTransactionQueue(TransactionQueue* queue) {
    transactionQueue = queue;
    if (queue) {
        LOG("TransactionQueue définie pour le bot " + id, "DEBUG");
    } else {
        LOG("TransactionQueue définie à null pour le bot " + id, "WARNING");
    }
}


// Charge le portefeuille depuis un fichier CSV
void Bot::loadWallet() {
    std::lock_guard<std::mutex> lock(balanceMutex); // Protège l'accès à 'balance'

    std::ifstream walletFile("../src/data/wallets/" + id + ".wallet"); // Chemin hardcodé
    if (walletFile.is_open()) {
        balance.clear(); // Nettoyer la map avant de charger
        std::string line;
        while (getline(walletFile, line)) {
            std::istringstream stream(line);
            std::string currency;
            double balance_value;
            if (std::getline(stream, currency, ':') && (stream >> balance_value)) {
                balance[currency] = balance_value;
            }
        }
        walletFile.close();
        LOG("Portefeuille chargé pour client ID: " + id + ". Solde DOLLARS: " + std::to_string(balance["DOLLARS"]) + ", SRD-BTC: " + std::to_string(balance["SRD-BTC"]), "INFO");
    } else {
        // Initialiser le portefeuille si le fichier n'existe pas
        balance.clear(); // Assurer qu'elle est vide
        balance["DOLLARS"] = 10000.0; // Solde initial hardcodé
        balance["SRD-BTC"] = 0.0;
        LOG("Fichier portefeuille non trouvé pour client ID: " + id + ". Initialisation avec solde par défaut.", "INFO");
        /*On ne sauvegarde pas ici, BotSession ou un autre mécanisme gérera la sauvegarde initiale si c'est un nouvel utilisateur.
        La sauvegarde est faite par SaveUsers appelée dans HandleClient après création du wallet file.*/
    }
}

// Sauvegarde le portefeuille vers un fichier CSV
void Bot::saveBalance() {
    std::lock_guard<std::mutex> lock(balanceMutex); // Protège l'accès à 'balance'

    std::ofstream walletFile("../src/data/wallets/" + id + ".wallet");
    if (walletFile.is_open()) {
        for (const auto &entry : balance) {
            walletFile << entry.first << ":" << std::fixed << std::setprecision(10) << entry.second << "\n";
        }
        walletFile.close();
        LOG("Solde sauvegardé pour client ID: " + id + ".", "INFO");
    } else {
        LOG("Erreur : Impossible d'ouvrir le fichier portefeuille pour sauvegarde : " + id, "ERROR");
    }
}

// Méthodes pour soumettre des requêtes (ne modifient pas le solde directement)
void Bot::submitBuyRequest(const std::string &currency, double pourcentage) {
    /* L'accès en lecture au solde pour la vérification basique peut se faire ici, mais la vérification finale et la 
    modification sont faites dans applyTransactionRequest. Pour être thread-safe, même une lecture ici devrait utiliser le mutex
    si le solde peut être modifié. Mais comme applyTransactionRequest fait la vérification finale, on peut s'en passer ici pour simplifier
    ou ajouter un lock_guard si la lecture ici est critique. Ajoutons le lock pour la lecture aussi, par sécurité.*/
    std::lock_guard<std::mutex> lock(balanceMutex); // Protège la lecture

    if (!transactionQueue) { /* ... */ return; }
    if (balance.count("DOLLARS") == 0 || balance["DOLLARS"] <= 0) { /* ... */ return; }

    TransactionRequest request(id, RequestType::BUY, currency, pourcentage);
    transactionQueue->addRequest(request);
    LOG("Bot " + id + " a soumis une requête d'ACHAT pour " + std::to_string(pourcentage * 100) + "% de capital en " + currency + ".", "INFO");
}

void Bot::submitSellRequest(const std::string &currency, double pourcentage) {
    std::lock_guard<std::mutex> lock(balanceMutex); // Protège la lecture

    if (!transactionQueue) { /* ... */ return; }
    if (balance.count(currency) == 0 || balance[currency] <= 0) { /* ... */ return; }

    TransactionRequest request(id, RequestType::SELL, currency, pourcentage);
    transactionQueue->addRequest(request);
    LOG("Bot " + id + " a soumis une requête de VENTE pour " + std::to_string(pourcentage * 100) + "% de " + currency + ".", "INFO");
}

// Méthode de trading (lit le solde pour décider), je vais bientôt l'implémenter avec une méthode plus raffinée
void Bot::trading(const std::string &currency) {
    double currentPrice = Global::getPrice(currency);
    double previousPrice = Global::getPreviousPrice(currency, 60);

    if (currentPrice <= 0 || previousPrice <= 0) { /* ... */ return; }

    // Lit le solde pour décider s'il y a quelque chose à acheter/vendre
    // Ces appels à getBalance doivent utiliser le mutex à l'intérieur de getBalance.
    // La décision elle-même ne modifie pas le solde.

    if (currentPrice > previousPrice) {
        submitBuyRequest(currency, 0.1);
    } else if (currentPrice < previousPrice) {
        submitSellRequest(currency, 0.1);
    }
}

// Retourne le solde d'une devise
double Bot::getBalance(const std::string &currency) const {
    std::lock_guard<std::mutex> lock(balanceMutex); // <<<--- PROTÈGE l'accès en lecture

    auto it = balance.find(currency);
    if (it != balance.end()) {
        return it->second;
    }
    return 0.0;
}

// Modifie le solde d'une devise
void Bot::setBalance(const std::string &currency, double value) {
    std::lock_guard<std::mutex> lock(balanceMutex); // Protège l'accès en écriture

    balance[currency] = value;
}

void Bot::updateBalance() {
    // Cette méthode appelle saveBalance(), qui utilise déjà le mutex.
    saveBalance();
}