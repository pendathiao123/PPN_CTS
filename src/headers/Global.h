#ifndef GLOBAL_H
#define GLOBAL_H

#include <string>
#include <vector>
#include <atomic> 
#include <thread>
#include <mutex> 
#include <cstddef> 


// Enums pour les devises supportées
enum class Currency { UNKNOWN, USD, SRD_BTC };

// Enums pour les types de transaction
enum class TransactionType { UNKNOWN, BUY, SELL };

// Enum pour le statut d'une transaction
enum class TransactionStatus { UNKNOWN, PENDING, COMPLETED, FAILED };

enum class PositionState {
    NONE,  // Aucune position ouverte.
    LONG,  // Position longue (acheté).
    SHORT,  // Position courte (vendu à découvert).
    UNKNOWN
};

enum class TradingAction {
    HOLD,        // Ne rien faire.
    BUY,         // Décision d'acheter.
    SELL,        // Décision de vendre.
    CLOSE_LONG,  // Clôturer position LONG.
    CLOSE_SHORT,  // Clôturer position SHORT.
    UNKNOWN
};

struct BollingerBands {
    double middleBand; // Moyenne mobile.
    double upperBand;  // Bande supérieure.
    double lowerBand;  // Bande inférieure.
};

// Déclarations des fonctions utilitaires pour convertir les enums en string et vice-versa
std::string currencyToString(Currency currency);
Currency stringToCurrency(const std::string& str);
std::string transactionTypeToString(TransactionType type);
TransactionType stringToTransactionType(const std::string& str);
std::string transactionStatusToString(TransactionStatus status);
TransactionStatus stringToTransactionStatus(const std::string& str);

std::string positionStateToString(PositionState ps);
std::string tradingActionToString(TradingAction ta); 

// Constante pour le pourcentage d'investissement du bot
const double BOT_INVESTMENT_PERCENTAGE = 10.0;

// --- Classe Global : Fournisseur de données et services globaux (gestion des prix, threads) ---
// Cette classe utilise principalement des membres et méthodes statiques.
class Global {
private:
    // --- Membres statiques pour la gestion du thread de génération de prix ---
    static std::atomic<bool> stopRequested; // Flag atomique pour signaler l'arrêt au thread (thread-safe par nature atomique).
    static std::thread priceGenerationWorker; // L'objet thread qui exécute la boucle de génération de prix.

    // --- Membres statiques pour la dernière valeur de prix SRD-BTC et son mutex ---
    // Le prix est mutable et partagé entre le thread de génération et les threads qui l'appellent (Bot, etc.).
    static std::mutex srdMutex; // Mutex pour protéger l'accès (lecture/écriture) à lastSRDBTCValue.
    static double lastSRDBTCValue; // La dernière valeur de prix connue - Protégée par srdMutex.

    // --- Membres statiques pour le buffer circulaire des prix historiques et son mutex ---
    // Le buffer et son index sont modifiés/lus par le thread de génération et lus par les appelants (Bot::calculateBands par ex).
    static std::vector<double> ActiveSRDBTC; // Buffer circulaire des prix - Protégé par bufferMutex.
    static std::atomic<int> activeIndex; // Index de prochaine écriture dans le buffer (utilisation atomique suffisante pour l'index).
    static const int MAX_VALUES_PER_DAY = 5760; // Capacité max du buffer (constant).
    static std::mutex bufferMutex; // Mutex pour protéger l'accès (lecture/écriture) au buffer ActiveSRDBTC (l'index est atomique).


    // --- Méthodes privées (implémentations internes) ---
    // Callback pour recevoir les données (ex: depuis une requête réseau, typique de libcurl) - Doit être statique.
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
    // Fonction exécutée dans le thread de génération de prix.
    static void generate_SRD_BTC_loop_impl();

public:
    // --- Méthodes de gestion du thread de génération de prix ---
    static void startPriceGenerationThread(); // Démarre le thread.
    static void stopPriceGenerationThread(); // Signale l'arrêt et attend la fin du thread.

    // --- Méthodes d'accès aux prix (Doivent être thread-safe dans .cpp) ---
    // L'implémentation de ces méthodes doit utiliser les mutex (srdMutex et bufferMutex)
    // pour s'assurer que l'accès aux données partagées est sécurisé.
    static double getPrice(const std::string& currency); // Obtient dernier prix (Thread-safe)
    static double getPreviousPrice(const std::string& currency, int secondsBack); // Obtient prix historique (Thread-safe)

    // --- Méthodes d'accès aux flags (pour vérifier l'état global) ---
    static std::atomic<bool>& getStopRequested(); // Retourne une référence au flag d'arrêt (accès atomique thread-safe).
};

#endif