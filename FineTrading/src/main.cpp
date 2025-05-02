#include <cstdlib>
#include <ctime>
#include <iostream>
#include <vector>
#include <string>
#include <stdexcept> // Pour std::exception
#include <iomanip>   // Pour std::setprecision
#include <algorithm> // Pour std::max

#include "CryptoData.h"   // Pour générer/récupérer les données
#include "Portfolio.h"    // Pour la classe Portfolio
#include "Strategy.h"   // Pour les fonctions de simulation

int main() {
    try {
        // --- Paramètres ---
        double initialBalance = 10000.0;
        int dataPointsToFetch = 1100; // Besoin d'un peu plus pour l'historique Bollinger et la marge
        int simulationSteps = 100;   // La durée de chaque simulation (fenêtre de trading)
        int numRuns = 100;           // Nombre de simulations aléatoires à lancer
        int bollingerPeriod = 20;    // Période utilisée par la stratégie Bollinger (IMPORTANT pour startIndex)

        // 1. Générer/Charger les données une seule fois
        std::cout << "Génération/Récupération des données BTC..." << std::endl;
        generateBTCValues(dataPointsToFetch); // Récupère les données

        // Vérifier si on a assez de données au total
        if (getBTCValuesNormal().size() < bollingerPeriod + simulationSteps) {
             std::cerr << "Erreur: Pas assez de données historiques totales (" << getBTCValuesNormal().size()
                       << ") pour assurer une simulation de " << simulationSteps << " étapes avec un historique Bollinger de "
                       << bollingerPeriod << ".\n";
             return 1;
        }

        // Initialise le générateur aléatoire
        std::srand(static_cast<unsigned int>(std::time(nullptr)));

        std::cout << "Démarrage de " << numRuns << " simulations...\n";

        for (int run = 1; run <= numRuns; ++run) {
            // --- Choix aléatoire de la période de simulation ---
            // Assurer que startIndex >= bollingerPeriod
            // Et que startIndex + simulationSteps <= totalDataSize
            int maxPossibleStartIndex = getBTCValuesNormal().size() - simulationSteps;
            int minPossibleStartIndex = bollingerPeriod; // Pour l'historique Bollinger

            if (minPossibleStartIndex > maxPossibleStartIndex) {
                 std::cerr << "Erreur logique: Impossible de choisir un startIndex valide avec les données actuelles.\n";
                 return 1; // Devrait pas arriver avec la vérification précédente mais sécurité
            }

            // Génère un startIndex dans la plage valide [minPossibleStartIndex, maxPossibleStartIndex]
            int startIndex = minPossibleStartIndex + (std::rand() % (maxPossibleStartIndex - minPossibleStartIndex + 1));

            // Affichage de la progression (optionnel)
            if (run % 10 == 0 || run == 1 || run == numRuns) {
                std::cout << "  Run " << run << "/" << numRuns << " (StartIndex: " << startIndex << ")" << std::endl;
            }


            // Types de données à tester
            std::vector<std::pair<std::string, const std::vector<double>*>> dataSets = {
                {"Normal", &getBTCValuesNormal()},
                {"Noise3", &getBTCValues3PercentNoise()},
                {"Noise6", &getBTCValues6PercentNoise()}
            };

            // Boucle sur chaque type de données (Normal, Bruit 3%, Bruit 6%)
            for (const auto& dataPair : dataSets) {
                const std::string& dataType = dataPair.first;
                const std::vector<double>& btcValues = *dataPair.second;

                // Indices de début et fin dans le vecteur pour CETTE simulation
                int simEndIndex = startIndex + simulationSteps - 1; // Indice du dernier prix utilisé

                // Prix BTC au début et à la fin de la simulation pour calculer la perf de base
                double btcStartPrice = btcValues[startIndex];
                double btcEndPrice = btcValues[simEndIndex];
                double btcChangePercent = (btcStartPrice > 0) ? ((btcEndPrice - btcStartPrice) / btcStartPrice * 100.0) : 0.0;

                // --- Simulation Stratégie Simple Momentum ---
                Portfolio portfolioSimple(initialBalance);
                simulateSimpleMomentumStrategy(portfolioSimple, btcValues, startIndex, simulationSteps); // Utilise la nouvelle fonction
                double finalValueSimple = portfolioSimple.getTotalValue(btcEndPrice); // Nouvelle méthode de calcul de valeur
                double portfolioSimpleChangePct = (initialBalance > 0) ? ((finalValueSimple - initialBalance) / initialBalance * 100.0) : 0.0;
                saveResultsToCSV(run, "SimpleMomentum", dataType, btcChangePercent, portfolioSimpleChangePct); // Fonction externe

                // --- Simulation Stratégie Bollinger Bands ---
                Portfolio portfolioBollinger(initialBalance); // Nouveau portefeuille pour cette strat
                // Les paramètres par défaut de Bollinger (20, 2.0, 1.0) sont utilisés si non spécifiés ici
                simulateBollingerBandsStrategy(portfolioBollinger, btcValues, startIndex, simulationSteps, bollingerPeriod); // Utilise la nouvelle fonction
                double finalValueBollinger = portfolioBollinger.getTotalValue(btcEndPrice);
                double portfolioBollingerChangePct = (initialBalance > 0) ? ((finalValueBollinger - initialBalance) / initialBalance * 100.0) : 0.0;
                saveResultsToCSV(run, "BollingerBands", dataType, btcChangePercent, portfolioBollingerChangePct); // Fonction externe
            }
        }

        std::cout << "\nSimulation terminée. Résultats dans results.csv ✅" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "\n*** Une erreur est survenue durant l'exécution ***\n";
        std::cerr << e.what() << std::endl;
        return 1; // Quitter avec un code d'erreur
    }

    return 0; // Succès
}