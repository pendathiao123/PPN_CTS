#ifndef GLOBALS_H
#define GLOBALS_H

#include <atomic>
#include <map>
#include <array>

extern std::atomic<bool> stopRequested;

extern std::array<double, 3927> BTC_daily_values;

extern std::map<std::pair<int, int>, double> BTC_sec_values;

#endif
