#include "DashView.h"
#include <math.h>

static const char* PAGE_NAMES[PAGE_COUNT] = {
    "ENGINE", "TEMPS", "BOOST", "ROUGHNESS", "G-FORCE", "TIMING", "SESSION"
};

// ─────────────────────────────────────────────────────────────────────────────

DashView::DashView(DashData& data)
    : _data(data), _tft(), _sprite(&_tft) {}

void DashView::begin() {
    _tft.init();
    _tft.setRotation(1);          // landscape: 480 wide × 320 tall
    _tft.fillScreen(COL_BG);
    _tft.setTextDatum(MC_DATUM);

    // Create a sprite for the main content area to minimise flicker.
    // ~370×290×2 = 214 KB. Uses PSRAM if present; falls back to internal RAM.
    _sprite.setColorDepth(16);
    bool ok = _sprite.createSprite(MAIN_W, MAIN_H);
    if (!ok) {
        // No PSRAM — fall back to direct drawing (more flicker, still functional).
        Serial.println("[DashView] Sprite allocation failed — direct draw mode");
    }
    _sprite.setTextDatum(MC_DATUM);

    _fullRedraw = true;
}

void DashView::nextPage() {
    _page = (_page + 1) % PAGE_COUNT;
    _data.activePage = _page;
    _fullRedraw = true;
}

void DashView::draw() {
    if (_lastPage != _page || _fullRedraw) {
        _tft.fillScreen(COL_BG);
        _fullRedraw = false;
        _lastPage   = _page;
    }

    drawPage(_page);
    drawSideStrips();
    drawStatusBar();
}

// ── Page dispatch ─────────────────────────────────────────────────────────────

void DashView::drawPage(int page) {
    TFT_eSprite* canvas = _sprite.created() ? &_sprite : nullptr;

    if (canvas) {
        canvas->fillSprite(COL_BG);
    }

    switch (page) {
        case 0: drawEnginePage();    break;
        case 1: drawTempsPage();     break;
        case 2: drawBoostPage();     break;
        case 3: drawRoughnessPage(); break;
        case 4: drawGforcePage();    break;
        case 5: drawTimingPage();    break;
        case 6: drawSessionPage();   break;
    }

    if (canvas) {
        canvas->pushSprite(MAIN_X, 0);
    }
}

// ── Page 0: ENGINE ────────────────────────────────────────────────────────────

void DashView::drawEnginePage() {
    TFT_eSprite* c = _sprite.created() ? &_sprite : nullptr;
    TFT_eSPI*    t = c ? nullptr : &_tft;
    // Helper: offset x/y when drawing into the sprite vs. direct TFT
    auto ox = [&](int x) { return c ? x - MAIN_X : x; };
    auto oy = [&](int y) { return y; };  // y origin is same for both

    // Hero arc: RPM (0–8000)
    drawArcGauge(c ? MAIN_W/2 : MAIN_CX, 120, 95,
                 0, 8000, _data.rpm,
                 "RPM", "",
                 rpmColour(_data.rpm),
                 6000, 7000);

    // Secondary tiles: load %, pedal %, timing
    drawTile(ox(MAIN_X) + 5,  200, 110, 60, "LOAD",   _data.loadPct,  "%.0f", "%");
    drawTile(ox(MAIN_X) + 125, 200, 110, 60, "PEDAL",  _data.pedalPct, "%.0f", "%");
    drawTile(ox(MAIN_X) + 245, 200, 110, 60, "TIMING", _data.timingDeg,"%.1f", "\xB0");
}

// ── Page 1: TEMPS ─────────────────────────────────────────────────────────────

void DashView::drawTempsPage() {
    auto ox = [](int x) { return x - MAIN_X; };

    drawArcGauge(MAIN_W/2, 120, 95,
                 60, 260, _data.coolantF(),
                 "COOLANT", "\xB0""F",
                 COL_ACCENT, 220, 240);

    drawTile(ox(MAIN_X) + 5,   200, 110, 60, "OIL",    _data.oilTempF(), "%.0f", "\xB0""F");
    drawTile(ox(MAIN_X) + 125, 200, 110, 60, "CVT",    _data.cvtTempF(), "%.0f", "\xB0""F");
    drawTile(ox(MAIN_X) + 245, 200, 110, 60, "IAT",    _data.iatF(),     "%.0f", "\xB0""F");
}

