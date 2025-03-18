#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <string>
#include <ctime>

class Transaction
{
private:
    // numéro d'identification de la transaction
    std::string id;
    // identifiant du client efectuant la transaction
    std::string clientId;
    // type de transaction
    std::string type;
    // cryptomonaie utilisée pour la transaction
    std::string cryptoName;
    // quantité de cryptomonaie concernée par la transaction
    double quantity;
    // prix unitaire (en $) de la cryptomonaie
    double unitPrice;
    // valeur totale de la transaction
    double totalAmount;
    // heure de la transaction
    std::time_t timestamp;
    // compteur statique de classe
    static int counter;

public:
    // Constructeur
    Transaction(const std::string &clientId, const std::string &type, const std::string &cryptoName, double quantity, double unitPrice);
    // Méthode pour enregistrer la transaction dans le fichier CSV
    void logTransactionToCSV(const std::string &filename) const;
    // Méthode pour retourner l'ID de la transaction
    std::string getId() const;
    // Méthode pour lire une transaction spécifique ou toutes les transactions
    static std::string readTransaction(int i, const std::string &filename);
};

#endif // TRANSACTION_H