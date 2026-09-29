#pragma once
#include <stddef.h>
#include <time.h>
constexpr size_t maxPrices = 200; // Two Finnish calendar days, including DST.
constexpr time_t intervalSeconds = 900;
struct PriceInterval { time_t start; float centsPerKWh; };
struct PriceTable {
    PriceInterval values[maxPrices]{};
    size_t count = 0;
    time_t fetchedAt = 0;
};
bool parseUtc(const char* text, time_t& result);
const PriceInterval* currentPrice(const PriceTable& table, time_t now);
struct PriceStats {
    size_t count = 0;
    float min = 0, max = 0, average = 0;
    time_t minAt = 0, maxAt = 0;
};
PriceStats priceStats(const PriceTable& table, time_t start, time_t end);
