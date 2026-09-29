#pragma once

#include "Config.h"

#include <cstring>

namespace Ui {
enum class Text : uint8_t {
    SyncingClock,
    WaitingForPrices,
    SettingClock,
    WifiConnected,
    ConnectingWifi,
    UpdatesContinue,
    PriceUnit,
    VatIncluded,
    Cheap,
    Moderate,
    Expensive,
    Now,
    ChartSummary,
    UpcomingQuarters,
    NotPublished,
    TodaySummary,
    NoPricesAvailable,
    Minimum,
    Maximum,
    Average,
    WifiOn,
    WifiOff,
    Fetched,
    Offline,
    Updating,
    Stale,
    Live,
    Retry,
    FooterControls,
    DisplayAllocationFailed
};

inline const char* text(Text id) {
    const bool fi = Config::uiLanguage == Config::UiLanguage::Finnish;
    switch (id) {
        case Text::SyncingClock: return fi ? "SYNKRONOIDAAN KELLO" : "SYNCING CLOCK";
        case Text::WaitingForPrices: return fi ? "Hintoja haetaan" : "Waiting for prices";
        case Text::SettingClock: return fi ? "Asetetaan kello" : "Setting the clock";
        case Text::WifiConnected: return fi ? "Wi-Fi yhdistetty" : "Wi-Fi connected";
        case Text::ConnectingWifi: return fi ? "Yhdistetaan Wi-Fi..." : "Connecting to Wi-Fi...";
        case Text::UpdatesContinue:
            return fi ? "Päivitykset jatkuvat kun näyttö on pimeänä"
                      : "Updates continue while screen is off";
        case Text::PriceUnit: return fi ? "snt/kWh" : "c/kWh";
        case Text::VatIncluded: return fi ? "sis. ALV" : "VAT incl.";
        case Text::Cheap: return fi ? "EDULLINEN" : "CHEAP";
        case Text::Moderate: return fi ? "KESKIHINTA" : "MODERATE";
        case Text::Expensive: return fi ? "KALLIS" : "EXPENSIVE";
        case Text::Now: return fi ? "NYT" : "NOW";
        case Text::ChartSummary: return fi ? "24 h hinnat %u/96  MIN %.1f MAX %.1f"
                                           : "24h known %u/96  MIN %.1f MAX %.1f";
        case Text::UpcomingQuarters: return fi ? "TULEVAT VARTIT" : "NEXT QUARTERS";
        case Text::NotPublished: return fi ? "Ei julkaistu" : "Not published";
        case Text::TodaySummary: return fi ? "TÄNÄÄN %u/%u varttia"
                                           : "TODAY / %u of %u quarters";
        case Text::NoPricesAvailable: return fi ? "Hintoja ei saatavilla" : "No prices available";
        case Text::Minimum: return fi ? "HALVIN" : "MIN";
        case Text::Maximum: return fi ? "KALLEIN" : "MAX";
        case Text::Average: return fi ? "KA" : "AVG";
        case Text::WifiOn: return fi ? "paalla" : "on";
        case Text::WifiOff: return fi ? "pois" : "off";
        case Text::Fetched: return fi ? "Haettu %s / sahkotin.fi" : "Fetched %s / sahkotin.fi";
        case Text::Offline: return fi ? "EI VERKKOYHTEYTTÄ" : "OFFLINE";
        case Text::Updating: return fi ? "PÄIVITETÄÄN" : "UPDATING";
        case Text::Stale: return fi ? "VANHA" : "STALE";
        case Text::Live: return fi ? "AJANTASALLA" : "LIVE";
        case Text::Retry: return fi ? "YRITETÄÄN" : "RETRY";
        case Text::FooterControls:
            return fi ? "oik: sivu  vas: pois" : "right: next  left: off";
        case Text::DisplayAllocationFailed:
            return fi ? "Muistivirhe" : "Display allocation failed";
    }
    return "";
}

inline const char* appMessage(const char* message) {
    if (Config::uiLanguage == Config::UiLanguage::English) return message;
    if (strcmp(message, "Connecting to Wi-Fi") == 0) return "Yhdistetään Wi-Fi...";
    if (strcmp(message, "Waiting for NTP") == 0) return "Odotetaan kellon synkronointia";
    if (strcmp(message, "Fetching prices") == 0) return "Haetaan sähkön hintoja";
    if (strcmp(message, "Wi-Fi connection lost") == 0) return "Wi-Fi-yhteys katkesi";
    if (strcmp(message, "Retrying Wi-Fi") == 0) return "Yritetään Wi-Fi-yhteyttä";
    if (strcmp(message, "Prices ready") == 0) return "Hinnat haettu";
    if (strncmp(message, "HTTPS:", 6) == 0) return "HTTPS-yhteysvirhe";
    if (strncmp(message, "HTTP ", 5) == 0) return "Palvelinvirhe";
    return "Hintojen haussa virhe";
}
} // namespace Ui
