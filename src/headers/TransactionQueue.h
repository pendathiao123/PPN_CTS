#ifndef TRANSACTION_QUEUE_H
#define TRANSACTION_QUEUE_H

#include "Transaction.h"

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <chrono> // Pour std::chrono::system_clock::time_point
#include <ctime>  // Pour std::time_t


// Forward declaration de ClientSession pour éviter une inclusion complète ici
class ClientSession;

// Enums pour les types de requêtes
// Retrait de DEPOSIT et WITHDRAW car non utilisés pour les requêtes client
enum class RequestType { UNKNOWN_REQUEST, BUY, SELL };

// Déclarations des fonctions utilitaires pour convertir les enums en string et vice-versa
// L'implémentation sera dans TransactionQueue.cpp
std::string requestTypeToString(RequestType type); // Sera implémenté dans TQ.cpp
// std::string transactionStatusToString(TransactionStatus status); // Est déclaré et implémenté dans Transaction.h


// Structure représentant une requête de transaction entrante (avant traitement par la TQ)
struct TransactionRequest {
    std::string clientId;
    RequestType type; // Maintenant seulement BUY, SELL, UNKNOWN_REQUEST
    std::string cryptoName;
    double quantity;
    /* Champs qui seront remplis par la TQ après traitement:
    std::string transactionId;
    double unitPrice; // Prix effectif d'exécution pour BUY/SELL
    double totalAmount; // quantité * unitPrice (ou juste quantity pour DEPOSIT/WITHDRAW - ce cas sera retiré)
    double fee;
    std::chrono::system_clock::time_point timestamp;
    std::time_t timestamp_t;
    TransactionStatus status; // Commence à PENDING, termine à COMPLETED ou FAILED
    std::string failureReason;
    */ 

    // Constructeur pour créer une requête initiale
    TransactionRequest(const std::string& client_id, RequestType req_type, const std::string& crypto_name, double qty)
        : clientId(client_id), type(req_type), cryptoName(crypto_name), quantity(qty)
    {}

    // TODO: Ajouter un constructeur par défaut si nécessaire (ex: pour std::queue, bien que TransactionRequest soit CopyConstructible/Assignable par défaut)
};


// Déclaration de la classe TransactionQueue
class TransactionQueue {
public:
    TransactionQueue();
    ~TransactionQueue();

    void start();
    void stop();

    void addRequest(const TransactionRequest& request); // Thread-safe

    void registerSession(const std::shared_ptr<ClientSession>& session); // Thread-safe
    void unregisterSession(const std::string& clientId); // Thread-safe

private:
    void process();
    void processRequest(const TransactionRequest& request);

    std::queue<TransactionRequest> queue;
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> running;
    std::thread worker;

    std::unordered_map<std::string, std::weak_ptr<ClientSession>> sessionMap;
    std::mutex sessionMapMtx;
};

// Déclaration de l'instance globale de la TransactionQueue.
extern TransactionQueue txQueue;


#endif // TRANSACTION_QUEUE_H