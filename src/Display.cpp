#include "Display.h"

#include "Config.h"
#include "Localization.h"
#include "TimeService.h"

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#if defined(TOUCH_ENABLED)
#include <TouchDrvCST.hpp>
#include <Wire.h>
#endif

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

#if defined(TOUCH_ENABLED)
constexpr uint8_t touchSdaPin = 18;
constexpr uint8_t touchSclPin = 17;
constexpr uint8_t touchInterruptPin = 16;
constexpr uint8_t touchResetPin = 21;
TouchDrvCSTXXX touch;
bool touchReady = false;
bool touchWasPressed = false;

void beginTouch() {
    touch.setPins(touchResetPin, touchInterruptPin);
    touch.setTouchDrvModel(TouchDrv_CST8XX);
    touchReady = touch.begin(Wire, CST816_SLAVE_ADDRESS, touchSdaPin, touchSclPin);
    if (!touchReady) {
        Serial.println("[Touch] Controller not detected; physical buttons remain available");
        return;
    }
    Serial.printf("[Touch] %s detected\n", touch.getModelName());
    touch.disableAutoSleep();
    touch.setMaxCoordinates(320, 170);
    touch.setMirrorXY(true, false);
    touch.setSwapXY(true);
}

bool touchPressEdge() {
    if (!touchReady) return false;
    const bool pressed = touch.getTouchPoints().hasPoints();
    const bool edge = pressed && !touchWasPressed;
    touchWasPressed = pressed;
    return edge;
}
#endif

uint16_t priceColor(float price) {
    if (price < Config::cheapPrice) return green;
    if (price >= Config::expensivePrice) return red;
    return yellow;
}

void makeDisplayText(const char* input, char* output, size_t outputSize, int font) {
    size_t out = 0;
    for (size_t in = 0; input[in] && out + 1 < outputSize; ++in) {
        const uint8_t first = static_cast<uint8_t>(input[in]);
        if (first == 0xC3 && input[in + 1]) {
            const uint8_t second = static_cast<uint8_t>(input[++in]);
            char base = '?';
            uint8_t cp437 = '?';
            switch (second) {
                case 0x84: base = 'A'; cp437 = 0x8E; break; // Ä
                case 0x85: base = 'A'; cp437 = 0x8F; break; // Å
                case 0x96: base = 'O'; cp437 = 0x99; break; // Ö
                case 0xA4: base = 'a'; cp437 = 0x84; break; // ä
                case 0xA5: base = 'a'; cp437 = 0x86; break; // å
                case 0xB6: base = 'o'; cp437 = 0x94; break; // ö
            }
            output[out++] = static_cast<char>(font == 1 ? cp437 : base);
        } else if ((first & 0xC0) == 0x80) {
            continue;
        } else {
            output[out++] = input[in];
        }
    }
    output[out] = '\0';
}

void drawTextOnBackground(const char* value, int x, int y, uint16_t color,
                          uint16_t textBackground, int font) {
    char rendered[160];
    makeDisplayText(value, rendered, sizeof(rendered), font);
    canvas.setAttribute(CP437_SWITCH, 1);
    canvas.setAttribute(UTF8_SWITCH, 0);
    canvas.setTextColor(color, textBackground);
    canvas.drawString(rendered, x, y, font);
    canvas.setAttribute(UTF8_SWITCH, 1);
    canvas.setAttribute(CP437_SWITCH, 0);
}

void drawText(const char* value, int x, int y, uint16_t color = foreground, int font = 2) {
    drawTextOnBackground(value, x, y, color, background, font);
}

void replaceDecimalSeparator(char* value) {
    if (Config::uiLanguage != Config::UiLanguage::Finnish) return;
    for (char* cursor = value; *cursor; ++cursor) {
        if (*cursor == '.') {
            *cursor = ',';
            return;
        }
    }
}

