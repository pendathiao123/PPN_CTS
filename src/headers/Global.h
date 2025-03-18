#ifndef GLOBAL_H
#define GLOBAL_H

#include <atomic>
#include <array>
#include <string>
#include <unordered_map>
#include <mutex>

namespace Global
{   
    // Indicateur atomique pour demander l'arrêt des opérations
    extern std::atomic<bool> stopRequested;
    
    // Contient les valeurs du BTC de toute une journée (24h)
    extern std::array<double, 1> BTC_daily_values;

    // Limites d'achat de BTC
    constexpr double GLOBAL_DAILY_BTC_LIMIT = 1000.0; // Limite globale d'achat journalier en BTC
    constexpr double CLIENT_DAILY_BTC_LIMIT = 10.0;   // Limite d'achat journalier par client en BTC

    // Variables pour suivre les achats effectués
    extern double globalDailyBTCPurchased; // Quantité globale de BTC achetée quotidiennement
    extern std::unordered_map<std::string, double> clientDailyBTCPurchased; // Quantité de BTC achetée par client quotidiennement
    extern std::mutex purchaseMutex; // Mutex pour protéger l'accès aux achats

    // Retourne la valeur de stopRequested
    std::atomic<bool> &getStopRequested();
    
    // Retourne la référence à BTC_daily_values
    std::array<double, 1> &getBTCDailyValues();
    
    // Remplit BTC_daily_values à partir d'un fichier CSV
    void populateBTCValuesFromCSV(const std::string &filename);
    
    // Écrit les valeurs BTC dans un fichier CSV
    void writeBTCValuesToCSV(const std::string &filename);
    
    // Lit les valeurs BTC à partir d'un fichier CSV
    void readBTCValuesFromCSV(const std::string &filename);
    
    // Affiche les valeurs BTC pour un jour donné, entre deux secondes spécifiques
    void printBTCValuesForDay(int day, int start_second, int end_second);
    
    // Complète les valeurs BTC pour chaque seconde de la journée et les écrit dans un fichier CSV
    void Complete_BTC_value();

    // Génère un double aléatoire respectant certaines contraintes
    double getRandomDouble();
    
    // Retourne la valeur quotidienne du BTC pour un jour donné
    float get_daily_BTC_value(int d);
}

#endif // GLOBAL_H