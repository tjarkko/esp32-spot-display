#pragma once
#include "PriceModel.h"
// Provider-specific HTTP and JSON stay behind this interface.
bool fetchPrices(PriceTable& result, time_t now, char* error, size_t errorSize);
