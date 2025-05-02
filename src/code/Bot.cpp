#include "../headers/Bot.h"
#include "../headers/Global.h"
#include "../headers/Logger.h"
#include "../headers/Transaction.h"
#include "../headers/Wallet.h" 

#include <iostream>
#include <cmath>     
#include <numeric>    
#include <algorithm>  
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <sstream>    
#include <iomanip>    


// --- Implémentation des helpers de calcul ---

// Calcule la moyenne mobile simple (SMA).
double Bot::calculateSMA(const std::vector<double>& data) {
    if (data.empty()) {
        return 0.0;
    }
    double sum = std::accumulate(data.begin(), data.end(), 0.0);
    return sum / static_cast<double>(data.size());
}

// Calcule l'écart-type.
double Bot::calculateStdDev(const std::vector<double>& data, double sma) {
    if (data.size() <= 1) {
        return 0.0;
    }
    double variance_sum = 0.0;
    for (double price : data) {
        variance_sum += std::pow(price - sma, 2);
    }
    return std::sqrt(variance_sum / static_cast<double>(data.size())); // Population standard deviation
}

// Calcule les Bandes de Bollinger en utilisant l'historique interne.
// Appelé par `processLatestPrice` (qui gère déjà le mutex).
BollingerBands Bot::calculateBands() const {
    // Pas de lock_guard ici car l'appelant (processLatestPrice) verrouille.

    size_t required_size = static_cast<size_t>(bollingerPeriod);
    if (priceHistoryForCalculation.size() < required_size) {
        // Pas assez de données.
        return {0.0, 0.0, 0.0};
    }

    size_t start_index = priceHistoryForCalculation.size() - required_size;
    std::vector<double> relevant_prices(
        priceHistoryForCalculation.begin() + start_index,
        priceHistoryForCalculation.end()
    );

    double sma = calculateSMA(relevant_prices);
    double stddev = calculateStdDev(relevant_prices, sma);

    return {
        sma, // Middle Band
        sma + bollingerK * stddev, // Upper Band
        sma - bollingerK * stddev  // Lower Band
    };
}


// --- Constructeur ---
Bot::Bot(const std::string& id, int period, double k, std::shared_ptr<Wallet> wallet)
    : clientId(id),
      bollingerPeriod(period),
      bollingerK(k),
      currentState(PositionState::NONE), // Commence sans position
      entryPrice(0.0),
      clientWallet(wallet)
{
    // Validation basique des paramètres Bollinger
    if (bollingerPeriod <= 0) {
         LOG("Bot " + clientId + " - Avertissement : Période Bollinger invalide (" + std::to_string(bollingerPeriod) + "). Utilisation par défaut 20.", "WARNING");
         bollingerPeriod = 20;
    }
     if (bollingerK <= 0 || !std::isfinite(bollingerK)) {
         std::stringstream ss_log;
         ss_log << "Bot " << clientId << " - Avertissement : Facteur K Bollinger invalide (" << std::fixed << std::setprecision(2) << bollingerK << "). Utilisation par défaut 2.0.";
         LOG(ss_log.str(), "WARNING");
         bollingerK = 2.0;
     }

    LOG("Bot créé pour client ID : " + clientId +
        " avec période Bollinger = " + std::to_string(bollingerPeriod) +
        " et K = " + std::to_string(bollingerK), "INFO");
}

// --- Destructeur ---
Bot::~Bot() {
    LOG("Bot détruit pour client ID : " + clientId, "INFO");
}

// --- Getters (thread-safe) ---

// Retourne l'état actuel de la position. Protégé par mutex.
PositionState Bot::getCurrentState() const {
    std::lock_guard<std::mutex> lock(botMutex);
    return currentState;
}

// Retourne le prix d'entrée. Protégé par mutex.
double Bot::getEntryPrice() const {
    std::lock_guard<std::mutex> lock(botMutex);
    return entryPrice;
}

// Retourne l'ID client (membre const).
const std::string& Bot::getClientId() const {
    return clientId;
}

// Retourne le shared_ptr du Wallet (le pointeur lui-même est atomique).
std::shared_ptr<Wallet> Bot::getClientWallet() const {
    return clientWallet;
}