void drawHeader(time_t now) {
    char stamp[32];
    drawText("FI / SPOT", 10, 4, green);
    TimeService::format(now, stamp, sizeof(stamp), "%d.%m %H:%M %Z");
    canvas.setTextDatum(TR_DATUM);
    drawText(TimeService::ready() ? stamp : Ui::text(Ui::Text::SyncingClock), 310, 4, muted, 1);
    canvas.setTextDatum(TL_DATUM);
    canvas.drawFastHLine(10, 23, 300, panel);
}

void drawWaiting(const AppState& state) {
    drawText(TimeService::ready() ? Ui::text(Ui::Text::WaitingForPrices)
                                  : Ui::text(Ui::Text::SettingClock), 12, 43, foreground, 4);
    drawText(state.wifiConnected ? Ui::text(Ui::Text::WifiConnected)
                                 : Ui::text(Ui::Text::ConnectingWifi), 12, 86, muted);
    drawText(Ui::appMessage(state.message), 12, 113, yellow, 1);
    drawText(Ui::text(Ui::Text::UpdatesContinue), 12, 137, muted, 1);
}

void drawOverview(const AppState& state, const PriceInterval& current) {
    char text[96];
    char start[12];
    char end[12];

    snprintf(text, sizeof(text), "%.2f", current.centsPerKWh);
    replaceDecimalSeparator(text);
    drawText(text, 10, 29, priceColor(current.centsPerKWh), 6);
    drawText(Ui::text(Ui::Text::PriceUnit), 206, 33, muted);
    drawText(Ui::text(Ui::Text::VatIncluded), 206, 52, muted, 1);

    TimeService::format(current.start, start, sizeof(start));
    TimeService::format(current.start + intervalSeconds, end, sizeof(end));
    const char* level = current.centsPerKWh < Config::cheapPrice
                            ? Ui::text(Ui::Text::Cheap)
                            : current.centsPerKWh >= Config::expensivePrice
                                  ? Ui::text(Ui::Text::Expensive)
                                  : Ui::text(Ui::Text::Moderate);
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
    drawText(Ui::text(Ui::Text::Now), 10, 134, muted, 1);
    drawText("+12h", 144, 134, muted, 1);
    drawText("+24h", 277, 134, muted, 1);
    snprintf(text, sizeof(text), Ui::text(Ui::Text::ChartSummary),
             static_cast<unsigned>(stats.count), stats.min, stats.max);
    replaceDecimalSeparator(text);
    drawText(text, 10, 145, foreground, 1);
}

void drawUpcoming(const AppState& state, const PriceInterval& current) {
    char text[48];
    char stamp[20];
    drawText(Ui::text(Ui::Text::UpcomingQuarters), 10, 30, green, 1);
    for (int index = 0; index < 5; ++index) {
        const time_t start = current.start + (index + 1) * intervalSeconds;
        const PriceInterval* price = currentPrice(state.prices, start);
        TimeService::format(start, stamp, sizeof(stamp), "%H:%M %Z");
        drawText(stamp, 10, 46 + index * 20, muted);
        if (price) {
            snprintf(text, sizeof(text), "%7.2f %s", price->centsPerKWh,
                     Ui::text(Ui::Text::PriceUnit));
            replaceDecimalSeparator(text);
            drawText(text, 154, 46 + index * 20, priceColor(price->centsPerKWh));
        } else {
            drawText(Ui::text(Ui::Text::NotPublished), 154, 46 + index * 20, muted);
        }
    }
}

