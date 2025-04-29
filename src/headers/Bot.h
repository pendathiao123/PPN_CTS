#ifndef BOT_H
#define BOT_H

#include <iostream>
#include <unordered_map>
#include <string>
#include <memory>       
#include <fstream>      
#include <mutex>      

// Déclaration anticipée pour TransactionQueue
class TransactionQueue;

// Inclure TransactionQueue.h pour la structure TransactionRequest et l'enum RequestType
#include "../headers/TransactionQueue.h"
#include "../headers/Logger.h"


class Bot {
private:
    std::string id; // ID unique du bot (correspond à l'ID client)
    std::unordered_map<std::string, double> balance; // Le portefeuille en mémoire
    mutable std::mutex balanceMutex; // Mutex pour protéger la map 'balance'

    // Pointeur vers la TransactionQueue (pas possédé par le bot)
    TransactionQueue* transactionQueue = nullptr;

    // Méthodes pour charger/sauvegarder le portefeuille depuis/vers un fichier
    void loadWallet();
    void saveBalance();

public:
    // Constructeur
    Bot(const std::string& clientId);

    // Destructeur
    ~Bot();

    // Retourne l'ID du bot
    std::string getId() const;

    // Méthode appelée par BotSession pour fournir l'accès à la TransactionQueue
    void setTransactionQueue(TransactionQueue* queue);

    // Méthodes pour soumettre des requêtes d'achat/vente
    void submitBuyRequest(const std::string& currency, double pourcentage);
    void submitSellRequest(const std::string& currency, double pourcentage);

    // Méthode de trading
    void trading(const std::string& currency);

    // Méthodes d'accès au solde (utilisent balanceMutex dans l'implémentation)
    double getBalance(const std::string& currency) const;
    void setBalance(const std::string& currency, double value);

    // Déclenche la sauvegarde
    void updateBalance();
};

#endif