// --- Implémentation des méthodes principales ---

// Traite le dernier prix, met à jour l'historique, calcule les indicateurs, décide de l'action.
// Thread-safe (appelé par ClientSession). Protégé par mutex.
TradingAction Bot::processLatestPrice() {
    // Protège l'accès aux membres mutables (historique, état, prix d'entrée)
    std::lock_guard<std::mutex> lock(botMutex);

    LOG("Bot " + clientId + " - Traitement du dernier prix...", "INFO");

    // 1. Obtenir le dernier prix.
    double latestPrice = Global::getPrice("SRD-BTC"); // Accès thread-safe à Global
    if (latestPrice <= 0 || !std::isfinite(latestPrice)) {
        std::stringstream ss_log;
        ss_log << "Bot " << clientId << " - Avertissement: Prix invalide (" << std::fixed << std::setprecision(10) << latestPrice << "). HOLD.";
        LOG(ss_log.str(), "WARNING");
        return TradingAction::HOLD;
    }

    // 2. Mettre à jour et limiter l'historique.
    priceHistoryForCalculation.push_back(latestPrice);
    size_t max_history_size = static_cast<size_t>(bollingerPeriod) * 2;
    if (priceHistoryForCalculation.size() > max_history_size) {
        priceHistoryForCalculation.erase(
            priceHistoryForCalculation.begin(),
            priceHistoryForCalculation.begin() + (priceHistoryForCalculation.size() - max_history_size)
        );
        // LOG("Bot " + clientId + " - Historique tronqué. Taille: " + std::to_string(priceHistoryForCalculation.size()), "DEBUG"); // Debug log optionnel
    }

    // 3. Calculer les Bandes de Bollinger.
    size_t required_size = static_cast<size_t>(bollingerPeriod);
    if (priceHistoryForCalculation.size() < required_size) {
        LOG("Bot " + clientId + " - Pas assez de données pour Bandes (" +
            std::to_string(priceHistoryForCalculation.size()) + "/" + std::to_string(bollingerPeriod) + "). HOLD.", "INFO");
        return TradingAction::HOLD;
    }

    BollingerBands bands = calculateBands(); // Appelle calculateBands (protégé par le même lock)
    std::stringstream ss_log_bands;
    ss_log_bands << "Bot " << clientId << " - Prix : " << std::fixed << std::setprecision(10) << latestPrice
                 << ", Bandes (M: " << std::fixed << std::setprecision(10) << bands.middleBand
                 << ", U: " << std::fixed << std::setprecision(10) << bands.upperBand
                 << ", L: " << std::fixed << std::setprecision(10) << bands.lowerBand << ")";
    LOG(ss_log_bands.str(), "INFO");


    // --- 4. Logique de Trading (Bollinger simple) ---
    TradingAction action = TradingAction::HOLD;

    if (currentState == PositionState::NONE) {
        if (latestPrice <= bands.lowerBand) {
            LOG("Bot " + clientId + " - Signal BUY (Prix <= Bande Inf.).", "INFO");
            action = TradingAction::BUY;
        }
    } else if (currentState == PositionState::LONG) {
        if (latestPrice >= bands.upperBand) {
             LOG("Bot " + clientId + " - Signal CLOSE_LONG (Prix >= Bande Sup.).", "INFO");
            action = TradingAction::CLOSE_LONG;
        }
    }

    // Log l'action décidée.
    // Utilise l'helper transactionTypeToString pour rendre le log plus propre pour BUY/SELL, etc.
    std::stringstream ss_log_action;
    ss_log_action << "Bot " << clientId << " - Action décidée: ";
    switch(action) {
        case TradingAction::HOLD: ss_log_action << "HOLD"; break;
        case TradingAction::BUY: ss_log_action << "BUY"; break;
        case TradingAction::SELL: ss_log_action << "SELL"; break; // Si SHORT implémenté
        case TradingAction::CLOSE_LONG: ss_log_action << "CLOSE_LONG"; break;
        case TradingAction::CLOSE_SHORT: ss_log_action << "CLOSE_SHORT"; break; // Si SHORT implémenté
        default: ss_log_action << "UNKNOWN"; break;
    }
    LOG(ss_log_action.str(), "INFO");
    return action; // Retourne l'action décidée.
}


