#include "PriceService.h"
#include "TimeService.h"
#include "ApiCertificate.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <algorithm>
#include <cmath>

bool fetchPrices(PriceTable& result,time_t now,char* error,size_t errorSize) {
    auto fail=[&](const char* message) { snprintf(error,errorSize,"%s",message); return false; };
    const time_t start=TimeService::midnight(now), end=TimeService::midnight(now,2);
    char from[25],until[25],base[21],url[200]; tm utc{};
    gmtime_r(&start,&utc);
    strftime(base,sizeof(base),"%Y-%m-%dT%H:%M:%S",&utc);
    snprintf(from,sizeof(from),"%s.000Z",base);
    gmtime_r(&end,&utc);
    strftime(base,sizeof(base),"%Y-%m-%dT%H:%M:%S",&utc);
    snprintf(until,sizeof(until),"%s.000Z",base);
    snprintf(url,sizeof(url),"https://sahkotin.fi/prices?quarter&fix&vat&start=%s&end=%s",from,until);
    WiFiClientSecure client;
    client.setCACert(apiRootCA);
    client.setHandshakeTimeout(15);
    client.setTimeout(10000);
    HTTPClient http;
    http.useHTTP10(true); // Stream JSON without chunked transfer framing.
    http.setConnectTimeout(10000); http.setTimeout(10000);
    if (!http.begin(client,url)) return fail("HTTPS setup failed");
    const int code=http.GET();
    if (code!=200) {
        if (code < 0) {
            snprintf(error,errorSize,"HTTPS: %s",http.errorToString(code).c_str());
        } else {
            snprintf(error,errorSize,"HTTP %d",code);
        }
        http.end();
        return false;
    }
    if (http.getSize()>32768) { http.end(); return fail("API response too large"); }
    DynamicJsonDocument doc(32768); // Bounded JSON allocation, once per refresh.
    const auto parse=deserializeJson(doc,http.getStream(),DeserializationOption::NestingLimit(4));
    http.end();
    if (parse) return fail(parse.c_str());
    JsonArray rows=doc["prices"].as<JsonArray>();
    if (rows.isNull() || rows.size()==0 || rows.size()>maxPrices) return fail("Missing/oversized price array");
    result.count=0;
    for (JsonObject row: rows) {
        time_t epoch;
        if (!row["value"].is<float>() || !parseUtc(row["date"] | "",epoch)) return fail("Invalid price row");
        float value=row["value"].as<float>();
        if (!std::isfinite(value)) return fail("Non-finite price");
        // The API may include the end boundary; it belongs to the following day.
        if (epoch<start || epoch>=end) continue;
        result.values[result.count++]={epoch,value};
    }
    std::sort(result.values,result.values+result.count,[](const PriceInterval& a,const PriceInterval& b){return a.start<b.start;});
    for (size_t i=1;i<result.count;++i)
        if (result.values[i].start-result.values[i-1].start!=intervalSeconds) return fail("Duplicate or missing quarter-hour");
    if (!currentPrice(result,now)) return fail("Current price missing");
    result.fetchedAt=now;
    error[0]=0;
    return true;
}
