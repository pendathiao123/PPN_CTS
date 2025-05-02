#ifndef BOT_H
#define BOT_H

// Includes standards nécessaires
#include <string>
#include <vector>
#include <memory>
#include <mutex>

// Includes des classes/composants liés
#include "Global.h"     // Accès aux prix globaux
#include "Wallet.h"     // Portefeuille du client associé
#include "Transaction.h" // Utilisé dans les notifications de transaction

// --- Enums pour l'état et les actions du bot ---
enum class PositionState {
    NONE,  // Aucune position ouverte.
    LONG,  // Position longue (acheté).
    SHORT  // Position courte (vendu à découvert).
};

enum class TradingAction {
    HOLD,        // Ne rien faire.
    BUY,         // Décision d'acheter.
    SELL,        // Décision de vendre.
    CLOSE_LONG,  // Clôturer position LONG.
    CLOSE_SHORT  // Clôturer position SHORT.
};

// --- Structure pour les Bandes de Bollinger ---
struct BollingerBands {
    double middleBand; // Moyenne mobile.
    double upperBand;  // Bande supérieure.
    double lowerBand;  // Bande inférieure.
};

// --- Classe Bot : Gère la stratégie de trading pour un client ---
class Bot {
private:
    // Membres de configuration (constants après construction)
    std::string clientId;
    int bollingerPeriod;
    double bollingerK;

    // État mutable du bot - PROTÉGÉ par botMutex
    PositionState currentState; // État de la position actuelle
    double entryPrice;        // Prix d'entrée de la position

    // Pointeur vers le portefeuille associé (shared_ptr lui-même est thread-safe pour copie/assignation,
    // mais l'accès au Wallet pointé doit utiliser ses méthodes thread-safe)
    std::shared_ptr<Wallet> clientWallet;

    // Historique local des prix - PROTÉGÉ par botMutex
    std::vector<double> priceHistoryForCalculation;

    // Mutex pour PROTÉGER l'accès CONCURRENT aux membres mutables ci-dessus (currentState, entryPrice, priceHistoryForCalculation)
    // 'mutable' permet de locker/unlocker ce mutex dans les méthodes marquées 'const'.
    mutable std::mutex botMutex;

    // --- Méthodes internes d'aide (calculs) ---
    static double calculateSMA(const std::vector<double>& data); // Calcule la moyenne mobile
    static double calculateStdDev(const std::vector<double>& data, double sma); // Calcule l'écart-type
    BollingerBands calculateBands() const; // Calcule les Bandes de Bollinger (utilise l'historique protégé par mutex)

public:
    // Constructeur et destructeur
    Bot(const std::string& clientId, int bollingerPeriod, double bollingerK, std::shared_ptr<Wallet> wallet); // Constructeur
    ~Bot(); // Destructeur

    // --- Getters ---
    // Les getters des membres mutables (état/prix) DOIVENT être thread-safe dans leur implémentation (.cpp)
    // en utilisant botMutex. Les getters des membres constants n'ont pas besoin de mutex.
    PositionState getCurrentState() const; // Retourne l'état de position (Thread-safe)
    double getEntryPrice() const; // Retourne le prix d'entrée (Thread-safe)
    const std::string& getClientId() const; // Retourne l'ID client (Constant)
    std::shared_ptr<Wallet> getClientWallet() const; // Retourne le shared_ptr du portefeuille (Pointeur lui-même atomique)

    // --- Méthodes principales d'interaction ---
    // Ces méthodes modifient l'état mutable du bot et DOIVENT être thread-safe dans leur implémentation (.cpp)
    // en utilisant botMutex pour protéger l'accès aux membres modifiés/lus.
    TradingAction processLatestPrice(); // Traite le nouveau prix et décide de l'action (Thread-safe)
    void notifyTransactionCompleted(const Transaction& tx); // Notifie le bot d'une transaction complétée (Thread-safe)
};

#endif // BOT_H