// ── Page 2: BOOST ─────────────────────────────────────────────────────────────

void DashView::drawBoostPage() {
    auto ox = [](int x) { return x - MAIN_X; };

    drawArcGauge(MAIN_W/2, 130, 105,
                 -5, 22, _data.boostPsi(),
                 "BOOST", "PSI",
                 COL_ACCENT, 18, 21);

    drawTile(ox(MAIN_X) + 80, 255, 210, 28, "WASTEGATE", _data.wastegatePct, "%.0f", "%");
}

// ── Page 3: ROUGHNESS ────────────────────────────────────────────────────────

void DashView::drawRoughnessPage() {
    // 4 vertical bars for cylinders 1-4 (FA20DIT boxer layout: 1,3 left; 2,4 right)
    // Display order left→right: cyl1, cyl2, cyl3, cyl4
    static constexpr int BAR_MAX_H = 220;
    static constexpr int BAR_W     = 60;
    static constexpr int BAR_Y     = 250;

    // Spacing: 4 bars across 370px → gap of (370 - 4*60) / 5 = 26px
    int xs[4] = { 26, 112, 198, 284 };

    const float vals[4] = { _data.rough1, _data.rough2, _data.rough3, _data.rough4 };
    for (int i = 0; i < 4; i++) {
        drawCylBar(xs[i], BAR_Y, BAR_MAX_H, BAR_W, vals[i], i + 1);
    }

    // Title
    TFT_eSprite* c = _sprite.created() ? &_sprite : nullptr;
    if (c) {
        c->setTextColor(COL_DIM, COL_BG);
        c->setTextFont(2);
        c->setTextDatum(TC_DATUM);
        c->drawString("ROUGHNESS", MAIN_W/2, 4);
    }
}

// ── Page 4: G-FORCE ──────────────────────────────────────────────────────────

void DashView::drawGforcePage() {
    // Sample longitudinal G from speed delta
    unsigned long now = millis();
    if (now - _lastGSample >= 100) {
        // Approximate G from RPM change direction (rough proxy without accelerometer)
        // If an accelerometer is wired later, replace with actual reading.
        float g = 0.0f;
        if (!isnan(_data.loadPct) && !isnan(_data.speedKph)) {
            // Rough: full throttle at 100 km/h ≈ 0.4G
            g = (_data.loadPct / 100.0f) * (_data.rpm > 2000 ? 0.4f : 0.0f);
        }
        _gHistory[_gHead] = g;
        _gHead = (_gHead + 1) % G_HIST;
        _lastGSample = now;
    }

    TFT_eSprite* c = _sprite.created() ? &_sprite : nullptr;
    if (!c) return;

    // Grid
    c->drawFastHLine(0, MAIN_H/2, MAIN_W, COL_DIM);
    c->drawFastVLine(MAIN_W - 20, 0, MAIN_H, COL_DIM);

    // Scale: ±1G maps to ±MAIN_H/2
    int plotW = MAIN_W - 20;
    float xScale = (float)plotW / G_HIST;
    for (int i = 0; i < G_HIST; i++) {
        int idx = (_gHead + i) % G_HIST;
        float g = _gHistory[idx];
        int x = (int)(i * xScale);
        int y = MAIN_H/2 - (int)(g * (MAIN_H/2));
        y = constrain(y, 0, MAIN_H - 1);
        c->fillCircle(x, y, 2, COL_ACCENT);
    }

    c->setTextColor(COL_DIM, COL_BG);
    c->setTextFont(2);
    c->drawString("G-FORCE (LONGITUDINAL)", MAIN_W/2, 4);
    c->drawString("+1G", MAIN_W - 10, 8);
    c->drawString(" 0G", MAIN_W - 10, MAIN_H/2 - 6);
    c->drawString("-1G", MAIN_W - 10, MAIN_H - 16);
}

// ── Page 5: TIMING ───────────────────────────────────────────────────────────

