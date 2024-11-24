#include "../headers/global.h"

std::atomic<bool> stopRequested(false);
std::array<double, 3927> BTC_daily_values = { /* valeurs initiales */ };
std::map<std::pair<int, int>, double> BTC_sec_values;
