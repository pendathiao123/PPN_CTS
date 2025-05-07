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
#include <chrono> 
#include <ctime>  

// Forward declaration de ClientSession pour éviter une inclusion complète ici
class ClientSession;

// Enums pour les types de requêtes
enum class RequestType { UNKNOWN_REQUEST, BUY, SELL };

// Déclarations des fonctions utilitaires pour convertir les enums en string et vice-versa
// L'implémentation sera dans TransactionQueue.cpp
std::string requestTypeToString(RequestType type);


// Structure représentant une requête de transaction entrante (avant traitement par la TQ)
struct TransactionRequest {
    std::string clientId;
    RequestType type;
    std::string cryptoName;
    double quantity;

    // Constructeur pour créer une requête initiale
    TransactionRequest(const std::string& client_id, RequestType req_type, const std::string& crypto_name, double qty)
        : clientId(client_id), type(req_type), cryptoName(crypto_name), quantity(qty)
    {}
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

#endif




/*
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
#include <vector>
#include <chrono> 
#include <ctime>  

// Forward declaration de ClientSession pour éviter une inclusion complète ici
class ClientSession;

// Enums pour les types de requêtes
enum class RequestType { UNKNOWN_REQUEST, BUY, SELL };

// Déclarations des fonctions utilitaires pour convertir les enums en string et vice-versa
// L'implémentation sera dans TransactionQueue.cpp
std::string requestTypeToString(RequestType type);

// Structure représentant une requête de transaction entrante (avant traitement par la TQ)
struct TransactionRequest {
    std::string clientId;
    RequestType type;
    std::string cryptoName;
    double quantity;

    // Constructeur par défaut
    TransactionRequest() 
        : clientId(""), type(RequestType::UNKNOWN_REQUEST), cryptoName(""), quantity(0.0) {}

    // Constructeur pour créer une requête initiale
    TransactionRequest(const std::string& client_id, RequestType req_type, const std::string& crypto_name, double qty)
        : clientId(client_id), type(req_type), cryptoName(crypto_name), quantity(qty) {}
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
    void process(); // Fonction exécutée par chaque thread du pool
    void processRequest(const TransactionRequest& request); // Traite une requête spécifique

    std::queue<TransactionRequest> queue; // File de requêtes
    std::mutex mtx; // Mutex pour protéger la file
    std::condition_variable cv; // Condition variable pour notifier les threads
    std::atomic<bool> running; // Indique si la queue est en cours d'exécution

    std::vector<std::thread> workers; // Pool de threads
    int threadPoolSize; // Taille du pool de threads

    std::unordered_map<std::string, std::weak_ptr<ClientSession>> sessionMap; // Map des sessions clients
    std::mutex sessionMapMtx; // Mutex pour protéger sessionMap
};

// Déclaration de l'instance globale de la TransactionQueue.
extern TransactionQueue txQueue;

#endif

*/