void DashView::drawTimingPage() {
    auto ox = [](int x) { return x - MAIN_X; };

    drawArcGauge(MAIN_W/2, 120, 95,
                 -20, 50, _data.timingDeg,
                 "TIMING", "\xB0",
                 COL_ACCENT);

    drawTile(ox(MAIN_X) + 5,   200, 110, 60, "KNOCK",    _data.knockCorr,    "%+.1f", "\xB0");
    drawTile(ox(MAIN_X) + 125, 200, 110, 60, "F.KNOCK",  _data.fineKnockDeg, "%+.1f", "\xB0");
    drawTile(ox(MAIN_X) + 245, 200, 110, 60, "DAM",      _data.damRatio,     "%.3f", "");
}

// ── Page 6: SESSION ───────────────────────────────────────────────────────────

void DashView::drawSessionPage() {
    TFT_eSprite* c = _sprite.created() ? &_sprite : nullptr;
    if (!c) return;

    c->setTextFont(2);
    c->setTextDatum(TL_DATUM);

    struct Row { const char* label; float value; const char* fmt; const char* unit; };
    Row rows[] = {
        { "PEAK BOOST",   _data.peakBoostPsi,   "%.1f", " psi"   },
        { "PEAK RPM",     _data.peakRpm,         "%.0f", " rpm"   },
        { "PEAK TIMING",  _data.peakTimingDeg,   "%.1f", "\xB0"   },
        { "PEAK LOAD",    _data.peakLoadPct,      "%.0f", "%"      },
        { "PEAK SPEED",   _data.peakSpeedMph,     "%.0f", " mph"   },
        { "WORST KNOCK",  _data.worstKnockCorr,   "%+.1f","\xB0"  },
        { "KNOCK EVENTS", (float)_data.knockEventCount, "%.0f","" },
        { "PEAK MAF",     _data.peakMafGs,        "%.1f", " g/s"  },
    };

    int y = 10;
    char buf[32];
    for (auto& row : rows) {
        c->setTextColor(COL_DIM, COL_BG);
        c->drawString(row.label, 8, y);

        fmtVal(buf, sizeof(buf), row.value, row.fmt);
        char full[40];
        if (!isnan(row.value)) snprintf(full, sizeof(full), "%s%s", buf, row.unit);
        else snprintf(full, sizeof(full), "---");

        c->setTextColor(COL_TEXT, COL_BG);
        c->setTextDatum(TR_DATUM);
        c->drawString(full, MAIN_W - 8, y);
        c->setTextDatum(TL_DATUM);
        y += 30;
    }

    c->setTextColor(COL_DIM, COL_BG);
    c->setTextDatum(BC_DATUM);
    c->drawString("TAP HOLD TO RESET", MAIN_W/2, MAIN_H - 4);
}

// ── Side strips ───────────────────────────────────────────────────────────────

void DashView::drawSideStrips() {
    // Left strip: vertical RPM bar
    static constexpr int BAR_PAD = 4;
    static constexpr int BAR_TOP = 8;
    int barH = MAIN_H - BAR_TOP - BAR_PAD;
    int barX = BAR_PAD;
    int barW = STRIP_W - BAR_PAD * 2;

    _tft.fillRect(0, 0, STRIP_W, MAIN_H, COL_BG);

    float rpmPct = 0.0f;
    if (!isnan(_data.rpm)) rpmPct = constrain(_data.rpm / 8000.0f, 0.0f, 1.0f);
    int fillH = (int)(rpmPct * barH);

    // Background track
    _tft.fillRect(barX, BAR_TOP, barW, barH, COL_DIM);
    // Fill
    if (fillH > 0) {
        _tft.fillRect(barX, BAR_TOP + barH - fillH, barW, fillH, rpmColour(_data.rpm));
    }
    // RPM label
    char buf[8]; fmtVal(buf, sizeof(buf), _data.rpm, "%.0f");
    _tft.setTextColor(COL_TEXT, COL_BG);
    _tft.setTextFont(1);
    _tft.setTextDatum(BC_DATUM);
    _tft.drawString(buf, STRIP_W/2, MAIN_H - 2);

    // Right strip: speed in MPH
    _tft.fillRect(SCREEN_W - STRIP_W, 0, STRIP_W, MAIN_H, COL_BG);
    char spd[8]; fmtVal(spd, sizeof(spd), _data.speedMph(), "%.0f");
    _tft.setTextColor(COL_TEXT, COL_BG);
    _tft.setTextFont(4);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString(spd, SCREEN_W - STRIP_W/2, MAIN_H/2 - 14);
    _tft.setTextFont(1);
    _tft.setTextColor(COL_DIM, COL_BG);
    _tft.drawString("MPH", SCREEN_W - STRIP_W/2, MAIN_H/2 + 14);
}

