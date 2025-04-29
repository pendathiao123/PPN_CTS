#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <string>
#include <ctime>    
#include <fstream>  
#include <iostream> 

#include "../headers/Logger.h"


class Transaction {
private:
    static int counter; // Compteur statique pour générer les IDs uniques
    /*Note : L'accès à 'counter' dans le constructeur se fait dans le thread
    de la TransactionQueue (via applyTransactionRequest), donc il est protégé
    par le traitement séquentiel de la queue. Si plusieurs threads pouvaient
    créer des Transactions simultanément, il faudrait un mutex pour protéger 'counter'.*/

    std::string id;         // ID unique de la transaction
    std::string clientId;   // ID du client/bot qui a initié la transaction
    std::string type;       // Type de transaction (ex: "BUY", "SELL")
    std::string cryptoName; // Nom de la cryptomonnaie (ex: "SRD-BTC")
    double quantity;        // Quantité de cryptomonnaie échangée
    double unitPrice;       // Prix unitaire en devise (ex: USD) au moment de la transaction
    double totalAmount;     // Montant total en devise (quantity * unitPrice)
    std::time_t timestamp;  // Timestamp de la transaction

public:
    // Constructeur : crée un objet Transaction
    Transaction(const std::string &clientId, const std::string &type, const std::string &cryptoName,
                         double quantity, double unitPrice);

    // Retourne l'ID de la transaction
    std::string getId() const;

    // Méthode pour enregistrer la transaction dans un fichier CSV
    // Utilise le Logger et gère l'ajout de l'en-tête.
    void logTransactionToCSV(const std::string &filename) const;

    // Méthode pour lire des transactions depuis le fichier (peut rester simple)
    // Retourne une chaîne formatée ou brute des transactions trouvées.
    static std::string readTransaction(int i, const std::string &filename);

    // Méthodes statiques pour gérer la persistance du compteur de transactions
    // Appelées par le serveur au démarrage et à l'arrêt.
    static void loadCounter(const std::string& filename);
    static void saveCounter(const std::string& filename);
};

#endif