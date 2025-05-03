#include "CryptoData.h" // Inclure le header correspondant
#include <iostream>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <random>
#include <chrono>
#include <thread>
#include <stdexcept> // Pour runtime_error

using json = nlohmann::json;

// Définition des variables globales déclarées dans le .h
std::vector<double> BTCValuesNormal;
std::vector<double> BTCValues3PercentNoise;
std::vector<double> BTCValues6PercentNoise;

// Fonction pour générer un bruit aléatoire suivant une distribution normale
double generateRandomNoise(double mean, double stddev) {
    // Note : Créer rd et gen à chaque appel n'est pas le plus efficace,
    // mais pour ce contexte, c'est acceptable. Pour des perfs max, on les créerait une fois.
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> dist(mean, stddev);
    return dist(gen);
}

// Fonction pour récupérer les données historiques du BTC sur une période de 90 jours
void generateBTCValues(int numValues) {
    if (!BTCValuesNormal.empty()) {
        // std::cout << "Les valeurs BTC existent déjà. Utilisation des valeurs en cache.\n";
        return; // Ne pas re-télécharger si déjà fait
    }
    std::cout << "Tentative de récupération de " << numValues << " valeurs BTC de CoinGecko...\n";

    // Calcul des timestamps UNIX pour la période de 90 jours (ou plus si numValues est grand)
    // On ajuste la durée demandée pour être sûr d'avoir assez de points (API renvoie souvent en daily)
    // CoinGecko renvoie des données horaires pour > 90 jours, daily pour < 90 jours
    // Demandons un peu plus pour être sûr d'avoir numValues points horaires/daily
    long duration_seconds = 90 * 24 * 60 * 60; // 90 jours par défaut
    // Si on demande beaucoup de valeurs, il faut peut-être plus de 90j
     if (numValues > 2160) { // Plus que 90*24
        duration_seconds = (static_cast<long>(numValues) / 24 + 1) * 24 * 60 * 60; // Estimer le nb jours + 1
     }


    auto now = std::chrono::system_clock::now();
    auto end_time = std::chrono::system_clock::to_time_t(now);
    auto start_time = end_time - duration_seconds;

    // Faire une requête pour obtenir les données historiques du BTC
    // Augmenter le timeout peut être utile
    cpr::Response r = cpr::Get(cpr::Url{"https://api.coingecko.com/api/v3/coins/bitcoin/market_chart/range"},
                               cpr::Parameters{
                                   {"vs_currency", "usd"},
                                   {"from", std::to_string(start_time)},
                                   {"to", std::to_string(end_time)}
                               },
                               cpr::Timeout{30000}); // Timeout de 30 secondes

    if (r.status_code == 0) {
         std::cerr << "Erreur critique: Impossible de contacter l'API CoinGecko (Timeout ou problème réseau?).\n";
         std::cerr << "Erreur CPR: " << r.error.message << std::endl;
         throw std::runtime_error("Erreur réseau API CoinGecko"); // Stopper si pas de données
    }
    if (r.status_code != 200) {
        std::cerr << "Erreur lors de la récupération des données CoinGecko, Code: " << r.status_code << "\n";
        std::cerr << "Réponse: " << r.text << std::endl;
         throw std::runtime_error("Erreur API CoinGecko (code != 200)"); // Stopper
    }

    try {
        json data = json::parse(r.text);
        if (!data.contains("prices") || !data["prices"].is_array()) {
             std::cerr << "Erreur: Réponse JSON invalide ou 'prices' manquant.\n";
             throw std::runtime_error("Format JSON invalide"); // Stopper
        }
        auto prices = data["prices"];

        if (prices.empty()) {
            std::cerr << "Erreur: Aucune donnée de prix retournée par l'API.\n";
            throw std::runtime_error("Aucune donnée de prix"); // Stopper
        }

        // On va prendre les 'numValues' dernières valeurs disponibles, ou toutes si moins
        int numAvailable = prices.size();
        int startIndex = std::max(0, numAvailable - numValues);
        int valuesToProcess = numAvailable - startIndex;


        BTCValuesNormal.reserve(valuesToProcess);
        BTCValues3PercentNoise.reserve(valuesToProcess);
        BTCValues6PercentNoise.reserve(valuesToProcess);

        // Remplir les tableaux avec les données récupérées
        for (int i = startIndex; i < numAvailable; ++i) {
             if (!prices[i].is_array() || prices[i].size() != 2 || !prices[i][1].is_number()) {
                 std::cerr << "Attention: Format de prix inattendu à l'indice " << i << ", ignoré.\n";
                 continue; // Ignorer cette donnée invalide
             }
            double btcValue = prices[i][1].get<double>();

            BTCValuesNormal.push_back(btcValue);

            double noise3 = generateRandomNoise(0.0, 0.03);
            double noisyValue3 = btcValue * (1.0 + noise3);
            BTCValues3PercentNoise.push_back(std::max(0.0, noisyValue3)); // Éviter prix négatifs

            double noise6 = generateRandomNoise(0.0, 0.06);
            double noisyValue6 = btcValue * (1.0 + noise6);
            BTCValues6PercentNoise.push_back(std::max(0.0, noisyValue6)); // Éviter prix négatifs
        }

        if (BTCValuesNormal.empty()) {
             std::cerr << "Erreur critique: Aucune valeur valide n'a pu être extraite.\n";
             throw std::runtime_error("Extraction de valeur échouée");
        }

        std::cout << "Récupération et traitement de " << BTCValuesNormal.size() << " valeurs du BTC réussis.\n";

    } catch (const json::parse_error& e) {
        std::cerr << "Erreur critique lors du parsing de la réponse JSON : " << e.what() << "\n";
        std::cerr << "Réponse reçue (tronquée): " << r.text.substr(0, 500) << "...\n";
         throw std::runtime_error("Erreur parsing JSON"); // Stopper
    } catch (const std::exception& e) {
        std::cerr << "Erreur critique inattendue : " << e.what() << std::endl;
        throw; // Re-lancer pour stopper
    }
}

// Implémentation des Getters
const std::vector<double>& getBTCValuesNormal() {
    if (BTCValuesNormal.empty()) generateBTCValues(); // Générer si nécessaire
    return BTCValuesNormal;
}
const std::vector<double>& getBTCValues3PercentNoise() {
     if (BTCValues3PercentNoise.empty()) generateBTCValues(); // Générer si nécessaire
    return BTCValues3PercentNoise;
}
const std::vector<double>& getBTCValues6PercentNoise() {
     if (BTCValues6PercentNoise.empty()) generateBTCValues(); // Générer si nécessaire
    return BTCValues6PercentNoise;
}