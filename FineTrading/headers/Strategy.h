#ifndef STRATEGY_H
#define STRATEGY_H

#include <vector>

// Forward declaration de Portfolio pour éviter l'inclusion circulaire si Portfolio incluait Strategy.h
class Portfolio;

// Fonction pour la stratégie simple (momentum)
void simulateSimpleMomentumStrategy(
    Portfolio& portfolio,
    const std::vector<double>& btcValues,
    int startIndex,
    int numSteps,
    double threshold = 0.01, // Seuil de changement pour agir
    double tradeFraction = 0.1 // Fraction du portefeuille à trader
);

// Fonction pour la stratégie Bollinger Bands (mean reversion)
void simulateBollingerBandsStrategy(
    Portfolio& portfolio,
    const std::vector<double>& btcValues,
    int startIndex,
    int numSteps,
    int bollingerPeriod = 20,  // Période pour les bandes
    double bollingerK = 2.0,    // Nombre d'écarts-types
    double tradeFraction = 1.0 // Fraction à acheter/vendre sur signal (1.0 = tout)
);


#endif // STRATEGY_H