// ── Status bar ────────────────────────────────────────────────────────────────

void DashView::drawStatusBar() {
    int y = MAIN_H;
    _tft.fillRect(0, y, SCREEN_W, STATUS_H, 0x1082);  // very dark blue-grey

    _tft.setTextFont(1);
    _tft.setTextDatum(ML_DATUM);
    _tft.setTextColor(_data.connected ? COL_GOOD : COL_WARN, 0x1082);
    _tft.drawString(_data.btStatus, 6, y + STATUS_H/2);

    // Protocol
    _tft.setTextColor(COL_DIM, 0x1082);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString(_data.obdProtocol, SCREEN_W/2, y + STATUS_H/2);

    // Hz + page
    char hz[24];
    snprintf(hz, sizeof(hz), "%d Hz  %s", _data.pollHz, PAGE_NAMES[_page]);
    _tft.setTextColor(COL_DIM, 0x1082);
    _tft.setTextDatum(MR_DATUM);
    _tft.drawString(hz, SCREEN_W - 4, y + STATUS_H/2);

    // Page dots
    int dotY = y + STATUS_H - 6;
    for (int i = 0; i < PAGE_COUNT; i++) {
        int dotX = SCREEN_W/2 - (PAGE_COUNT - 1) * 7 + i * 14 - 36;
        uint16_t col = (i == _page) ? COL_ACCENT : COL_DIM;
        _tft.fillCircle(dotX, dotY, 3, col);
    }
}

// ── Arc gauge ─────────────────────────────────────────────────────────────────

void DashView::drawArcGauge(int cx, int cy, int r,
                             float vmin, float vmax, float v,
                             const char* label, const char* unit,
                             uint16_t arcCol,
                             float warnAt, float dangerAt) {
    TFT_eSprite* c = _sprite.created() ? &_sprite : nullptr;

    // Arc spans 240° centred at the bottom: from 150° to 390° (clockwise from 3 o'clock=0°)
    static constexpr float START_DEG = 150.0f;
    static constexpr float SWEEP_DEG = 240.0f;

    // Track arc (background)
    if (c) {
        c->drawArc(cx, cy, r, r - 12, (int)START_DEG, (int)(START_DEG + SWEEP_DEG), COL_DIM, COL_BG);
    } else {
        _tft.drawArc(cx, cy, r, r - 12, (int)START_DEG, (int)(START_DEG + SWEEP_DEG), COL_DIM, COL_BG);
    }

    // Value arc
    if (!isnan(v) && vmax > vmin) {
        float pct = constrain((v - vmin) / (vmax - vmin), 0.0f, 1.0f);
        float endDeg = START_DEG + pct * SWEEP_DEG;

        // Colour escalation
        uint16_t col = arcCol;
        if (!isnan(dangerAt) && v >= dangerAt) col = COL_DANGER;
        else if (!isnan(warnAt) && v >= warnAt) col = COL_WARN;

        if (c) {
            c->drawArc(cx, cy, r, r - 12, (int)START_DEG, (int)endDeg, col, COL_BG);
        } else {
            _tft.drawArc(cx, cy, r, r - 12, (int)START_DEG, (int)endDeg, col, COL_BG);
        }
    }

    // Value text (centre)
    char buf[16]; fmtVal(buf, sizeof(buf), v, isnan(v) ? nullptr : "%.1f");
    char valStr[20];
    snprintf(valStr, sizeof(valStr), "%s%s", buf, unit);

    if (c) {
        c->setTextColor(COL_TEXT, COL_BG);
        c->setTextFont(6);
        c->setTextDatum(MC_DATUM);
        c->drawString(valStr, cx, cy - 10);
        c->setTextFont(2);
        c->setTextColor(COL_DIM, COL_BG);
        c->drawString(label, cx, cy + 22);
    } else {
        _tft.setTextColor(COL_TEXT, COL_BG);
        _tft.setTextFont(6);
        _tft.setTextDatum(MC_DATUM);
        _tft.drawString(valStr, cx, cy - 10);
        _tft.setTextFont(2);
        _tft.setTextColor(COL_DIM, COL_BG);
        _tft.drawString(label, cx, cy + 22);
    }
}

