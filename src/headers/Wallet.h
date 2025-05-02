#ifndef WALLET_H
#define WALLET_H

// Includes standards nécessaires
#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <memory>
#include <filesystem> // Pour la gestion des chemins de fichiers

// Includes des classes/composants liés
#include "Transaction.h" // Déclaration de la classe Transaction et de l'enum Currency, etc.


// --- Classe Wallet : Gère les soldes, l'historique et la persistance pour un client ---
/**
 * Cette classe est conçue pour être thread-safe.
 */
class Wallet {
private:
    // Membres d'identification et de chemin
    std::string clientId;
    std::string dataDirectoryPath;
    std::string walletFilePath;

    // Données mutables du portefeuille - PROTÉGÉES par walletMutex
    std::map<Currency, double> balances; // Soldes par devise
    std::vector<Transaction> transactionHistory; // Historique des transactions

    // Mutex pour PROTÉGER l'accès CONCURRENT aux données mutables (balances, transactionHistory)
    // 'mutable' permet de locker/unlocker ce mutex dans les méthodes marquées 'const' (comme getBalance ou saveToFile si implémenté ainsi).
    mutable std::mutex walletMutex;

    // --- Méthodes privées (gestion interne des fichiers/répertoires) ---
    std::string generateWalletFilePath(const std::string& dataDirPath) const; // Génère chemin fichier
    bool ensureWalletsDirectoryExists() const; // Assure répertoire existe

public:
    // --- Constructeur et destructeur ---
    // Le constructeur initialise et charge, le destructeur sauvegarde automatiquement.
    Wallet(const std::string& clientId, const std::string& dataDirPath); // Constructeur
    ~Wallet(); // Destructeur (Sauvegarde automatique)

    // --- Méthodes de gestion des soldes (DOIVENT être thread-safe dans .cpp en utilisant walletMutex) ---
    double getBalance(Currency currency) const; // Retourne solde (Thread-safe)

    // Prend la devise et le montant à mettre à jour
    void updateBalance(Currency currency, double amount);

    // --- Méthodes de gestion de l'historique (DOIVENT être thread-safe dans .cpp en utilisant walletMutex) ---
    void addTransaction(const Transaction& tx); // Ajoute transaction (Thread-safe)
    std::vector<Transaction> getTransactionHistory() const; // Retourne historique (Thread-safe, copie pour sécurité)

    // --- Méthodes de persistance (DOIVENT être thread-safe dans .cpp en utilisant walletMutex) ---
    bool loadFromFile(); // Charge données depuis fichier (Thread-safe, modifie l'état)
    bool saveToFile() const; // Sauvegarde données dans fichier (Thread-safe, lecture de l'état)

    // --- Méthode CRUCIALE pour verrouiller le Wallet depuis l'extérieur (par la TQ) ---
    std::mutex& getMutex();

    // --- Getter simple (sur membre constant) ---
    const std::string& getClientId() const; // Retourne l'ID client
};

#endif // WALLET_H