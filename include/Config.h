#pragma once
#include <stdint.h>
namespace Config {
enum class UiLanguage : uint8_t { English, Finnish };
// Select the on-screen language here, then rebuild/upload the firmware.
constexpr UiLanguage uiLanguage = UiLanguage::Finnish;
constexpr char timezone[] = "EET-2EEST,M3.5.0/3,M10.5.0/4";
constexpr uint32_t screenTimeoutMs = 30000;
constexpr uint8_t brightness = 100; // PWM duty, 0..255
constexpr uint8_t rotation = 3;     // 1 reverses the landscape orientation
constexpr float cheapPrice = 5.0f;  // VAT-inclusive c/kWh; user preferences
constexpr float expensivePrice = 15.0f;
}
