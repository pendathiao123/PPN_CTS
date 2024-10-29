#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <string>
#include <ctime>
#include <fstream>
#include <iostream>

class Transaction {
private:
    std::string id;           // Identifiant unique de la transaction
    std::string type;         // Type de transaction ("buy" ou "sell")
    std::string cryptoName;   // Nom de la crypto-monnaie
    double quantity;          // Quantité de crypto-monnaie achetée/vendue
    double unitPrice;         // Prix unitaire lors de la transaction
    double totalAmount;       // Montant total de la transaction (quantity * unitPrice)
    std::time_t timestamp;    // Timestamp de la transaction

    static int counter;  // Compteur statique pour générer des IDs

public:
    // Constructeur
    Transaction(const std::string& type, const std::string& cryptoName, 
                double quantity, double unitPrice);

    // Getters pour accéder aux attributs de la transaction
    std::string getId() const { return id; }
    std::string getType() const { return type; }
    std::string getCryptoName() const { return cryptoName; }
    double getQuantity() const { return quantity; }
    double getUnitPrice() const { return unitPrice; }
    double getTotalAmount() const { return totalAmount; }
    std::time_t getTimestamp() const { return timestamp; }

    // Méthode pour enregistrer la transaction dans le fichier CSV
    static void logTransactionToCSV(const Transaction& transaction, const std::string& filename);
};

#endif  // TRANSACTION_H