// ── Bar widget ────────────────────────────────────────────────────────────────

void DashView::drawBar(int x, int y, int w, int h, float pct, uint16_t col, const char* label) {
    TFT_eSprite* c = _sprite.created() ? &_sprite : nullptr;
    int fill = isnan(pct) ? 0 : (int)(constrain(pct, 0, 100) / 100.0f * w);

    if (c) {
        c->fillRect(x, y, w, h, COL_DIM);
        if (fill > 0) c->fillRect(x, y, fill, h, col);
        c->setTextColor(COL_DIM, COL_BG);
        c->setTextFont(1);
        c->setTextDatum(TL_DATUM);
        c->drawString(label, x, y - 12);
    } else {
        _tft.fillRect(x, y, w, h, COL_DIM);
        if (fill > 0) _tft.fillRect(x, y, fill, h, col);
    }
}

// ── Tile widget ───────────────────────────────────────────────────────────────

void DashView::drawTile(int x, int y, int w, int h, const char* label, float value, const char* fmt, const char* unit) {
    TFT_eSprite* c = _sprite.created() ? &_sprite : nullptr;

    char buf[16]; fmtVal(buf, sizeof(buf), value, fmt);
    char valStr[24]; snprintf(valStr, sizeof(valStr), "%s%s", buf, isnan(value) ? "" : unit);

    if (c) {
        c->fillRoundRect(x, y, w, h, 4, 0x2104);  // very dark fill
        c->setTextColor(COL_DIM, 0x2104);
        c->setTextFont(1);
        c->setTextDatum(TC_DATUM);
        c->drawString(label, x + w/2, y + 4);
        c->setTextColor(COL_TEXT, 0x2104);
        c->setTextFont(4);
        c->setTextDatum(MC_DATUM);
        c->drawString(valStr, x + w/2, y + h/2 + 6);
    } else {
        _tft.fillRoundRect(x + MAIN_X, y, w, h, 4, 0x2104);
        _tft.setTextColor(COL_DIM, 0x2104);
        _tft.setTextFont(1);
        _tft.setTextDatum(TC_DATUM);
        _tft.drawString(label, x + MAIN_X + w/2, y + 4);
        _tft.setTextColor(COL_TEXT, 0x2104);
        _tft.setTextFont(4);
        _tft.setTextDatum(MC_DATUM);
        _tft.drawString(valStr, x + MAIN_X + w/2, y + h/2 + 6);
    }
}

// ── Cylinder bar (roughness) ──────────────────────────────────────────────────

void DashView::drawCylBar(int x, int yBottom, int maxH, int w, float raw, int cylNum) {
    TFT_eSprite* c = _sprite.created() ? &_sprite : nullptr;

    int fillH = isnan(raw) ? 0 : (int)(constrain(raw / 100.0f, 0.0f, 1.0f) * maxH);
    uint16_t col = (raw > 50) ? COL_WARN : (raw > 20 ? COL_ACCENT : COL_GOOD);

    if (c) {
        c->fillRect(x, yBottom - maxH, w, maxH, COL_DIM);
        if (fillH > 0) c->fillRect(x, yBottom - fillH, w, fillH, col);
        char buf[4]; snprintf(buf, sizeof(buf), "%d", cylNum);
        c->setTextColor(COL_DIM, COL_BG);
        c->setTextFont(2);
        c->setTextDatum(BC_DATUM);
        c->drawString(buf, x + w/2, yBottom + 14);
        char val[8]; fmtVal(val, sizeof(val), raw, "%.0f");
        c->setTextColor(COL_TEXT, COL_BG);
        c->setTextFont(1);
        c->drawString(val, x + w/2, yBottom - maxH - 4);
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void DashView::fmtVal(char* buf, int bufLen, float v, const char* fmt) {
    if (isnan(v) || fmt == nullptr) {
        strncpy(buf, "---", bufLen);
        return;
    }
    snprintf(buf, bufLen, fmt, v);
}

uint16_t DashView::rpmColour(float rpm) {
    if (isnan(rpm) || rpm < 5000) return COL_RPM_BAR;
    if (rpm < 6500)               return COL_WARN;
    return COL_DANGER;
}
