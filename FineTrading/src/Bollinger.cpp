#include "Bollinger.h" // Inclut la déclaration de la classe

#include <numeric>       // Pour std::accumulate
#include <cmath>         // Pour std::sqrt, std::pow
#include <stdexcept>     // Pour std::invalid_argument
#include <iostream>      // Pour les éventuels logs/debug (non utilisé dans les méthodes ici)
#include <iomanip>       // Pour std::setprecision (utilisé dans main, pas ici)

// --- Implémentation du Constructeur ---
BollingerBot::BollingerBot(int p, double k_stddev)
    : period(p),
      k(k_stddev),
      currentState(PositionState::NONE),
      entryPrice(0.0)
{
    if (period <= 1) {
        throw std::invalid_argument("Period must be greater than 1.");
    }
    if (k <= 0) {
        throw std::invalid_argument("K (stddev multiplier) must be positive.");
    }
    priceHistory.reserve(1000); // Pré-allocation optionnelle
}

// --- Implémentation de calculateSMA ---
double BollingerBot::calculateSMA(const std::vector<double>& data) {
    // Note: La vérification data.size() < period est faite avant l'appel dans calculateBands
    double sum = std::accumulate(data.end() - period, data.end(), 0.0);
    return sum / period;
}

// --- Implémentation de calculateStdDev ---
double BollingerBot::calculateStdDev(const std::vector<double>& data, double sma) {
     // Note: La vérification data.size() < period est faite avant l'appel dans calculateBands
    double sq_sum = 0.0;
    for (auto it = data.end() - period; it != data.end(); ++it) {
        sq_sum += (*it - sma) * (*it - sma); // ou std::pow(*it - sma, 2)
    }
    // Utiliser N (period) pour l'écart-type de population (courant en finance)
    // Si on voulait un estimateur non biaisé (sample stddev), on diviserait par period - 1
    return std::sqrt(sq_sum / period);
}

// --- Implémentation de calculateBands ---
BollingerBands BollingerBot::calculateBands() {
    BollingerBands bands;
    if (priceHistory.size() < period) {
        bands.valid = false;
        return bands;
    }

    bands.middle = calculateSMA(priceHistory);
    double stdDev = calculateStdDev(priceHistory, bands.middle);
    bands.upper = bands.middle + k * stdDev;
    bands.lower = bands.middle - k * stdDev;
    bands.valid = true;
    return bands;
}


// --- Implémentation de processNewPrice ---
TradingAction BollingerBot::processNewPrice(double newPrice) {
    priceHistory.push_back(newPrice);
    TradingAction action = TradingAction::HOLD;

    if (priceHistory.size() < period) {
        return TradingAction::HOLD;
    }

    BollingerBands bands = calculateBands();
    if (!bands.valid) {
        // Sécurité, ne devrait pas arriver si size >= period
        return TradingAction::HOLD;
    }

    // --- Logique de décision ---
    switch (currentState) {
        case PositionState::NONE:
            if (newPrice < bands.lower) {
                action = TradingAction::BUY;
                currentState = PositionState::LONG;
                entryPrice = newPrice;
            } else if (newPrice > bands.upper) {
                action = TradingAction::SELL;
                currentState = PositionState::SHORT;
                entryPrice = newPrice;
            }
            break;

        case PositionState::LONG:
            if (newPrice > bands.middle) {
                 action = TradingAction::CLOSE_LONG;
                 currentState = PositionState::NONE;
                 entryPrice = 0.0;
            }
            break;

        case PositionState::SHORT:
             if (newPrice < bands.middle) {
                 action = TradingAction::CLOSE_SHORT;
                 currentState = PositionState::NONE;
                 entryPrice = 0.0;
             }
             break;
    }

    return action;
}

// --- Implémentation des Getters ---
PositionState BollingerBot::getCurrentState() const {
    return currentState;
}

double BollingerBot::getEntryPrice() const {
    return entryPrice;
}

BollingerBands BollingerBot::getLastBands() {
    // Recalcule à chaque appel. Pourrait être optimisé en stockant
    // les dernières bandes calculées si processNewPrice vient d'être appelé.
    return calculateBands();
}
