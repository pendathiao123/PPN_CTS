#ifndef GLOBAL_H
#define GLOBAL_H

#include <atomic>
#include <array>
#include <string>

namespace Global
{
    extern std::atomic<bool> stopRequested;
    extern std::array<double, 1> BTC_daily_values;

    std::atomic<bool> &getStopRequested();
    std::array<double, 1> &getBTCDailyValues();
    void populateBTCValuesFromCSV(const std::string &filename);
    void writeBTCValuesToCSV(const std::string &filename);
    void readBTCValuesFromCSV(const std::string &filename);
    void printBTCValuesForDay(int day, int start_second, int end_second);

    void Complete_BTC_value();
    //double getRandomDouble(double range);
    double getRandomDouble();
    float get_daily_BTC_value(int d);
}

#endif // GLOBAL_H