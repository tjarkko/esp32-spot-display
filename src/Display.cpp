#include "Display.h"

#include "Config.h"
#include "TimeService.h"

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
TFT_eSPI tft;
TFT_eSprite canvas(&tft);

constexpr uint8_t powerPin = 15;
constexpr uint8_t backlightPin = 38;
constexpr uint8_t leftButtonPin = 0;
constexpr uint8_t rightButtonPin = 14;

constexpr uint16_t background = 0x0842;
constexpr uint16_t panel = 0x10C4;
constexpr uint16_t muted = 0x8CB3;
constexpr uint16_t foreground = 0xEF7D;
constexpr uint16_t green = 0x4F17;
constexpr uint16_t yellow = 0xFE68;
constexpr uint16_t red = 0xFAAA;

bool screenAwake = false;
bool spriteReady = false;
uint8_t page = 0;
uint32_t lastInteraction = 0;
uint32_t lastDraw = 0;

struct Button {
    uint8_t pin;
    bool raw = true;
    bool stable = true;
    uint32_t changedAt = 0;

    explicit Button(uint8_t buttonPin) : pin(buttonPin) {}

    bool pressed(uint32_t now) {
        const bool value = digitalRead(pin);
        if (value != raw) {
            raw = value;
            changedAt = now;
        }
        if (now - changedAt >= 35 && stable != raw) {
            stable = raw;
            return !stable;
        }
        return false;
    }
};

Button leftButton{leftButtonPin};
Button rightButton{rightButtonPin};

uint16_t priceColor(float price) {
    if (price < Config::cheapPrice) return green;
    if (price >= Config::expensivePrice) return red;
    return yellow;
}

void drawText(const char* value, int x, int y, uint16_t color = foreground, int font = 2) {
    canvas.setTextColor(color, background);
    canvas.drawString(value, x, y, font);
}

void drawHeader(time_t now) {
    char stamp[32];
    drawText("FI / SPOT", 10, 4, green);
    TimeService::format(now, stamp, sizeof(stamp), "%d.%m %H:%M %Z");
    canvas.setTextDatum(TR_DATUM);
    drawText(TimeService::ready() ? stamp : "SYNCING CLOCK", 310, 4, muted, 1);
    canvas.setTextDatum(TL_DATUM);
    canvas.drawFastHLine(10, 23, 300, panel);
}

void drawWaiting(const AppState& state) {
    drawText(TimeService::ready() ? "Waiting for prices" : "Setting the clock", 12, 43, foreground, 4);
    drawText(state.wifiConnected ? "Wi-Fi connected" : "Connecting to Wi-Fi...", 12, 86, muted);
    drawText(state.message, 12, 113, yellow, 1);
    drawText("Updates continue while screen is off", 12, 137, muted, 1);
}

void drawOverview(const AppState& state, const PriceInterval& current) {
    char text[96];
    char start[12];
    char end[12];

    snprintf(text, sizeof(text), "%.2f", current.centsPerKWh);
    drawText(text, 10, 29, priceColor(current.centsPerKWh), 6);
    drawText("c/kWh", 206, 33, muted);
    drawText("VAT incl.", 206, 52, muted, 1);

    TimeService::format(current.start, start, sizeof(start));
    TimeService::format(current.start + intervalSeconds, end, sizeof(end));
    const char* level = current.centsPerKWh < Config::cheapPrice
                            ? "CHEAP"
                            : current.centsPerKWh >= Config::expensivePrice ? "EXPENSIVE" : "MODERATE";
    snprintf(text, sizeof(text), "%s-%s  %s", start, end, level);
    drawText(text, 10, 77, priceColor(current.centsPerKWh), 1);

    const time_t horizon = current.start + 86400;
    const PriceStats stats = priceStats(state.prices, current.start, horizon);
    const float low = std::min(0.0f, stats.min);
    float high = std::max(1.0f, stats.max);
    if (high - low < 1.0f) high = low + 1.0f;
    const auto yForPrice = [low, high](float price) {
        return 129 - static_cast<int>((price - low) / (high - low) * 34.0f);
    };
    const int zeroY = yForPrice(0.0f);
    canvas.drawFastHLine(10, zeroY, 288, muted);
    for (int slot = 0; slot < 96; ++slot) {
        const PriceInterval* price = currentPrice(state.prices, current.start + slot * intervalSeconds);
        if (!price) {
            canvas.drawPixel(10 + slot * 3, 131, panel);
            continue;
        }
        const int priceY = yForPrice(price->centsPerKWh);
        canvas.fillRect(10 + slot * 3, std::min(priceY, zeroY), 2,
                        std::max(1, abs(priceY - zeroY)), priceColor(price->centsPerKWh));
    }
    drawText("NOW", 10, 134, muted, 1);
    drawText("+12h", 144, 134, muted, 1);
    drawText("+24h", 277, 134, muted, 1);
    snprintf(text, sizeof(text), "24h known %u/96  MIN %.1f MAX %.1f",
             static_cast<unsigned>(stats.count), stats.min, stats.max);
    drawText(text, 10, 145, foreground, 1);
}

void drawUpcoming(const AppState& state, const PriceInterval& current) {
    char text[48];
    char stamp[20];
    drawText("NEXT QUARTERS", 10, 30, green, 1);
    for (int index = 0; index < 5; ++index) {
        const time_t start = current.start + (index + 1) * intervalSeconds;
        const PriceInterval* price = currentPrice(state.prices, start);
        TimeService::format(start, stamp, sizeof(stamp), "%H:%M %Z");
        drawText(stamp, 10, 46 + index * 20, muted);
        if (price) {
            snprintf(text, sizeof(text), "%7.2f c/kWh", price->centsPerKWh);
            drawText(text, 154, 46 + index * 20, priceColor(price->centsPerKWh));
        } else {
            drawText("Not published", 154, 46 + index * 20, muted);
        }
    }
}

