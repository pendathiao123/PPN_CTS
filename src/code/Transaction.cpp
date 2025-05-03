#include "../headers/Transaction.h" 
#include "../headers/Logger.h"      

#include <fstream>     
#include <iostream>    
#include <sstream>      
#include <iomanip>      
#include <chrono>       
#include <ctime>       
#include <cmath>      
#include <filesystem>  
#include <random>       
#include <mutex>        
#include <cerrno>       
#include <cstring>      

// Ajout pour stringToCurrency, etc.
#include <algorithm>    
#include <cctype>       
#include <unordered_map> 


// --- Définition et initialisation des membres statiques ---
// Ces membres statiques déclarés dans le .h doivent être définis (et initialisés si besoin) dans UN SEUL fichier .cpp
int Transaction::counter = 0; // Initialisation du compteur d'ID unique à 0.
std::mutex Transaction::counterMutex; // Définition du mutex statique pour le compteur.
std::mutex Transaction::persistenceMutex; // Définition du mutex statique pour les accès fichiers statiques.



// --- Implémentation generateUniqueId ---
// Génère un ID unique pour une nouvelle transaction. Thread-safe.
std::string Transaction::generateUniqueId() {
    std::lock_guard<std::mutex> lock(counterMutex); // Protège l'accès au compteur statique

    counter++; // Incrémente le compteur

    auto now = std::chrono::high_resolution_clock::now();
    long long timestamp_nano = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    std::stringstream ss;
    // Format de l'ID : T<timestamp_nanosec>_<counter>
    ss << "T" << timestamp_nano << "_" << counter;

    return ss.str();
}

// --- Implémentation generateNewIdString ---
// Génère et retourne un nouvel ID de transaction unique. Thread-safe.
// Appelée par TransactionQueue.
std::string Transaction::generateNewIdString() {
    return generateUniqueId(); // Appelle simplement la méthode interne protégée.
}


// --- Implémentation des Constructeurs ---

// Constructeur pour créer une transaction.
// Prend tous les détails comme arguments.
Transaction::Transaction(const std::string& id, const std::string& clientId, TransactionType type,
                const std::string& cryptoName, double quantity, double unitPrice,
                double totalAmount, double fee, std::time_t timestamp_t, TransactionStatus status,
                const std::string& failureReason)
    // Liste d'initialisation : Assigne directement toutes les valeurs fournies.
    : id(id), clientId(clientId), type(type), cryptoName(cryptoName),
      quantity(quantity), unitPrice(unitPrice), totalAmount(totalAmount), fee(fee),
      timestamp_t(timestamp_t), status(status), failureReason(failureReason)
{
    // Convertit le timestamp C-style chargé en chrono::time_point.
    this->timestamp = std::chrono::system_clock::from_time_t(this->timestamp_t);

    // Log la transaction créée/chargée.
    std::stringstream ss_log;
    ss_log << "Transaction créée/chargée : ID=" << this->id
           << ", Client=" << this->clientId
           << ", Type=" << transactionTypeToString(this->type) // Utilise helper
           << ", Statut=" << transactionStatusToString(this->status); // Utilise helper
    // Ajoute la raison d'échec si le statut est FAILED.
    if (this->status == TransactionStatus::FAILED) { // Utilise le membre directement
        ss_log << ", Raison Échec='" << this->failureReason << "'";
    }
}


// --- Implémentation des Getters ---
// Retournent la valeur des membres correspondants. Simples accès, ne nécessitent pas de mutex.
const std::string& Transaction::getId() const { return id; }
const std::string& Transaction::getClientId() const { return clientId; }
TransactionType Transaction::getType() const { return type; }
const std::string& Transaction::getCryptoName() const { return cryptoName; }
double Transaction::getQuantity() const { return quantity; }
double Transaction::getUnitPrice() const { return unitPrice; }
double Transaction::getTotalAmount() const { return totalAmount; }
double Transaction::getFee() const { return fee; }
std::time_t Transaction::getTimestamp_t() const { return timestamp_t; }
std::chrono::system_clock::time_point Transaction::getTimestamp() const { return timestamp; }
TransactionStatus Transaction::getStatus() const { return status; } // Accède au membre 'status'
const std::string& Transaction::getFailureReason() const { return failureReason; }


// --- Implémentation des Getters Helpers ---

