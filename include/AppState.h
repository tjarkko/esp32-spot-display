#pragma once

#include "PriceModel.h"

struct AppState {
    PriceTable prices;
    bool wifiConnected = false;
    bool fetching = false;
    int signalDbm = 0;
    char message[80] = "Connecting to Wi-Fi";
};
