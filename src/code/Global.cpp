#include "../headers/Global.h"
#include "../headers/Logger.h"

#include <curl/curl.h>        
#include <nlohmann/json.hpp>  
#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>              
#include <atomic>             
#include <random>             
#include <thread>             
#include <chrono>             
#include <cmath>              
#include <filesystem>
#include <stdexcept>          
#include <iomanip>            
#include <sstream>            
#include <mutex>              


// Utilisation de l'espace de noms pour la bibliothèque JSON
using json = nlohmann::json;

// --- Initialisation des membres statiques ---

// Mutex pour la dernière valeur instantanée
std::mutex Global::srdMutex;
double Global::lastSRDBTCValue = 0.0;

// Mutex pour le buffer circulaire et son index
std::mutex Global::bufferMutex;

// Buffer circulaire des prix historiques (taille fixe, initialisé à 0.0)
std::vector<double> Global::ActiveSRDBTC(Global::MAX_VALUES_PER_DAY, 0.0);
// Index de la prochaine position d'écriture dans le buffer circulaire
std::atomic<int> Global::activeIndex = 0; // Commence à 0

// Flag pour signaler l'arrêt du thread de génération de prix
std::atomic<bool> Global::stopRequested = false;

// Thread dédié à la génération/mise à jour des prix
std::thread Global::priceGenerationWorker;


// --- Callback de libcurl ---
// Stocke les données reçues dans un std::string.
size_t Global::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// --- Implémentation des Fonctions Utilitaires StringTo/ToString ---


// Convertit Currency enum en string
std::string currencyToString(Currency currency) {
    switch (currency) {
        case Currency::USD: return "USD";
        case Currency::SRD_BTC: return "SRD-BTC";
        case Currency::UNKNOWN: return "UNKNOWN_CURRENCY";
        default: return "UNKNOWN_CURRENCY"; // Retour par défaut pour robustesse.
    }
}

// Convertit string en Currency enum (insensible à la casse)
Currency stringToCurrency(const std::string& str) {
    std::string lower_str = str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
    if (lower_str == "usd") return Currency::USD;
    if (lower_str == "srd-btc") return Currency::SRD_BTC;
    return Currency::UNKNOWN;
}

// Convertit TransactionType enum en string
std::string transactionTypeToString(TransactionType type) {
    switch (type) {
        case TransactionType::BUY: return "BUY";
        case TransactionType::SELL: return "SELL";
        case TransactionType::UNKNOWN: return "UNKNOWN_TYPE";
        default: return "UNKNOWN_TYPE"; // Retour par défaut pour robustesse.
    }
}

// Convertit string en TransactionType enum (insensible à la casse)
TransactionType stringToTransactionType(const std::string& str) {
    std::string lower_str = str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
    if (lower_str == "buy") return TransactionType::BUY;
    if (lower_str == "sell") return TransactionType::SELL;
    return TransactionType::UNKNOWN;
}

// Convertit TransactionStatus enum en string
std::string transactionStatusToString(TransactionStatus status) {
    switch (status) {
        case TransactionStatus::PENDING: return "PENDING";
        case TransactionStatus::COMPLETED: return "COMPLETED";
        case TransactionStatus::FAILED: return "FAILED";
        case TransactionStatus::UNKNOWN: return "UNKNOWN_STATUS";
        default: return "UNKNOWN_STATUS"; // Retour par défaut pour robustesse.
    }
}

// Convertit string en TransactionStatus enum (insensible à la casse)
TransactionStatus stringToTransactionStatus(const std::string& str) {
    std::string lower_str = str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
    if (lower_str == "pending") return TransactionStatus::PENDING;
    if (lower_str == "completed") return TransactionStatus::COMPLETED;
    if (lower_str == "failed") return TransactionStatus::FAILED;
    if (lower_str == "unknown") return TransactionStatus::UNKNOWN; // Gère aussi "unknown" string si jamais loggué ainsi
    return TransactionStatus::UNKNOWN;
}

// --- Implémentation de positionStateToString ---
std::string positionStateToString(PositionState ps) {
    switch (ps) {
        case PositionState::NONE: return "NONE";
        case PositionState::LONG: return "LONG";
        case PositionState::SHORT: return "SHORT";
        case PositionState::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN_STATE"; // Cas par défaut si une valeur inattendue est passée
    }
}

// --- Implémentation de tradingActionToString ---
std::string tradingActionToString(TradingAction ta) {
    switch (ta) {
        case TradingAction::HOLD: return "HOLD";
        case TradingAction::BUY: return "BUY";
        case TradingAction::SELL: return "SELL";
        case TradingAction::CLOSE_LONG: return "CLOSE_LONG";
        case TradingAction::CLOSE_SHORT: return "CLOSE_SHORT";
        case TradingAction::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN_ACTION"; // Cas par défaut
    }
}