// Génère une description textuelle lisible de la transaction.
std::string Transaction::getDescription() const {
    std::stringstream ss;
    // Début de la description basé sur le type
    ss << transactionTypeToString(type) << " " // Utilise helper
       << std::fixed << std::setprecision(10) << quantity << " "
       << cryptoName; // Ou devise selon le type

    // Ajoute les détails de prix pour BUY/SELL.
    if (type == TransactionType::BUY || type == TransactionType::SELL) {
        ss << " at " << std::fixed << std::setprecision(10) << unitPrice << " USD"; // Supposant prix unitaire toujours en USD
    }

    // Ajoute le montant total et les frais.
    ss << " (Total: " << std::fixed << std::setprecision(10) << totalAmount << ", Fee: " << std::fixed << std::setprecision(10) << fee << ")";

    // La raison d'échec est incluse dans le log/CSV, pas toujours dans la description courte.

    return ss.str();
}

// Retourne le timestamp formaté en string ("YYYY-MM-DD HH:MM:SS"). Thread-safe.
std::string Transaction::getTimestampString() const {
    // Convertit le time_point en time_t pour utiliser les fonctions C standard de formatage.
    auto tt = std::chrono::system_clock::to_time_t(timestamp);

    // Utilise localtime_r pour la thread-safety (version réentrante) avec strftime.
    std::tm timeinfo_buffer;
    if (localtime_r(&tt, &timeinfo_buffer)) {
        char buffer[80]; // Buffer suffisant pour le format "YYYY-MM-DD HH:MM:SS\0"
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %X", &timeinfo_buffer); // %X est le format standard HH:MM:SS
        return buffer; // Retourne la string formatée.
    } else {
        LOG("Transaction::getTimestampString Erreur: localtime_r a échoué pour timestamp.", "ERROR");
        return "[TIMESTAMP_ERROR]"; // Retourne une string d'erreur.
    }
}


// --- Implémentation du Setter pour Status et FailureReason ---

// Modifie le statut de la transaction.
void Transaction::setStatus(TransactionStatus status) {
    this->status = status;
}

// Définit la raison de l'échec.
void Transaction::setFailureReason(const std::string& reason) {
    this->failureReason = reason;
}


// --- Implémentation des Méthodes Statiques de Persistance ---

// Log une transaction terminée dans un fichier CSV global. Thread-safe.
// Protégé par persistenceMutex.
void Transaction::logTransactionToCSV(const std::string &filename, const Transaction& tx_to_log) {
    std::lock_guard<std::mutex> lock(persistenceMutex); // Protège l'accès concurrentiel au fichier de log.

    std::ofstream logFile(filename, std::ios::app); // Ouvre le fichier en mode ajout à la fin.

    if (!logFile.is_open()) {
        LOG("Transaction::logTransactionToCSV Erreur: Impossible d'ouvrir log transaction CSV: " + filename + ". Erreur système: " + std::string(strerror(errno)), "ERROR");
        return; // Quitte si l'ouverture échoue.
    }

    // Vérification optionnelle : Écrire l'en-tête CSV si le fichier est vide (nouvelle création).
    // Nécessite un seekp pour vérifier la taille.
    logFile.seekp(0, std::ios::end); // Déplace le curseur à la fin du fichier
    std::streampos current_pos = logFile.tellp(); // Obtient la position

    if (current_pos == 0) { // Si la position est 0, le fichier était vide
         // Écrire l'en-tête. Assure-toi que l'en-tête correspond au format d'écriture ci-dessous.
         logFile << "ID,Client ID,Type,Crypto,Quantite,Prix Unitaire,Montant Total,Frais,Timestamp (Epoch),Timestamp (String),Statut,Description,Raison Echec\n";
         LOG("Transaction Fichier de log transaction CSV créé avec en-tête: " + filename, "INFO");
    }
     // Si le fichier n'était pas vide, tellp() est à la fin, on peut écrire directement.


    // Format CSV : ID,Client ID,Type,Crypto,Quantité,Prix Unitaire,Montant Total,Frais,Timestamp (Epoch),Timestamp (String),Statut,Description,Raison Echec
    // Utilise get...() pour accéder aux données de la transaction (passée par référence const).
    logFile << tx_to_log.getId() << ","
            << tx_to_log.getClientId() << ","
            << transactionTypeToString(tx_to_log.getType()) << "," // Helper
            << tx_to_log.getCryptoName() << ","
            << std::fixed << std::setprecision(10) << tx_to_log.getQuantity() << ","
            << std::fixed << std::setprecision(10) << tx_to_log.getUnitPrice() << ","
            << std::fixed << std::setprecision(10) << tx_to_log.getTotalAmount() << ","
            << std::fixed << std::setprecision(10) << tx_to_log.getFee() << ","
            << tx_to_log.getTimestamp_t() << "," // Timestamp Epoch.
            << tx_to_log.getTimestampString() << "," // Timestamp formaté (getter helper).
            << transactionStatusToString(tx_to_log.getStatus()) << "," // Statut en string (Helper).
            << "\"" << tx_to_log.getDescription() << "\"," // Description, potentiellement avec espaces/virgules, entre guillemets.
            << "\"" << tx_to_log.getFailureReason() << "\"" // Raison d'échec, entre guillemets.
            << "\n"; // Nouvelle ligne.

    // Pas besoin de log de succès ici, ça serait trop verbeux.

    logFile.close(); // Ferme le fichier. Le contenu est flushé ici.

    // Vérification optionnelle après fermeture pour les erreurs d'écriture différées.
    if (logFile.fail()) {
         LOG("Transaction::logTransactionToCSV Erreur: Échec écriture/fermeture log CSV: " + filename + ". Erreur système: " + std::string(strerror(errno)), "ERROR");
    }
}

