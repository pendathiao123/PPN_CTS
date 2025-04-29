#include "../headers/Transaction.h"
#include "../headers/Logger.h" 

#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <limits>       
#include <sstream>     
#include <vector>       
#include <ctime>       


// Initialisation du compteur statique. Sa valeur sera chargée depuis le fichier au démarrage.
int Transaction::counter = 0;

// Constructeur : calcule les montants, timestamp et génère l'ID unique
Transaction::Transaction(const std::string &clientId, const std::string &type, const std::string &cryptoName,
                         double quantity, double unitPrice)
    : clientId(clientId), type(type), cryptoName(cryptoName), quantity(quantity), unitPrice(unitPrice)
{
    totalAmount = quantity * unitPrice;    // Calculer le montant total
    timestamp = std::time(nullptr);        // Initialise le timestamp au moment de la création
    // Utilise le compteur statique. L'incrémentation ici est sécurisée car seule
    // BotSession::applyTransactionRequest (appelée par le thread unique de la queue)
    // crée des objets Transaction.
    id = "TX" + std::to_string(++counter);
    // LOG("Transaction créée : " + id + ", Type: " + type + ", Crypto: " + cryptoName, LogLevel::DEBUG); // Trop verbeux ?
}

// Méthode pour retourner l'ID de la transaction
std::string Transaction::getId() const
{
    return id;
}

// Méthode pour enregistrer la transaction dans le fichier CSV
// Cette méthode est appelée par BotSession::applyTransactionRequest (dans le thread de la TransactionQueue).
// Comme un seul thread (celui de la queue) logge, l'accès au fichier est implicitement sécurisé.
void Transaction::logTransactionToCSV(const std::string &filename) const
{
    // Mutex statique pour protéger l'accès au fichier CSV des transactions.
    // Il est partagé par toutes les instances de Transaction qui appellent cette fonction.
    static std::mutex csvMutex; // Mutex statique pour le fichier CSV

    std::lock_guard<std::mutex> lock(csvMutex); // Protège l'accès au fichier

    // Utilise le Logger pour les messages d'erreur ou d'information
    std::ofstream file(filename, std::ios::app); // Ouverture en mode append
    if (!file.is_open())
    {
        LOG("Erreur : Impossible d'ouvrir le fichier de transactions " + filename + " pour écriture.", "ERROR");
        return; // Sortie en cas d'erreur. Le verrou est relâché automatiquement.
    }

    // Vérifier si le fichier est vide pour ajouter l'en-tête
    file.seekp(0, std::ios::end); // Aller à la fin
    if (file.tellp() == 0) // Si la position est 0, le fichier est vide
    {
        file << "ClientID,ID,Type,CryptoName,Quantity,UnitPrice,TotalAmount,Timestamp\n";
    }

    // Formater le timestamp (utiliser localtime_r ou gmtime_r pour la sécurité thread)
    char readableTimestamp[20];
    struct tm timeinfo_buffer;
    std::tm* timeinfo = localtime_r(&timestamp, &timeinfo_buffer);

    if (!timeinfo) {
        LOG("Erreur : localtime_r a retourné nullptr pour timestamp = " + std::to_string(timestamp) + ". Impossible de logguer la transaction " + id + ".", "ERROR");
        file.close(); // Fermer le fichier. Le verrou est toujours actif.
        return; // Sortie si le timestamp n'est pas convertible
    }

    std::strftime(readableTimestamp, sizeof(readableTimestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    // Écrire les données de la transaction dans le fichier CSV
    file << clientId << ","
         << id << ","
         << type << ","
         << cryptoName << ","
         << std::fixed << std::setprecision(10) << quantity << ","
         << std::fixed << std::setprecision(10) << unitPrice << ","
         << std::fixed << std::setprecision(10) << totalAmount << ","
         << readableTimestamp << "\n";

    file.close(); // Fermer le fichier. Le verrou est toujours actif.
    // Le verrou sur csvMutex est relâché automatiquement ici à la fin de la fonction.
}


// Méthode statique pour lire une transaction spécifique ou toutes les transactions
// Peut être utilisée par Server::ProcessRequest pour SHOW TRANSACTION HISTORY
std::string Transaction::readTransaction(int i, const std::string &filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        LOG("Erreur : Impossible d'ouvrir le fichier de transactions " + filename + " pour lecture.", "ERROR");
        return "Erreur lors de l'ouverture de l'historique des transactions.";
    }

    std::string line, result;
    int currentLine = 0;

    // Lire et ignorer l'en-tête
    if (!getline(file, line)) {
        file.close();
        return "Le fichier d'historique des transactions est vide ou illisible.";
    }

    if (i == 0)
    {
        // Lire la dernière ligne (après l'en-tête)
        std::string last_line;
        while (getline(file, line))
        {
            last_line = line;
        }
        result = last_line;
    }
    else if (i > 0)
    {
        // Lire la ligne spécifiée (après l'en-tête)
        while (getline(file, line))
        {
            if (++currentLine == i) // Ligne i (après avoir sauté l'en-tête)
            {
                result = line;
                break;
            }
        }
    }
    else // i < 0
    {
        // Lire toutes les lignes (sauf l'en-tête)
        while (getline(file, line))
        {
            result += line + "\n";
        }
    }

    file.close();
    // Retourne la/les ligne(s) lue(s) ou un message si rien n'a été trouvé (hors en-tête)
    return result.empty() ? "Aucune transaction trouvée." : result;
}


// Méthode statique pour charger le compteur depuis un fichier
void Transaction::loadCounter(const std::string& filename) {
    std::ifstream file(filename);
    if (file.is_open()) {
        // Essayer de lire un entier. Si ça échoue (fichier vide ou non-numérique), on reste à 0.
        if (!(file >> counter)) {
            counter = 0; // Initialiser à 0 si la lecture échoue
            LOG("Fichier compteur de transactions " + filename + " vide ou illisible. Initialisation du compteur à 0.", "WARNING");
        } else {
             LOG("Compteur de transactions chargé depuis " + filename + " : " + std::to_string(counter), "INFO");
        }
        file.close();
    } else {
        counter = 0; // Initialiser à 0 si le fichier n'existe pas
        LOG("Fichier compteur de transactions non trouvé (" + filename + "). Initialisation du compteur à 0.", "WARNING");
    }
}

// Méthode statique pour sauvegarder le compteur dans un fichier
void Transaction::saveCounter(const std::string& filename) {
    std::ofstream file(filename); // Ouvre en mode troncation (écrase)
    if (file.is_open()) {
        file << counter; // Sauvegarder la valeur actuelle du compteur
        file.close();
        LOG("Compteur de transactions sauvegardé dans " + filename + " : " + std::to_string(counter), "INFO");
    } else {
        LOG("Erreur : Impossible d'ouvrir le fichier " + filename + " pour sauvegarder le compteur de transactions.", "ERROR");
    }
}