#ifndef BOT_H
#define BOT_H

#include <string>
#include <vector>
#include <memory>   
#include <thread>   
#include <atomic>   
#include <mutex>   
#include <chrono>   

#include "Global.h"     
#include "Wallet.h"     
#include "Transaction.h" 

// Forward declaration de ClientSession, l'include n'est pas nécessaire ici.
class ClientSession;

// --- Classe Bot : Gère la stratégie de trading pour un client ---
class Bot {
public:
    // Constructeur
    Bot(const std::string& clientId, int period, double k,
        std::shared_ptr<Wallet> wallet,
        std::weak_ptr<ClientSession> session_ptr);

    // Destructeur
    ~Bot();

    // --- Méthodes de gestion du thread ---
    void start(); // Démarre le thread principal du bot
    void stop();  // Signale au thread de s'arrêter et attend sa fin

    // --- Getters (Thread-safe car utilisent botMutex en interne, sauf clientId et Wallet) ---
    PositionState getCurrentState() const;
    double getEntryPrice() const;
    const std::string& getClientId() const;
    std::shared_ptr<Wallet> getClientWallet() const;

    // --- Méthodes principales de logique/interaction (Thread-safe) ---
    TradingAction processLatestPrice(); // Traite le nouveau prix et décide de l'action
    void notifyTransactionCompleted(const Transaction& tx); // Notifie le bot d'une transaction complétée

private:
    // La méthode qui sera exécutée dans le thread du bot
    void tradeLoop();

    // --- Membres du bot ---
    std::string clientId;
    int bollingerPeriod;        // Période pour la SMA et l'écart-type
    double bollingerK;          // Facteur multiplicateur de l'écart-type

    std::vector<double> priceHistoryForCalculation; // Historique des prix pour les calculs
    PositionState currentState; // État actuel de la position
    double entryPrice;          // Prix d'entrée de la position actuelle

    std::shared_ptr<Wallet> clientWallet;           // Pointeur vers le Wallet associé
    std::weak_ptr<ClientSession> clientSessionPtr; // Pointeur faible vers la ClientSession

    // --- Membres pour la gestion du thread et de la concurrence ---
    std::thread botThread;          // Le thread d'exécution du bot
    std::atomic<bool> running;      // Flag atomique pour signaler l'arrêt du thread
    mutable std::mutex botMutex;    // Mutex pour protéger l'accès concurrent aux membres

    // Configuration de la fréquence d'exécution de la logique du bot
    const std::chrono::seconds tradeInterval = std::chrono::seconds(5);

    // --- Méthodes internes d'aide (calculs) ---
    // Statiques si elles n'utilisent pas les membres de l'objet.
    static double calculateSMA(const std::vector<double>& data);
    static double calculateStdDev(const std::vector<double>& data, double sma);
    BollingerBands calculateBands() const; // Utilise l'historique protégé par mutex
};

#endif