// Notification reçue lorsque l'application d'une transaction pour ce client bot est complétée.
// Met à jour l'état de position du bot. Thread-safe (appelé par TransactionQueue). Protégé par mutex.
void Bot::notifyTransactionCompleted(const Transaction& tx) {
    // Protège l'accès aux membres mutables (currentState, entryPrice)
    std::lock_guard<std::mutex> lock(botMutex);

    // Log la notification.
    // Utilise Transaction helpers pour les enums
    std::stringstream ss_log_tx;
    ss_log_tx << "Bot " << clientId << " - Notification transaction (ID: " << tx.getId()
              << ", Type: " << transactionTypeToString(tx.getType()) // Utilise helper
              << ", Statut: " << transactionStatusToString(tx.getStatus()) // Utilise helper
              << ", Client ID TX: " << tx.getClientId() << ")";
    LOG(ss_log_tx.str(), "INFO");


    // Vérifie si la transaction est COMPLETED/FAILED et si elle appartient bien à ce client bot.
    if (tx.getClientId() == this->clientId) { // Utilise getter
        if (tx.getStatus() == TransactionStatus::COMPLETED) { // Utilise getter
            // Gère la mise à jour de l'état en cas de succès.
            if (tx.getType() == TransactionType::BUY) { // Utilise getter
                if (currentState == PositionState::NONE) {
                    currentState = PositionState::LONG;
                    entryPrice = tx.getUnitPrice(); // Utilise getter
                    std::stringstream ss_log_entry;
                    ss_log_entry << "Bot " << clientId << " - Position ouverte: LONG @ " << std::fixed << std::setprecision(10) << entryPrice;
                    LOG(ss_log_entry.str(), "INFO");
                } else if (currentState == PositionState::LONG) {
                     std::stringstream ss_log_reinforce;
                     ss_log_reinforce << "Bot " << clientId << " - LONG renforcée @ " << std::fixed << std::setprecision(10) << tx.getUnitPrice() << " (logique moyenne d'entrée à implémenter)";
                     LOG(ss_log_reinforce.str(), "INFO");
                }
                

            } else if (tx.getType() == TransactionType::SELL) { // Utilise getter
                if (currentState == PositionState::LONG) {
                     std::stringstream ss_log_close_long;
                     ss_log_close_long << "Bot " << clientId << " - Position LONG clôturée @ " << std::fixed << std::setprecision(10) << tx.getUnitPrice() << ". Entrée était @ " << std::fixed << std::setprecision(10) << entryPrice;
                     LOG(ss_log_close_long.str(), "INFO");
                    currentState = PositionState::NONE;
                    entryPrice = 0.0;
                } else if (currentState == PositionState::NONE) {
                     std::stringstream ss_log_try_short;
                     ss_log_try_short << "Bot " << clientId << " - Tente d'ouvrir une position SHORT @ " << std::fixed << std::setprecision(10) << tx.getUnitPrice();
                     LOG(ss_log_try_short.str(), "INFO");
                }
            }
            // TODO: Gérer autres types de transactions (DEPOSIT/WITHDRAW) si pertinents pour le bot
            // Log le nouvel état
            LOG("Bot " + clientId + " - Nouvel état après TX: " + (currentState == PositionState::NONE ? "NONE" : (currentState == PositionState::LONG ? "LONG" : "SHORT")), "INFO");

        } else if (tx.getStatus() == TransactionStatus::FAILED) { // Utilise getter
            // Gère l'échec de transaction
            std::stringstream ss_log_failed;
            ss_log_failed << "Bot " << clientId << " - Transaction (ID: " << tx.getId()
                          << ", Type: " << transactionTypeToString(tx.getType())
                          << ") ÉCHOUÉE. Raison: " << tx.getFailureReason(); // Utilise getter
            LOG(ss_log_failed.str(), "ERROR"); // Log niveau ERROR
            // TODO : Implémenter logique de gestion d'erreur (retry? ajuster?)
        }
         // Ignore les statuts PENDING ou UNKNOWN.
    }
     // Ignore les transactions d'autres clients.
}