// Charge la dernière valeur du compteur de transactions depuis un fichier (lors de l'initialisation). Thread-safe.
// Protégé par persistenceMutex (accès fichier) et counterMutex (mise à jour compteur statique).
void Transaction::loadCounter(const std::string& filename) {
    std::lock_guard<std::mutex> lock_file(persistenceMutex); // Protège l'accès au fichier du compteur.

    std::ifstream counterFile(filename);
    int loadedCounter = 0; // Valeur par défaut si fichier non trouvé ou vide.

    if (counterFile.is_open()) {
        counterFile >> loadedCounter; // Tente de lire la valeur.
        // Vérifie si la lecture a échoué MAIS que ce n'est pas juste la fin du fichier (fichier vide).
        if (counterFile.fail() && !counterFile.eof()) {
            LOG("Transaction::loadCounter Erreur: Échec lecture compteur depuis fichier: " + filename + ". Erreur système: " + std::string(strerror(errno)) + ". Utilisation compteur 0.", "ERROR");
            loadedCounter = 0; // Réinitialise si la lecture a vraiment échoué.
        } else if (counterFile.eof() && loadedCounter == 0) {
             // Fichier vide ou ne contenant que 0. C'est valide.
             LOG("Transaction::loadCounter Fichier compteur vide ou contenant 0. Compteur démarrera à 0.", "INFO");
        } else if (!counterFile.fail()) {
             // Lecture réussie (et pas seulement EOF).
             LOG("Transaction::loadCounter Compteur chargé: " + std::to_string(loadedCounter) + " depuis " + filename, "INFO");
        }
        counterFile.close();
    } else {
        // Le fichier n'existe pas. Le compteur démarrera à 0 (initialisation par défaut).
        LOG("Transaction::loadCounter Fichier compteur non trouvé: " + filename + ". Le compteur démarrera à 0.", "INFO");
    }

    // Met à jour le compteur statique global avec la valeur chargée.
    { // Utilise un bloc pour limiter la portée du lock sur counterMutex.
         std::lock_guard<std::mutex> lock_counter(counterMutex);
         counter = loadedCounter; // Met à jour le compteur statique.
    } // counterMutex libéré.
}

// Sauvegarde la valeur actuelle du compteur de transactions dans un fichier. Thread-safe.
// Protégé par persistenceMutex (accès fichier) et counterMutex (accès compteur).
void Transaction::saveCounter(const std::string& filename) {
    int currentCounterValue;
    { // Bloc pour limiter la portée de counterMutex.
         std::lock_guard<std::mutex> lock_counter(counterMutex);
         currentCounterValue = counter; // Lire le compteur sous sa protection.
    } // counterMutex libéré.

    std::lock_guard<std::mutex> lock_file(persistenceMutex); // Protège l'accès au fichier du compteur.

    // Ouvre le fichier en mode écriture avec troncature (std::ios::trunc). Écrase le contenu précédent.
    std::ofstream counterFile(filename, std::ios::trunc);

    if (counterFile.is_open()) {
        counterFile << currentCounterValue; // Écrit la valeur.
        counterFile.close(); // Ferme le fichier. Le contenu est flushé.
        // Vérification optionnelle pour les erreurs d'écriture différées.
        if (counterFile.fail()) {
             LOG("Transaction::saveCounter Erreur: Échec écriture/fermeture fichier compteur: " + filename + ". Erreur système: " + std::string(strerror(errno)), "ERROR");
             return;
        }
        LOG("Transaction::saveCounter Compteur sauvegardé: " + std::to_string(currentCounterValue) + " vers " + filename, "INFO");
    } else {
        // L'ouverture a échoué.
        LOG("Transaction::saveCounter Erreur: Impossible d'ouvrir fichier compteur pour sauvegarde: " + filename + ". Erreur système: " + std::string(strerror(errno)), "ERROR");
    }
}