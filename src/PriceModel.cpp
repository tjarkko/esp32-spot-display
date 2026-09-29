#include "PriceModel.h"
#include <cstdio>
#include <cstring>

bool parseUtc(const char* text, time_t& result) {
    if (!text) return false;
    const size_t length = strlen(text);
    if (length != 20 && length != 24) return false;
    if (text[4]!='-' || text[7]!='-' || text[10]!='T' || text[13]!=':' ||
        text[16]!=':' || text[length-1]!='Z') return false;
    for (size_t i=0; i<length; ++i) {
        if (i==4 || i==7 || i==10 || i==13 || i==16 || i==length-1) continue;
        if (i==19 && length==24) { if (text[i]!='.') return false; continue; }
        if (text[i]<'0' || text[i]>'9') return false;
    }
    int y,m,d,h,mi,s;
    if (sscanf(text,"%4d-%2d-%2dT%2d:%2d:%2d",&y,&m,&d,&h,&mi,&s)!=6)
        return false;
    if (y<2024 || y>2099 || m<1 || m>12 || d<1 || d>31 || h>23 || mi>59 || s!=0 || mi%15)
        return false;
    if (length==24 && strncmp(text+20,"000",3)!=0) return false;
    // Gregorian civil date -> UTC epoch, independent of the process timezone.
    int year=y-(m<=2);
    const int era=year/400, yoe=year-era*400;
    const int doy=(153*(m+(m>2?-3:9))+2)/5+d-1;
    const int days=era*146097+yoe*365+yoe/4-yoe/100+doy-719468;
    result=static_cast<time_t>(days)*86400+h*3600+mi*60;
    tm check{}; gmtime_r(&result,&check);
    return check.tm_year+1900==y && check.tm_mon+1==m && check.tm_mday==d;
}

const PriceInterval* currentPrice(const PriceTable& table, time_t now) {
    for (size_t i=0;i<table.count;++i)
        if (now>=table.values[i].start && now<table.values[i].start+intervalSeconds)
            return &table.values[i];
    return nullptr;
}

PriceStats priceStats(const PriceTable& table, time_t start, time_t end) {
    PriceStats stats;
    double sum=0;
    for (size_t i=0;i<table.count;++i) {
        const auto& p=table.values[i];
        if (p.start<start || p.start>=end) continue;
        if (!stats.count || p.centsPerKWh<stats.min) { stats.min=p.centsPerKWh; stats.minAt=p.start; }
        if (!stats.count || p.centsPerKWh>stats.max) { stats.max=p.centsPerKWh; stats.maxAt=p.start; }
        sum+=p.centsPerKWh; ++stats.count;
    }
    if (stats.count) stats.average=sum/stats.count;
    return stats;
}
