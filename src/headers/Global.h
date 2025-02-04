#ifndef GLOBAL_H
#define GLOBAL_H

#include <atomic>
#include <array>
#include <string>

namespace Global
{   
    // ?
    extern std::atomic<bool> stopRequested;
    // contient les valeurs du BTC de toute une journée (24h)
    extern std::array<double, 1> BTC_daily_values;

    // retourne la valeur de stopRequested
    std::atomic<bool> &getStopRequested();
    // retourne la valeur de BTC_daily_values
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

    //double getRandomDouble(double range);

    // genère un double aléatoire respectant certaines contraintes
    double getRandomDouble();
    // Retourne la valeur quotidienne du BTC pour un jour donné
    float get_daily_BTC_value(int d);
}

#endif // GLOBAL_H