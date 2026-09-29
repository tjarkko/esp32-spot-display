#pragma once

#include "AppState.h"

namespace Display {
void begin();
void tick(const AppState& state, time_t now);
}
