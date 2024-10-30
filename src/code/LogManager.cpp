#include <fstream>    // Pour std::ofstream
#include <iomanip>    // Pour formatage des dates
#include <sstream>    // Pour convertir le timestamp
#include <iostream>   // Pour std::cerr
#include "Transaction.h"

// Fonction pour formater un timestamp
std::string formatTimestamp(std::time_t timestamp) {
    std::tm* tmPtr = std::localtime(&timestamp);
    std::ostringstream oss;
    oss << std::put_time(tmPtr, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// Méthode pour ajouter une transaction au fichier CSV
void addTransactionToCSV(const Transaction& transaction, const std::string& filename = "transactions.csv") {
    std::ofstream file(filename, std::ios::app); // Ouvre le fichier en mode ajout
    
    if (file.is_open()) {
        // Si c'est un fichier vide, ajoute l'en-tête
        if (file.tellp() == 0) {
            file << "ID,Type,CryptoName,Quantity,UnitPrice,TotalAmount,Timestamp\n";
        }
        
        // Ecriture de la transaction
        file << transaction.getId() << ","
             << transaction.getType() << ","
             << transaction.getCryptoName() << ","
             << transaction.getQuantity() << ","
             << transaction.getUnitPrice() << ","
             << transaction.getTotalAmount() << ","
             << formatTimestamp(transaction.getTimestamp()) << "\n";
        
        file.close();
    } else {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier " << filename << " pour l'écriture.\n";
    }
}
