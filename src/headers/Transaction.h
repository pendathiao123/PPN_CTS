#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <string>
#include <chrono>
#include <ctime>
#include <vector>
#include <mutex>

// Enums pour les devises supportées
enum class Currency { UNKNOWN, USD, SRD_BTC };

// Enums pour les types de transaction
// Retrait de DEPOSIT et WITHDRAW
enum class TransactionType { UNKNOWN, BUY, SELL };

// Enum pour le statut d'une transaction
// INCLUT UNKNOWN pour la robustesse (parsing, état initial si besoin)
enum class TransactionStatus { UNKNOWN, PENDING, COMPLETED, FAILED };


// Déclarations des fonctions utilitaires pour convertir les enums en string et vice-versa
// Les implémentations seront dans Transaction.cpp
std::string currencyToString(Currency currency);
Currency stringToCurrency(const std::string& str);
std::string transactionTypeToString(TransactionType type);
TransactionType stringToTransactionType(const std::string& str);
std::string transactionStatusToString(TransactionStatus status);
TransactionStatus stringToTransactionStatus(const std::string& str);


// Structure représentant une Transaction complète (résultat d'une requête traitée ou chargée)
struct Transaction {
private:
    std::string id; // ID unique de la transaction
    std::string clientId; // ID du client concerné
    TransactionType type; // Type de la transaction (BUY, SELL, UNKNOWN)
    std::string cryptoName; // Nom de la crypto concernée (ex: "SRD-BTC")
    double quantity; // Quantité de crypto (achetée, vendue, etc.)
    double unitPrice; // Prix unitaire au moment de la transaction (pour BUY/SELL)
    double totalAmount; // Montant total (quantity * unitPrice, ou montant dépôt/retrait)
    double fee; // Frais de transaction
    std::chrono::system_clock::time_point timestamp; // Timestamp de la transaction (en time_point)
    std::time_t timestamp_t; // Timestamp de la transaction (en time_t)
    TransactionStatus status; // Statut final de la transaction (COMPLETED, FAILED, UNKNOWN)
    std::string failureReason; // Raison de l'échec si applicable

    // Membres statiques pour la génération d'ID et la persistance du compteur (définis dans Transaction.cpp).
    static int counter;
    static std::mutex counterMutex;
    static std::mutex persistenceMutex;

    // Méthode privée statique pour générer l'ID (appelée par generateNewIdString).
    static std::string generateUniqueId();


public: // <<< SECTION PUBLIQUE

    // Constructeur (utilisé par TQ ou chargement)
    // Prend tous les détails comme arguments.
    Transaction(const std::string& id, const std::string& clientId, TransactionType type,
                const std::string& cryptoName, double quantity, double unitPrice,
                double totalAmount, double fee, std::time_t timestamp_t, TransactionStatus status,
                const std::string& failureReason = "");

    // Getters (const car ils ne modifient pas l'objet)
    const std::string& getId() const;
    const std::string& getClientId() const;
    TransactionType getType() const;
    const std::string& getCryptoName() const;
    double getQuantity() const;
    double getUnitPrice() const;
    double getTotalAmount() const;
    double getFee() const;
    std::time_t getTimestamp_t() const;
    std::chrono::system_clock::time_point getTimestamp() const;
    TransactionStatus getStatus() const;
    const std::string& getFailureReason() const;

    // Setters pour Status et FailureReason
    void setStatus(TransactionStatus status);
    void setFailureReason(const std::string& reason);

    // Génère une description textuelle lisible de la transaction
    std::string getDescription() const;

    // Retourne le timestamp formaté en string ("YYYY-MM-DD HH:MM:SS"). Thread-safe.
    std::string getTimestampString() const; // Getter helper

    // Méthode statique pour générer un nouvel ID unique (thread-safe)
    static std::string generateNewIdString();

    // Méthode statique pour logguer une transaction dans un fichier CSV global (thread-safe)
    static void logTransactionToCSV(const std::string& filePath, const Transaction& tx);

    // Méthodes statiques pour la persistance du compteur (appelées par le serveur)
    static void loadCounter(const std::string& filename); // <<< DÉCLARATION AJOUTÉE
    static void saveCounter(const std::string& filename); // <<< DÉCLARATION AJOUTÉE

    // TODO: Ajouter d'autres méthodes si nécessaire (ex: validation interne)
};

#endif // TRANSACTION_H