void drawToday(const AppState& state, time_t now) {
    char text[96];
    char stamp[32];
    const time_t start = TimeService::midnight(now);
    const time_t end = TimeService::midnight(now, 1);
    const PriceStats stats = priceStats(state.prices, start, end);
    snprintf(text, sizeof(text), Ui::text(Ui::Text::TodaySummary),
             static_cast<unsigned>(stats.count), static_cast<unsigned>((end - start) / intervalSeconds));
    drawText(text, 10, 32, green, 1);
    if (!stats.count) {
        drawText(Ui::text(Ui::Text::NoPricesAvailable), 10, 64, yellow, 4);
        return;
    }
    TimeService::format(stats.minAt, stamp, sizeof(stamp), "%H:%M %Z");
    snprintf(text, sizeof(text), "%s %7.2f   %s", Ui::text(Ui::Text::Minimum), stats.min, stamp);
    replaceDecimalSeparator(text);
    drawText(text, 10, 50, green);
    TimeService::format(stats.maxAt, stamp, sizeof(stamp), "%H:%M %Z");
    snprintf(text, sizeof(text), "%s %7.2f   %s", Ui::text(Ui::Text::Maximum), stats.max, stamp);
    replaceDecimalSeparator(text);
    drawText(text, 10, 74, red);
    snprintf(text, sizeof(text), "%s %7.2f %s", Ui::text(Ui::Text::Average), stats.average,
             Ui::text(Ui::Text::PriceUnit));
    replaceDecimalSeparator(text);
    drawText(text, 10, 98, foreground);
    snprintf(text, sizeof(text), "Wi-Fi %s  RSSI %d dBm",
             state.wifiConnected ? Ui::text(Ui::Text::WifiOn) : Ui::text(Ui::Text::WifiOff),
             state.signalDbm);
    drawText(text, 10, 124, muted, 1);
    TimeService::format(state.prices.fetchedAt, stamp, sizeof(stamp), "%d.%m %H:%M");
    snprintf(text, sizeof(text), Ui::text(Ui::Text::Fetched), stamp);
    drawText(text, 10, 140, muted, 1);
}

void drawFooter(const AppState& state, time_t now) {
    const Ui::Text status = !state.wifiConnected ? Ui::Text::Offline
                            : state.fetching      ? Ui::Text::Updating
                            : state.prices.fetchedAt && now - state.prices.fetchedAt > 7200
                                  ? Ui::Text::Stale
                            : strcmp(state.message, "Prices ready") == 0 ? Ui::Text::Live
                                                                          : Ui::Text::Retry;
    const uint32_t elapsed = millis() - lastInteraction;
    const uint32_t secondsLeft = elapsed < Config::screenTimeoutMs
                                     ? (Config::screenTimeoutMs - elapsed + 999) / 1000
                                     : 0;
    char text[80];
    snprintf(text, sizeof(text), "%s | %s  %lus", Ui::text(status),
             Ui::text(Ui::Text::FooterControls), static_cast<unsigned long>(secondsLeft));
    canvas.fillRect(0, 157, 320, 13, panel);
    drawTextOnBackground(text, 10, 159, muted, panel, 1);
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
#if defined(TOUCH_ENABLED)
    beginTouch();
#endif

    tft.init();
    tft.setRotation(Config::rotation);
    tft.fillScreen(background);
    canvas.setColorDepth(16);
    spriteReady = canvas.createSprite(320, 170) != nullptr;

    ledcSetup(0, 5000, 8);
    ledcAttachPin(backlightPin, 0);
    ledcWrite(0, 0);
    tft.writecommand(0x28);
    Serial.printf("[Display] Ready; framebuffer %s. Press either button to wake%s.\n",
                  spriteReady ? "OK" : "FAILED",
#if defined(TOUCH_ENABLED)
                  " or tap the screen"
#else
                  ""
#endif
    );
}

void Display::tick(const AppState& state, time_t now) {
    const uint32_t currentMillis = millis();
    const bool leftPressed = leftButton.pressed(currentMillis);
    const bool rightPressed = rightButton.pressed(currentMillis);
#if defined(TOUCH_ENABLED)
    const bool tapped = touchPressEdge();
#else
    constexpr bool tapped = false;
#endif

    if (leftPressed || rightPressed || tapped) {
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
        char rendered[64];
        makeDisplayText(Ui::text(Ui::Text::DisplayAllocationFailed), rendered, sizeof(rendered), 2);
        tft.setAttribute(CP437_SWITCH, 1);
        tft.setAttribute(UTF8_SWITCH, 0);
        tft.setTextColor(TFT_WHITE);
        tft.drawString(rendered, 5, 50, 2);
        tft.setAttribute(UTF8_SWITCH, 1);
        tft.setAttribute(CP437_SWITCH, 0);
    }
    ledcWrite(0, Config::brightness);
}