void drawToday(const AppState& state, time_t now) {
    char text[96];
    char stamp[32];
    const time_t start = TimeService::midnight(now);
    const time_t end = TimeService::midnight(now, 1);
    const PriceStats stats = priceStats(state.prices, start, end);
    snprintf(text, sizeof(text), "TODAY / %u of %u quarters",
             static_cast<unsigned>(stats.count), static_cast<unsigned>((end - start) / intervalSeconds));
    drawText(text, 10, 32, green, 1);
    if (!stats.count) {
        drawText("No prices available", 10, 64, yellow, 4);
        return;
    }
    TimeService::format(stats.minAt, stamp, sizeof(stamp), "%H:%M %Z");
    snprintf(text, sizeof(text), "MIN %7.2f   %s", stats.min, stamp);
    drawText(text, 10, 50, green);
    TimeService::format(stats.maxAt, stamp, sizeof(stamp), "%H:%M %Z");
    snprintf(text, sizeof(text), "MAX %7.2f   %s", stats.max, stamp);
    drawText(text, 10, 74, red);
    snprintf(text, sizeof(text), "AVG %7.2f c/kWh", stats.average);
    drawText(text, 10, 98, foreground);
    snprintf(text, sizeof(text), "Wi-Fi %s  RSSI %d dBm",
             state.wifiConnected ? "on" : "off", state.signalDbm);
    drawText(text, 10, 124, muted, 1);
    TimeService::format(state.prices.fetchedAt, stamp, sizeof(stamp), "%d.%m %H:%M");
    snprintf(text, sizeof(text), "Fetched %s / sahkotin.fi", stamp);
    drawText(text, 10, 140, muted, 1);
}

void drawFooter(const AppState& state, time_t now) {
    const char* status = !state.wifiConnected ? "OFFLINE"
                         : state.fetching      ? "UPDATING"
                         : state.prices.fetchedAt && now - state.prices.fetchedAt > 7200 ? "STALE"
                         : strcmp(state.message, "Prices ready") == 0 ? "LIVE" : "RETRY";
    const uint32_t elapsed = millis() - lastInteraction;
    const uint32_t secondsLeft = elapsed < Config::screenTimeoutMs
                                     ? (Config::screenTimeoutMs - elapsed + 999) / 1000
                                     : 0;
    char text[80];
    snprintf(text, sizeof(text), "%s | right: next  left: off  %lus",
             status, static_cast<unsigned long>(secondsLeft));
    canvas.fillRect(0, 157, 320, 13, panel);
    canvas.setTextColor(muted, panel);
    canvas.drawString(text, 10, 159, 1);
}

void draw(const AppState& state, time_t now) {
    canvas.fillSprite(background);
    drawHeader(now);
    const PriceInterval* current = TimeService::ready() ? currentPrice(state.prices, now) : nullptr;
    if (!current) drawWaiting(state);
    else if (page == 0) drawOverview(state, *current);
    else if (page == 1) drawUpcoming(state, *current);
    else drawToday(state, now);
    drawFooter(state, now);
    canvas.pushSprite(0, 0);
}

void turnOff() {
    ledcWrite(0, 0);
    tft.writecommand(0x28);
    screenAwake = false;
    Serial.println("[Display] Off; background updates continue");
}

void turnOn(uint32_t now) {
    screenAwake = true;
    lastInteraction = now;
    page = 0;
    lastDraw = now - 1000;
    tft.writecommand(0x29);
    Serial.println("[Display] On");
}
}

void Display::begin() {
    pinMode(powerPin, OUTPUT);
    digitalWrite(powerPin, HIGH);
    pinMode(backlightPin, OUTPUT);
    digitalWrite(backlightPin, LOW);
    pinMode(leftButtonPin, INPUT_PULLUP);
    pinMode(rightButtonPin, INPUT_PULLUP);

    tft.init();
    tft.setRotation(Config::rotation);
    tft.fillScreen(background);
    canvas.setColorDepth(16);
    spriteReady = canvas.createSprite(320, 170) != nullptr;

    ledcSetup(0, 5000, 8);
    ledcAttachPin(backlightPin, 0);
    ledcWrite(0, 0);
    tft.writecommand(0x28);
    Serial.printf("[Display] Ready; framebuffer %s. Press either button to wake.\n",
                  spriteReady ? "OK" : "FAILED");
}

void Display::tick(const AppState& state, time_t now) {
    const uint32_t currentMillis = millis();
    const bool leftPressed = leftButton.pressed(currentMillis);
    const bool rightPressed = rightButton.pressed(currentMillis);

    if (leftPressed || rightPressed) {
        if (!screenAwake) {
            turnOn(currentMillis);
        } else if (leftPressed) {
            turnOff();
            return;
        } else {
            page = (page + 1) % 3;
            lastInteraction = currentMillis;
            lastDraw = currentMillis - 1000;
        }
    }
    if (!screenAwake) return;
    if (currentMillis - lastInteraction >= Config::screenTimeoutMs) {
        turnOff();
        return;
    }
    if (currentMillis - lastDraw < 1000) return;
    lastDraw = currentMillis;
    if (spriteReady) {
        draw(state, now);
    } else {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_WHITE);
        tft.drawString("Display allocation failed", 5, 50, 2);
    }
    ledcWrite(0, Config::brightness);
}
