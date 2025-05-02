#include "../headers/Transaction.h"
#include <fstream>
#include <iostream>
#include <iomanip>

// Initialisation du compteur
int Transaction::counter = 0;

// Constructeur
Transaction::Transaction(const std::string &clientId, const std::string &type, const std::string &cryptoName,
                         double quantity, double unitPrice)
    : clientId(clientId), type(type), cryptoName(cryptoName), quantity(quantity), unitPrice(unitPrice)
{
    totalAmount = quantity * unitPrice;    // Calculer le montant total
    timestamp = std::time(nullptr);        // Initialise le timestamp au moment de la création
    id = "TX" + std::to_string(++counter); // Génère un ID unique
    //std::cout << "Transaction créée : " << id << ", " << type << ", " << cryptoName << ", " << quantity << ", " << unitPrice << ", " << totalAmount << "\n";
}

// Méthode pour retourner l'ID de la transaction
std::string Transaction::getId() const
{
    return id;
}

// Méthode pour enregistrer la transaction dans le fichier CSV
void Transaction::logTransactionToCSV(const std::string &filename) const
{
    std::ofstream file(filename, std::ios::app);
    if (!file.is_open())
    {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier " << filename << " pour écriture.\n";
        return;
    }

    //std::cout << "Enregistrement de la transaction dans le fichier " << filename << "\n";

    // Vérifie si le fichier est vide pour ajouter l'en-tête
    if (file.tellp() == 0)
    {
        file << "ClientID,ID,Type,CryptoName,Quantity,UnitPrice,TotalAmount,Timestamp\n";
    }

    // Convertir le timestamp en date lisible
    char readableTimestamp[20];
    std::strftime(readableTimestamp, sizeof(readableTimestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&timestamp));

    // Écrire les données de la transaction
    file << clientId << "," << id << "," << type << "," << cryptoName << "," << quantity << ","
         << unitPrice << "," << totalAmount << "," << readableTimestamp << "\n";

    file.close();
}

// Méthode pour lire une transaction spécifique ou toutes les transactions
std::string Transaction::readTransaction(int i, const std::string &filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier " << filename << " pour lecture.\n";
        return "";
    }

    std::string line, result;
    int currentLine = 0;

    if (i == 0)
    {
        // Lire la dernière ligne
        while (getline(file, line))
        {
        } // Lecture jusqu'à la dernière ligne
        result = line;
    }
    else if (i > 0)
    {
        // Lire la ligne spécifiée
        while (getline(file, line))
        {
            if (++currentLine == i + 1)
            { // Ligne i+1 (car la première ligne est l'en-tête)
                result = line;
                break;
            }
        }
    }
    else
    {
        // Lire toutes les lignes (sauf l'en-tête)
        getline(file, line); // Ignorer l'en-tête
        while (getline(file, line))
        {
            result += line + "\n";
        }
    }

    file.close();
    return result.empty() ? "Aucune transaction trouvée." : result;
}