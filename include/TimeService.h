#pragma once
#include <time.h>
#include <stddef.h>
namespace TimeService {
void begin();
bool ready();
time_t midnight(time_t now, int dayOffset=0);
void format(time_t value, char* output, size_t length, const char* pattern="%H:%M");
}
