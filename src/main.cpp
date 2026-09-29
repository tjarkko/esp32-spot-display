#include <Arduino.h>
#include <WiFi.h>

#include "secrets.h"
#include "AppState.h"
#include "Display.h"
#include "PriceService.h"
#include "TimeService.h"

namespace {
AppState sharedState;
AppState displayState;
SemaphoreHandle_t stateMutex;

void setMessage(const char* value) {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    snprintf(sharedState.message, sizeof(sharedState.message), "%s", value);
    xSemaphoreGive(stateMutex);
    Serial.printf("[App] %s\n", value);
}

void networkTask(void*) {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    WiFi.begin(Secrets::WIFI_SSID, Secrets::WIFI_PASSWORD);

    uint32_t lastWifiAttempt = millis();
    uint32_t lastPriceAttempt = 0;
    uint32_t refreshInterval = 0;
    bool priceAttempted = false;
    bool wasOnline = false;
    bool clockLogged = false;
    static PriceTable candidate;

    for (;;) {
        const uint32_t currentMillis = millis();
        const bool online = WiFi.status() == WL_CONNECTED;
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        sharedState.wifiConnected = online;
        sharedState.signalDbm = online ? WiFi.RSSI() : 0;
        xSemaphoreGive(stateMutex);

        if (online && !wasOnline) {
            Serial.printf("[WiFi] Connected; IP %s; RSSI %d dBm\n",
                          WiFi.localIP().toString().c_str(), WiFi.RSSI());
            setMessage(TimeService::ready() ? "Fetching prices" : "Waiting for NTP");
        } else if (!online && wasOnline) {
            setMessage("Wi-Fi connection lost");
        }
        if (!online && currentMillis - lastWifiAttempt >= 30000) {
            setMessage("Retrying Wi-Fi");
            WiFi.disconnect();
            WiFi.begin(Secrets::WIFI_SSID, Secrets::WIFI_PASSWORD);
            lastWifiAttempt = currentMillis;
        }
        wasOnline = online;

        const time_t now = time(nullptr);
        if (TimeService::ready() && !clockLogged) {
            char stamp[40];
            TimeService::format(now, stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S %Z");
            Serial.printf("[Time] Synced: %s\n", stamp);
            clockLogged = true;
        }

        if (online && TimeService::ready() &&
            (!priceAttempted || currentMillis - lastPriceAttempt >= refreshInterval)) {
            priceAttempted = true;
            lastPriceAttempt = currentMillis;
            xSemaphoreTake(stateMutex, portMAX_DELAY);
            sharedState.fetching = true;
            xSemaphoreGive(stateMutex);

            char error[80];
            if (fetchPrices(candidate, now, error, sizeof(error))) {
                xSemaphoreTake(stateMutex, portMAX_DELAY);
                sharedState.prices = candidate;
                xSemaphoreGive(stateMutex);
                setMessage("Prices ready");

                tm local{};
                localtime_r(&now, &local);
                const time_t tomorrowEnd = TimeService::midnight(now, 2);
                const bool tomorrowComplete = candidate.count > 0 &&
                    candidate.values[candidate.count - 1].start + intervalSeconds >= tomorrowEnd;
                refreshInterval = !tomorrowComplete && local.tm_hour >= 14 ? 1800000 : 3600000;
                Serial.printf("[Prices] %u quarters; VAT-inclusive c/kWh; refresh in %lu min\n",
                              static_cast<unsigned>(candidate.count),
                              static_cast<unsigned long>(refreshInterval / 60000));
            } else {
                setMessage(error);
                refreshInterval = 300000;
            }
            xSemaphoreTake(stateMutex, portMAX_DELAY);
            sharedState.fetching = false;
            xSemaphoreGive(stateMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
}

void setup() {
    Serial.begin(115200);
    TimeService::begin();
    Display::begin();
    stateMutex = xSemaphoreCreateMutex();
    if (!stateMutex || xTaskCreate(networkTask, "network", 16384, nullptr, 1, nullptr) != pdPASS) {
        Serial.println("[Fatal] Cannot start network task");
        for (;;) delay(1000);
    }
}

void loop() {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    displayState = sharedState;
    xSemaphoreGive(stateMutex);

    const time_t now = time(nullptr);
    Display::tick(displayState, now);

    static uint32_t lastLog = 0;
    if (millis() - lastLog >= 30000) {
        lastLog = millis();
        Serial.printf("[Status] Wi-Fi %s | clock %s | %s | free heap %u\n",
                      displayState.wifiConnected ? "on" : "off",
                      TimeService::ready() ? "synced" : "waiting",
                      displayState.message, ESP.getFreeHeap());
        const PriceInterval* current = currentPrice(displayState.prices, now);
        if (!current) {
            Serial.println("[Price] Current interval unavailable");
        } else {
            for (int index = 0; index < 5; ++index) {
                const PriceInterval* price = currentPrice(
                    displayState.prices, current->start + index * intervalSeconds);
                if (!price) break;
                char label[32];
                TimeService::format(price->start, label, sizeof(label), "%d.%m %H:%M %Z");
                Serial.printf("[Price] %s %s: %.3f c/kWh\n",
                              index == 0 ? "NOW" : "next", label, price->centsPerKWh);
            }
        }
    }
    delay(10);
}