// --- Boucle principale du thread de génération de prix ---
// S'exécute dans priceGenerationWorker.
void Global::generate_SRD_BTC_loop_impl() {
    LOG("Global Thread de génération de prix démarré.", "INFO");

    // Initialisation des ressources du thread.
    CURL* curl = nullptr;
    std::string readBuffer;
    std::default_random_engine generator(std::random_device{}());
    // Distribution normale pour une fluctuation aléatoire (moyenne 0.0, écart-type 0.015 = 1.5%)
    std::normal_distribution<double> distribution(0.0, 0.015);

    std::string priceLogPath = "../src/data/srd_btc_values.csv"; // Chemin relatif du fichier de log
    std::ofstream priceFile(priceLogPath, std::ios::app); // Ouvre en mode ajout

    if (!priceFile.is_open()) {
        LOG("Global Impossible d'ouvrir/créer le fichier de log des prix : " + priceLogPath + ". Le thread continuera mais sans logging disque.", "ERROR");
    } else {
        priceFile.seekp(0, std::ios::end);
        if (priceFile.tellp() == 0) {
             priceFile << "Timestamp,SRD-BTC_USD\n";
             priceFile.flush();
             LOG("Global Fichier de log des prix '" + priceLogPath + "' ouvert. En-tête ajouté.", "INFO");
        } else {
             LOG("Global Fichier de log des prix '" + priceLogPath + "' ouvert en mode append.", "INFO");
        }
    }

    // Initialisation cURL.
    curl = curl_easy_init();
    if (!curl) {
        LOG("Global Erreur fatale : Impossible d'initialiser cURL. Les requêtes API échoueront.", "ERROR");
    }

    // --- Boucle principale ---
    while (!stopRequested.load()) { // Le thread tourne tant que l'arrêt n'est pas demandé

        double currentBTCValue = -1.0; // Initialise la valeur BTC pour ce cycle

        // --- Récupération du prix via API ---
        if (curl) {
            readBuffer.clear();
            curl_easy_setopt(curl, CURLOPT_URL, "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd");
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

            CURLcode res = curl_easy_perform(curl);

            if (res == CURLE_OK) {
                try {
                    auto jsonData = json::parse(readBuffer);
                    if (jsonData.contains("bitcoin") && jsonData["bitcoin"].contains("usd")) {
                        currentBTCValue = jsonData["bitcoin"]["usd"].get<double>();
                    } else {
                         LOG("Global Réponse JSON de CoinGecko inattendue. Réponse: '" + readBuffer + "'.", "WARNING");
                    }
                } catch (const json::exception& e) {
                    LOG("Global Erreur parsing JSON. Erreur: " + std::string(e.what()) + ". Réponse brute: '" + readBuffer + "'.", "ERROR");
                } catch (const std::exception& e) {
                     LOG("Global Erreur inattendue lors du traitement de la réponse API. Erreur: " + std::string(e.what()) + ".", "ERROR");
                }
            } else {
                LOG("Global Erreur cURL lors de la récupération du prix : " + std::string(curl_easy_strerror(res)) + ".", "ERROR");
            }
        } else {
             if (!stopRequested.load()) {
                 LOG("Global Le handle cURL n'est pas valide. Impossible d'effectuer la requête API.", "ERROR");
             }
        }
        // --- Fin Récupération Prix ---


        // --- Simulation et Mise à jour Thread-Safe ---
        if (currentBTCValue > 0 && std::isfinite(currentBTCValue)) { // Vérifie si le prix BTC est valide et fini
            double fluctuation = distribution(generator);
            double srd_btc = currentBTCValue * (1.0 + fluctuation);
            if (srd_btc <= 0 || !std::isfinite(srd_btc)) srd_btc = 0.01; // Assure un prix positif et fini

            { // Début section critique avec verrous
                std::lock_guard<std::mutex> lock_srd(srdMutex);
                lastSRDBTCValue = srd_btc; // Mise à jour dernière valeur

                std::lock_guard<std::mutex> lock_buffer(bufferMutex);
                 // Utilisation simple (seq_cst) ou relaxed/acquire/release - seq_cst est plus sûr/simple ici
                 int index = activeIndex.load(); // Peut utiliser memory_order_seq_cst (par défaut)
                ActiveSRDBTC[index] = srd_btc;
                 activeIndex.store((index + 1) % MAX_VALUES_PER_DAY); // Peut utiliser memory_order_seq_cst (par défaut)
            } // Les verrous sont libérés

            // --- Logging de la nouvelle valeur ---
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::tm timeinfo_buffer;
            std::tm* timeinfo = localtime_r(&time_t, &timeinfo_buffer); // Utilise localtime_r (thread-safe)

            std::stringstream ss_timestamp;
            if (timeinfo) { ss_timestamp << std::put_time(timeinfo, "%Y-%m-%d %X"); } else { ss_timestamp << "[TIMESTAMP_ERROR]"; }

            if (priceFile.is_open()) {
                priceFile << ss_timestamp.str() << "," << std::fixed << std::setprecision(10) << srd_btc << "\n";
                priceFile.flush();
            }
            std::stringstream ss_log_price;
            ss_log_price << "Global Nouveau prix SRD-BTC simulé : " << std::fixed << std::setprecision(10) << srd_btc;
            LOG(ss_log_price.str(), "INFO");


        } else {
             LOG("Global Prix BTC non récupéré ou invalide. La valeur SRD-BTC n'est pas mise à jour dans ce cycle.", "WARNING");
        }
        // --- Fin Simulation et Mise à jour ---


        // Pause avant le prochain cycle.
        std::this_thread::sleep_for(std::chrono::seconds(15)); // Fréquence de mise à jour des prix

    } // --- Fin de la boucle principale ---

    // --- Nettoyage à l'arrêt du thread ---
    if (curl) { curl_easy_cleanup(curl); } // Supprimé (DEBUG)
    if (priceFile.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm timeinfo_buffer;
        std::tm* timeinfo = localtime_r(&time_t, &timeinfo_buffer);
        std::stringstream ss_timestamp;
        if (timeinfo) { ss_timestamp << std::put_time(timeinfo, "%Y-%m-%d %X"); } else { ss_timestamp << "[TIMESTAMP_ERROR]"; }
        priceFile << "--- Fin du log de prix : " << ss_timestamp.str() << " ---\n";
        priceFile.close();
        LOG("Global Fichier de log des prix fermé.", "INFO");
    }
    LOG("Global Thread de génération de prix terminé.", "INFO");
}


