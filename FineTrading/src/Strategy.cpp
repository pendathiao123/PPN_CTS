#include "Strategy.h"
#include "Portfolio.h"    // Inclure Portfolio.h pour utiliser la classe Portfolio
#include "Bollinger.h" // Inclure BollingerBot.h pour la stratégie Bollinger
#include <vector>
#include <iostream>
#include <stdexcept> // Pour std::runtime_error

// --- Stratégie Simple Momentum ---
void simulateSimpleMomentumStrategy(
    Portfolio& portfolio,
    const std::vector<double>& btcValues,
    int startIndex,
    int numSteps,
    double threshold, // = 0.01 par défaut dans le .h
    double tradeFraction // = 0.1 par défaut dans le .h
) {
    std::cout << "\n--- Simulation Stratégie Simple Momentum ---\n";
    if (startIndex + numSteps > btcValues.size() || startIndex < 0) {
        throw std::runtime_error("Indices de simulation invalides pour Simple Momentum.");
    }

    for (int i = 0; i < numSteps; ++i) {
        int currentIndex = startIndex + i;
        double currentPrice = btcValues[currentIndex];

        // On a besoin du prix précédent pour calculer la variation
        if (i > 0) {
            double prevPrice = btcValues[currentIndex - 1];
            if (prevPrice <= 0) continue; // Éviter division par zéro

            double change = (currentPrice - prevPrice) / prevPrice;

            // Affichage de l'étape (optionnel, peut devenir verbeux)
            // std::cout << "Étape " << i << ": Prix=" << currentPrice << ", Var=" << change*100 << "%\n";

            if (change > threshold) {
                // Si ça monte significativement, vendre une fraction du BTC détenu
                // std::cout << "  Signal Vente (Momentum)\n";
                portfolio.sellBTC(currentPrice, tradeFraction);
            } else if (change < -threshold) {
                // Si ça baisse significativement, acheter avec une fraction du solde dispo
                // std::cout << "  Signal Achat (Momentum)\n";
                portfolio.buyBTC(currentPrice, tradeFraction);
            }
        }
         // À l'étape 0, on ne fait rien car pas de prix précédent
    }
     std::cout << "--- Fin Simulation Simple Momentum ---\n";
}


// --- Stratégie Bollinger Bands (Mean Reversion Long Only) ---
void simulateBollingerBandsStrategy(
    Portfolio& portfolio,
    const std::vector<double>& btcValues,
    int startIndex,
    int numSteps,
    int bollingerPeriod, // = 20 par défaut
    double bollingerK,   // = 2.0 par défaut
    double tradeFraction // = 1.0 par défaut (pour acheter/vendre tout)
) {
    std::cout << "\n--- Simulation Stratégie Bollinger Bands ---\n";
     if (startIndex + numSteps > btcValues.size() || startIndex < 0) {
        throw std::runtime_error("Indices de simulation invalides pour Bollinger Bands.");
    }
    if (startIndex < bollingerPeriod) {
         std::cout << "Attention: Pas assez de données historiques avant startIndex pour initialiser Bollinger correctement. La simulation démarrera avec moins de " << bollingerPeriod << " points initiaux.\n";
         // Ou lancer une erreur si on veut être strict:
         // throw std::runtime_error("Pas assez de données historiques avant startIndex pour Bollinger.");
    }

    // Créer le bot Bollinger
    BollingerBot bot(bollingerPeriod, bollingerK);

    // *** Initialisation du Bot avec les données PRÉCÉDANT le début de la simulation ***
    // C'est CRUCIAL pour que les bandes soient correctes dès le début.
    int historyStart = std::max(0, startIndex - bollingerPeriod);
    for(int h = historyStart; h < startIndex; ++h) {
        bot.processNewPrice(btcValues[h]); // On nourrit le bot sans trader pour calculer les 1ères bandes
    }
     std::cout << "Bot Bollinger initialisé avec " << startIndex - historyStart << " points de données.\n";


    // --- Boucle de simulation ---
    for (int i = 0; i < numSteps; ++i) {
        int currentIndex = startIndex + i;
        double currentPrice = btcValues[currentIndex];

        // Obtenir la décision du bot Bollinger
        TradingAction action = bot.processNewPrice(currentPrice);

        // Affichage (optionnel)
        // BollingerBands bands = bot.getLastBands();
        // std::cout << "Étape " << i << ": Prix=" << currentPrice;
        // if(bands.valid) std::cout << " (Bandes: L=" << bands.lower << " M=" << bands.middle << " U=" << bands.upper << ")";
        // std::cout << " -> Action Bot: ";
        // switch(action){ /* ... afficher nom action ... */ } std::cout << "\n";


        // --- Agir en fonction de la décision du bot (Version LONG ONLY) ---
        switch (action) {
            case TradingAction::BUY:
                // Acheter seulement si on n'a pas déjà de BTC (pour éviter d'acheter encore si déjà long)
                if (portfolio.getBTC() <= 1e-9) { // Utiliser une petite tolérance pour les flottants
                     std::cout << "  Signal Achat (Bollinger)\n";
                    portfolio.buyBTC(currentPrice, tradeFraction); // Acheter avec X% du solde
                } else {
                    // std::cout << "  Signal Achat ignoré (déjà en position Long)\n";
                }
                break;

            case TradingAction::CLOSE_LONG:
                // Vendre seulement si on a du BTC
                if (portfolio.getBTC() > 1e-9) {
                     std::cout << "  Signal Clôture Achat (Bollinger)\n";
                    portfolio.sellBTC(currentPrice, 1.0); // Vendre 100% du BTC détenu
                } else {
                    // std::cout << "  Signal Clôture Achat ignoré (pas de position Long)\n";
                }
                break;

            // On ignore les signaux Short car le Portfolio ne les gère pas
            case TradingAction::SELL:
                 // std::cout << "  Signal Vente (Short) ignoré\n";
                break;
            case TradingAction::CLOSE_SHORT:
                 // std::cout << "  Signal Clôture Vente (Short) ignoré\n";
                break;

            case TradingAction::HOLD:
            default:
                // Ne rien faire
                break;
        }
    }
     std::cout << "--- Fin Simulation Bollinger Bands ---\n";
}  