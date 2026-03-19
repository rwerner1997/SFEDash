# SFEDash — ESP32 Hardware Setup

**Target vehicle:** 2015+ Subaru WRX (FA20DIT, CVT)
**OBD adapter:** Veepeak OBDII Bluetooth (device name `OBDII`, PIN `1234`)

---

## Hardware Required

| Part | Notes |
|------|-------|
| ESP32 DevKit (WROOM-32U recommended) | 38-pin dev board; WROOM-32U has external antenna connector for better BT range in-car |
| ILI9488 480×320 TFT display (SPI) | 3.5" landscape; must be ILI9488 — **not** ILI9341 |
| HX1838 IR receiver module | NEC-protocol; comes with the common D-pad mini remote |
| NEC D-pad IR remote | See button map below |
| Micro-USB cable + 5V power | Powered from car USB port or 12V→5V regulator |

---

## Wiring

### TFT Display → ESP32

| TFT pin | ESP32 GPIO | Notes |
|---------|-----------|-------|
| MOSI (SDI) | 23 | SPI data |
| SCLK (SCK) | 18 | SPI clock |
| CS (SS) | 15 | Chip select |
| DC (RS) | 2 | Data/command |
| RST | 4 | Reset |
| BL (LED) | 32 | Backlight — wire directly; driven HIGH at boot |
| VCC | 3.3 V | Some modules have a 3.3 V reg onboard; check your module |
| GND | GND | |

> **Note:** MISO is not connected — TFT_eSPI is write-only for this display.

### IR Receiver (HX1838) → ESP32

| HX1838 pin | ESP32 GPIO | Notes |
|-----------|-----------|-------|
| DATA (S) | 34 | GPIO 34 is input-only — ideal for IR, no accidental drive conflicts |
| VCC | 3.3 V | |
| GND | GND | |

---

## Arduino IDE Setup

### 1. Install ESP32 board support

In Arduino IDE: **File → Preferences → Additional boards manager URLs**, add:

```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

Then **Tools → Board → Boards Manager**, search `esp32`, install **ESP32 by Espressif Systems**.

Select board: **ESP32 Dev Module**
Upload speed: **921600**
Partition scheme: **Default 4MB with spiffs** (or any scheme with ≥1.2 MB app)

### 2. Install libraries

In **Sketch → Include Library → Manage Libraries**:

| Library | Author | Tested version |
|---------|--------|---------------|
| TFT_eSPI | Bodmer | 2.5.43+ |
| IRremoteESP8266 | crankyoldgit | 2.8.x |

### 3. Configure TFT_eSPI

TFT_eSPI requires a one-time edit to its `User_Setup.h` before it will compile correctly.

Find the file — typically:
`~/Arduino/libraries/TFT_eSPI/User_Setup.h`

Make the following changes (comment out the default driver and uncomment/add these):

```cpp
// Driver
#define ILI9488_DRIVER

// Dimensions
#define TFT_WIDTH  320
#define TFT_HEIGHT 480

// Pins
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4
#define TFT_BL   32
#define TFT_BACKLIGHT_ON HIGH

// SPI speed
#define SPI_FREQUENCY      40000000
#define SPI_READ_FREQUENCY 20000000

// Fonts (add/uncomment all of these)
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT
```

> **Important:** Every time you update the TFT_eSPI library, `User_Setup.h` gets overwritten. Re-apply these settings after any library update.

### 4. Open the sketch

Open `esp32/src/main.cpp` — Arduino IDE expects a `.ino` file. Either:

- Rename `main.cpp` to `SFEDash.ino` inside a folder named `SFEDash`, **or**
- Create a minimal `SFEDash.ino` in the same folder that just `#include`s `main.cpp`

All `.cpp` and `.h` files in the same folder are compiled automatically by Arduino IDE.

### 5. Flash

Select the correct COM port, click **Upload**. Open **Serial Monitor at 115200 baud** to see connection status and IR debug output.

---

## IR Remote Button Map

Uses the common **HX1838 D-pad remote** (NEC protocol). If your remote uses different codes, enable `#define SERIAL_IR_DEBUG` in `main.cpp` and read the hex values from Serial Monitor, then update the `#define IR_*` constants.

| Button | Function |
|--------|----------|
| ◄ LEFT | Previous page |
| ► RIGHT | Next page |
| ▲ UP | Previous theme |
| ▼ DOWN | Next theme |
| OK | Toggle drive mode (minimal overlay) |
| 1–9 | Jump to page 1–9 directly |
| 9 | Reset peaks + averages |
| ★ (star) | Dismiss alert |
| # (hash) | Mute alerts for 5 minutes |

---

## Pages (0–9)

| # | Name | Description |
|---|------|-------------|
| 0 | ENGINE | RPM arc (hero), MAF, STFT, LTFT, Load |
| 1 | TEMPS | Coolant arc, Oil, CVT, CAT, Fuel level |
| 2 | BOOST | Boost arc + animated turbo wheel, Wastegate, STFT, Fine Knock |
| 3 | FUEL | STFT arc (hero), LTFT, MAF, Inj Duty, Inj Pulse width |
| 4 | VITALS | FA20DIT boxer diagram (animated pistons), Load, Throttle, Oil, VVT-L |
| 5 | ROUGHNESS | 4-cylinder roughness bars (colour-coded) |
| 6 | G-FORCE | Longitudinal G dot plot (120-sample rolling history) |
| 7 | TIMING | Timing arc, Fine Knock, DAM, STFT, Load |
| 8 | AVERAGES | Running trip averages (3×3 grid) with live "now" sub-values |
| 9 | SESSION | Peak values: Boost, RPM, Timing, Load, Speed, CVT, MAF, HP, CAT |

Side strips (always visible): RPM bar (left), Speed MPH (right).
Status bar (bottom): BT status, gear position, poll Hz, page name, theme, page dots.

---

## OBD Polling Summary

| Tier | Rate | Header | PIDs |
|------|------|--------|------|
| 1 | ~20 Hz | 7DF/7E8 | RPM (010C), MAP (010B) |
| 2 | ~7 Hz | 7DF/7E8 + 7E0/7E8 | Speed, Timing, MAF, Pedal, Load, STFT, LTFT, Wastegate (2210A8), Fine Knock (2210B0), VVT-L (2210B9) |
| 3a | ~1 Hz | 7DF/7E8 + 7E1/7E9 | Coolant, Oil, CAT, Baro, Fuel, Battery, CVT Temp (2210D2), Shift Selector (221093+221095) |
| 3d | ~1 Hz | 7E0/7E8 | DAM (2210B1), Inj Duty (2210C1), Inj Pulse (2210B4) |

Roughness (223062/223048/223068/22304A) is polled only when on the VITALS or ROUGHNESS page.

---

## Themes

Cycle with ▲/▼: **HUD TEAL** → **AMBER** → **RACING RED** → **MATRIX** → **NEON PURPLE** → **STEALTH**
