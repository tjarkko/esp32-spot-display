#include "TimeService.h"
#include "Config.h"
#include <Arduino.h>
void TimeService::begin() {
    configTzTime(Config::timezone,"pool.ntp.org","time.cloudflare.com","time.google.com");
}
bool TimeService::ready() { return time(nullptr)>1704067200; }
time_t TimeService::midnight(time_t now, int dayOffset) {
    tm local{}; localtime_r(&now,&local);
    local.tm_hour=local.tm_min=local.tm_sec=0;
    local.tm_mday+=dayOffset;
    local.tm_isdst=-1; // Recompute DST for the target date, not today's offset.
    return mktime(&local);
}
void TimeService::format(time_t value,char* output,size_t length,const char* pattern) {
    tm local{}; localtime_r(&value,&local); strftime(output,length,pattern,&local);
}