// --- Implémentation des méthodes de gestion du thread ---

// Démarre le thread s'il n'est pas lancé.
void Global::startPriceGenerationThread() {
    if (!priceGenerationWorker.joinable()) {
        LOG("Global Demande de démarrage du thread de génération de prix.", "INFO");
        stopRequested.store(false);
        priceGenerationWorker = std::thread(&Global::generate_SRD_BTC_loop_impl);
        LOG("Global Thread de génération de prix initié.", "INFO");
    } else {
        LOG("Global Thread de génération de prix déjà en cours.", "WARNING");
    }
}

// Signale l'arrêt et attend la fin du thread.
void Global::stopPriceGenerationThread() {
    LOG("Global Demande d'arrêt du thread de génération de prix.", "INFO");
    stopRequested.store(true);
    if (priceGenerationWorker.joinable()) {
        LOG("Global Jointure du thread de génération de prix...", "INFO");
        priceGenerationWorker.join();
        LOG("Global Thread de génération de prix joint.", "INFO");
    } else {
        LOG("Global Thread de génération de prix non joignable ou déjà terminé lors de la demande d'arrêt.", "WARNING");
    }
}


// --- Implémentation des méthodes d'accès aux prix (Thread-Safe) ---

// Retourne le dernier prix SRD-BTC connu.
double Global::getPrice(const std::string& currency) {
    if (currency == "SRD-BTC") {
        std::lock_guard<std::mutex> lock(srdMutex); // Protège la lecture
        return lastSRDBTCValue;
    }
    LOG("Global Tentative d'accès au prix pour devise non supportée : " + currency, "WARNING");
    return 0.0;
}

// Retourne un prix historique approximatif.
double Global::getPreviousPrice(const std::string& currency, int secondsBack) {
     if (currency != "SRD-BTC") {
        LOG("Global Tentative d'accès à l'historique pour devise non supportée : " + currency, "WARNING");
        return 0.0;
     }
     if (secondsBack <= 0) {
        return getPrice(currency); // Retourne le prix actuel (qui est déjà thread-safe)
     }

    // L'intervalle entre deux mises à jour stockées.
    const int update_interval_sec = 15; // Doit correspondre à la pause dans la boucle de génération.

    // Calcule le nombre de pas en arrière.
    int stepsBack = secondsBack / update_interval_sec;

    // Limite les pas si cela dépasse la capacité du buffer.
    if (stepsBack >= MAX_VALUES_PER_DAY) {
        LOG("Global getPreviousPrice - Histoire demandée (" + std::to_string(secondsBack) + "s) excède la capacité du buffer (" + std::to_string(MAX_VALUES_PER_DAY * update_interval_sec) + "s). Retourne l'élément le plus ancien stocké.", "WARNING");
        stepsBack = MAX_VALUES_PER_DAY - 1; // Indexe l'élément le plus ancien.
    }

    { // Début section critique pour lecture du buffer et de l'index
        std::lock_guard<std::mutex> lock(bufferMutex); // Protège la lecture

        // Calcule l'index cible.
        // activeIndex pointe vers la prochaine écriture.
        int raw_index = activeIndex.load() - stepsBack; // Peut utiliser memory_order_seq_cst (par défaut)
        int target_index = raw_index % MAX_VALUES_PER_DAY;
        if (target_index < 0) {
            target_index += MAX_VALUES_PER_DAY; // Gère l'enroulement pour les indices négatifs
        }

        // Retourne la valeur.
        return ActiveSRDBTC[target_index];

    } // Le verrou est libéré
}

// Accède au flag atomique stopRequested (utilisé par le Server pour arrêter le thread).
std::atomic<bool>& Global::getStopRequested() {
    return stopRequested;
}