# ESP32 spot-price display

Finnish day-ahead electricity price display for the LILYGO T-Display-S3
capacitive touch board. Prices come from Sähkötin as VAT-inclusive cents/kWh
at 15-minute resolution.

The screen stays dark while the device continues updating over Wi-Fi. Press
either physical button to wake it. The right button cycles through the current
price/chart, upcoming quarters and today's statistics. The left button turns
the screen off. It also turns off automatically after 30 seconds.

Touch support is optional at build time. The default build has it disabled and
does not include the touch-driver library. To build firmware for the capacitive
touch version, select the `lilygo-t-display-s3-touch` PlatformIO environment;
a screen tap wakes the display or advances to the next page. The physical
buttons continue to work in either build.

## VS Code

Open this directory in VS Code. Install the PlatformIO IDE extension and allow
its initial setup to finish. Use the PlatformIO toolbar Build, Upload and Serial
Monitor actions, or open **PlatformIO: New Terminal** from the Command Palette.
The PlatformIO terminal provides `pio` without changing your system shell.

```sh
pio run                    # Compile; no board needs to be connected
pio run -e lilygo-t-display-s3-touch  # Compile with capacitive touch enabled
pio device list            # Find the connected board
pio run --target upload    # Flash over USB
pio run -e lilygo-t-display-s3-touch --target upload # Flash touch-enabled build
pio device monitor         # Monitor at 115200 baud; Ctrl+C exits
```

For an ordinary terminal, after PlatformIO setup:

```sh
export PATH="$HOME/.platformio/penv/bin:$PATH"
cd ~/prj/git/esp32-spot-display
pio run
```

Use a USB data cable. If automatic upload fails, hold BOOT, press and release
RESET, then release BOOT and retry. Reset after upload if needed. Runtime status
and upcoming prices are also logged to Serial every 30 seconds.

## Project layout

- `platformio.ini`: pinned ESP32 platform, Arduino framework, board and USB serial settings.
- `src/main.cpp`: Wi-Fi lifecycle, background refresh and application loop.
- `src/PriceService.cpp`: Sähkötin HTTPS request and JSON validation.
- `src/TimeService.cpp`: NTP and Finnish EET/EEST timezone handling.
- `src/Display.cpp`: 320x170 UI, graph, buttons and screen timeout.
- `include/Config.h`: UI language, screen timeout, brightness and price thresholds.
- `include/Localization.h`: Finnish and English UI strings.
- `include/secrets.h`: local Wi-Fi credentials; excluded from Git.
- `test/`: reserved for tests as application logic is introduced.

The official `lilygo-t-display-s3` board definition supplies the board's flash
and PSRAM configuration. USB CDC flags route Serial through the ESP32-S3's
native USB connection. This is the standard T-Display-S3, not an AMOLED or Pro
variant. The display uses TFT_eSPI's setup for its 8-bit parallel ST7789 panel.

## Data and refresh behavior

The firmware requests the current and following Finnish calendar day over
HTTPS. UTC timestamps are retained internally and converted with the ESP32's
POSIX timezone support, including EET/EEST transitions. A failed refresh keeps
the previous valid table. Normal refreshes run hourly; after 14:00, missing
tomorrow prices are checked every 30 minutes. Failures retry after five minutes.

Cheap and expensive thresholds, display brightness and the 30-second timeout
can be adjusted in `include/Config.h`. The screen defaults to Finnish; change
`Config::uiLanguage` there to `Config::UiLanguage::English` and rebuild to use
English. Finnish special characters use the display's extended small font;
larger built-in fonts use readable ASCII spellings.

Dependencies belong in `platformio.ini`; no globally installed Arduino libraries
are needed. The ESP32 platform version is pinned for repeatable builds.

References: [PlatformIO VS Code guide](https://docs.platformio.org/en/latest/integration/ide/vscode.html),
[LILYGO examples](https://github.com/Xinyuan-LilyGO/T-Display-S3).
