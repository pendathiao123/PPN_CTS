#ifndef TRANSACTIONQUEUE_H
#define TRANSACTIONQUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <string>
#include <memory> 
#include <unordered_map>

#include "Logger.h"

// Déclaration anticipée de la classe BotSession car la queue y maintient des pointeurs faibles
class BotSession;

// Enumération des types de requêtes de transaction (peut rester ici ou être globale si utilisée ailleurs)
enum class RequestType {
    BUY,
    SELL
};

// Structure représentant une requête de transaction soumise par un bot
// Elle est ajoutée à la file d'attente.
struct TransactionRequest {
    std::string clientId; // L'ID du client/bot qui a soumis la requête
    RequestType type;     // Le type de l'opération demandée (BUY ou SELL)
    std::string cryptoName; // La cryptomonnaie concernée
    double quantity;      // Pourcentage du capital (BUY) ou de la crypto (SELL) à utiliser

    // Constructeur
    TransactionRequest(const std::string& id, RequestType t, const std::string& name, double qty)
        : clientId(id), type(t), cryptoName(name), quantity(qty) {}
};


class TransactionQueue {
private:
    std::queue<TransactionRequest> queue; // La file d'attente des requêtes
    std::mutex mtx;                     // Mutex pour protéger l'accès à la file
    std::condition_variable cv;         // Variable de condition pour signaler au thread worker qu'il y a des requêtes
    std::thread worker;                 // Le thread qui traite les requêtes
    std::atomic<bool> running;          // Flag atomique pour contrôler l'exécution du thread

    // Map pour stocker les pointeurs faibles vers les sessions BotSession actives.
    // Utilisée pour accéder à la BotSession (et donc au Bot) de manière sécurisée
    // depuis le thread worker. L'ID client est la clé.
    std::unordered_map<std::string, std::weak_ptr<BotSession>> sessionMap;
    std::mutex sessionMapMtx; // Mutex pour protéger l'accès à sessionMap

    // Fonction exécutée par le thread worker. Elle traite les requêtes de la file.
    void process();

public:
    // Constructeur et Destructeur
    TransactionQueue();
    ~TransactionQueue();

    // Enregistre une BotSession auprès de la queue. Le pointeur faible est stocké.
    // Appelée par le Server (ou BotSession si elle a un pointeur vers la queue) quand une BotSession est créée/active.
    void registerSession(const std::shared_ptr<BotSession>& session);

    // Désenregistre une BotSession de la queue. Supprime le pointeur faible.
    // Appelée par le Server (ou BotSession) quand une session se termine/déconnecte.
    void unregisterSession(const std::string& clientId);

    // Ajoute une requête à la file d'attente. Appelé par les Bots.
    void addRequest(const TransactionRequest& request);

    // Démarre le thread de traitement de la queue. Appelé par le Server.
    void start();

    // Demande l'arrêt du thread de traitement et attend sa fin. Appelé par le Server.
    void stop();
};

// Déclaration de la file de transactions globale
extern TransactionQueue txQueue;


#endif