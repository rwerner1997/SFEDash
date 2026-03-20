#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// SFEDash_config.h — Hardware & app configuration
//
// PlatformIO passes these via -D build_flags in platformio.ini.
// Arduino IDE does not read platformio.ini, so the defaults below are used
// instead. All defines are guarded with #ifndef so PlatformIO values win.
//
// Include this file BEFORE <TFT_eSPI.h> so the driver selection and pin
// definitions are in scope when TFT_eSPI.h is parsed.
// ─────────────────────────────────────────────────────────────────────────────

// ── TFT_eSPI: tell the library to use our defines instead of User_Setup.h ───
#ifndef USER_SETUP_LOADED
#define USER_SETUP_LOADED
#endif

// ── TFT driver ───────────────────────────────────────────────────────────────
#ifndef ILI9488_DRIVER
#define ILI9488_DRIVER
#endif

// ── Screen dimensions (portrait native; rotation 1 = landscape in firmware) ──
#ifndef TFT_WIDTH
#define TFT_WIDTH  320
#endif
#ifndef TFT_HEIGHT
#define TFT_HEIGHT 480
#endif

// ── SPI GPIO pins ─────────────────────────────────────────────────────────────
#ifndef TFT_MOSI
#define TFT_MOSI 23
#endif
#ifndef TFT_SCLK
#define TFT_SCLK 18
#endif
#ifndef TFT_CS
#define TFT_CS   15
#endif
#ifndef TFT_DC
#define TFT_DC    2
#endif
#ifndef TFT_RST
#define TFT_RST   4
#endif
#ifndef TFT_BL
#define TFT_BL   32
#endif
#ifndef TFT_BACKLIGHT_ON
#define TFT_BACKLIGHT_ON HIGH
#endif

// ── Fonts ─────────────────────────────────────────────────────────────────────
#ifndef LOAD_GLCD
#define LOAD_GLCD
#endif
#ifndef LOAD_FONT2
#define LOAD_FONT2
#endif
#ifndef LOAD_FONT4
#define LOAD_FONT4
#endif
#ifndef LOAD_FONT6
#define LOAD_FONT6
#endif
#ifndef LOAD_FONT7
#define LOAD_FONT7
#endif
#ifndef LOAD_FONT8
#define LOAD_FONT8
#endif
#ifndef LOAD_GFXFF
#define LOAD_GFXFF
#endif
#ifndef SMOOTH_FONT
#define SMOOTH_FONT
#endif

// ── SPI clock speeds ──────────────────────────────────────────────────────────
#ifndef SPI_FREQUENCY
#define SPI_FREQUENCY      27000000
#endif
#ifndef SPI_READ_FREQUENCY
#define SPI_READ_FREQUENCY 20000000
#endif

// ── No touchscreen on this build ─────────────────────────────────────────────
// Defining TOUCH_CS as -1 suppresses TFT_eSPI's "TOUCH_CS not defined" warning.
#ifndef TOUCH_CS
#define TOUCH_CS -1
#endif

// ── App config ────────────────────────────────────────────────────────────────
#ifndef IR_RECV_PIN
#define IR_RECV_PIN 34
#endif
#ifndef BT_DEVICE_NAME
#define BT_DEVICE_NAME "OBDII"
#endif
