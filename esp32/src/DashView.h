#pragma once
#include <TFT_eSPI.h>
#include "DashData.h"

// ─────────────────────────────────────────────────────────────────────────────
// DashView — TFT_eSPI renderer for ILI9488 480×320 (landscape, rotation 1).
//
// Layout (pixels):
//   Left strip   x: 0..54       RPM bar (vertical)
//   Right strip  x: 426..479    Speed (MPH)
//   Main area    x: 55..425     370×289 — page content
//   Status bar   y: 290..319    BT status / Hz / page name
//
// Pages:
//   0  ENGINE     RPM arc (hero), load, pedal, timing
//   1  TEMPS      coolant arc, oil, CVT, IAT
//   2  BOOST      boost arc (hero), wastegate %
//   3  ROUGHNESS  4-cylinder roughness bars
//   4  G-FORCE    longitudinal G dot plot
//   5  TIMING     timing arc, knock corr, fine knock, DAM
//   6  SESSION    peak values table
// ─────────────────────────────────────────────────────────────────────────────

static constexpr int PAGE_COUNT = 7;

// Screen geometry
static constexpr int STRIP_W    = 55;
static constexpr int SCREEN_W   = 480;
static constexpr int SCREEN_H   = 320;
static constexpr int STATUS_H   = 30;
static constexpr int MAIN_X     = STRIP_W;
static constexpr int MAIN_W     = SCREEN_W - STRIP_W * 2;   // 370
static constexpr int MAIN_H     = SCREEN_H - STATUS_H;       // 290
static constexpr int MAIN_CX    = MAIN_X + MAIN_W / 2;       // 240
static constexpr int MAIN_CY    = MAIN_H / 2;                // 145

// Colours
static constexpr uint16_t COL_BG      = TFT_BLACK;
static constexpr uint16_t COL_TEXT    = TFT_WHITE;
static constexpr uint16_t COL_DIM     = 0x4208;   // dark grey
static constexpr uint16_t COL_ACCENT  = 0x07FF;   // cyan
static constexpr uint16_t COL_WARN    = TFT_ORANGE;
static constexpr uint16_t COL_DANGER  = TFT_RED;
static constexpr uint16_t COL_GOOD    = TFT_GREEN;
static constexpr uint16_t COL_RPM_BAR = 0x07E0;   // bright green → orange at high RPM

class DashView {
public:
    explicit DashView(DashData& data);

    void begin();
    void draw();               // call from render task at ~30 Hz
    void nextPage();           // advance page (called from button ISR or task)

private:
    DashData& _data;
    TFT_eSPI  _tft;
    TFT_eSprite _sprite;       // off-screen sprite for main area (PSRAM-friendly)

    int _page     = 0;
    int _lastPage = -1;
    bool _fullRedraw = true;

    // G-force history
    static constexpr int G_HIST = 120;
    float _gHistory[G_HIST] = {};
    int   _gHead = 0;
    unsigned long _lastGSample = 0;

    // ── Page dispatchers ─────────────────────────────────────────────────────
    void drawPage(int page);
    void drawEnginePage();
    void drawTempsPage();
    void drawBoostPage();
    void drawRoughnessPage();
    void drawGforcePage();
    void drawTimingPage();
    void drawSessionPage();

    // ── Common widgets ───────────────────────────────────────────────────────
    void drawSideStrips();
    void drawStatusBar();

    // Arc gauge: centred at (cx, cy), radius r, sweep 240°, value in [vmin,vmax].
    void drawArcGauge(int cx, int cy, int r,
                      float vmin, float vmax, float v,
                      const char* label, const char* unit,
                      uint16_t arcCol,
                      float warnAt = NAN, float dangerAt = NAN);

    // Horizontal bar: top-left (x,y), w×h, value [0,100].
    void drawBar(int x, int y, int w, int h, float pct, uint16_t col, const char* label);

    // Value tile: small labelled numeric box at (x,y) with given width.
    void drawTile(int x, int y, int w, int h, const char* label, float value, const char* fmt, const char* unit);

    // Roughness bar (vertical): bottom at y, height up to maxH.
    void drawCylBar(int x, int yBottom, int maxH, int w, float raw, int cylNum);

    // Format a float to a fixed-width string; returns "---" for NaN.
    void fmtVal(char* buf, int bufLen, float v, const char* fmt);

    uint16_t rpmColour(float rpm);
};
