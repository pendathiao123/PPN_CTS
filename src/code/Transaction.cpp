#include "Transaction.h"
#include <fstream>
#include <iostream>
#include <iomanip>

// Initialisation du compteur
int Transaction::counter = 0;

// Constructeur
Transaction::Transaction(const std::string& type, const std::string& cryptoName, 
                         double quantity, double unitPrice)
    : type(type), cryptoName(cryptoName), quantity(quantity), unitPrice(unitPrice) {
    totalAmount = quantity * unitPrice;
    timestamp = std::time(nullptr);  // Initialise le timestamp au moment de la création
    id = "TX" + std::to_string(++counter);  // Génère un ID unique
}

// Méthode pour enregistrer la transaction dans le fichier CSV
void Transaction::logTransactionToCSV(const Transaction& transaction, const std::string& filename) {
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
