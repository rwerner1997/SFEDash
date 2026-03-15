#include <Arduino.h>
#include "DashData.h"
#include "OBDManager.h"
#include "DashView.h"

// ─────────────────────────────────────────────────────────────────────────────
// SFEDash ESP32 — 2015+ Subaru WRX FA20DIT / CVT
//
// Core 0: render task (DashView, ~30 Hz)
// Core 1: OBD poll task (OBDManager, ~20 Hz burst)
//
// Page button: PAGE_BTN_GPIO (default GPIO 0 = BOOT button on DevKit)
//   Short press → next page
//   Long press  → reset session peaks (page 6)
// ─────────────────────────────────────────────────────────────────────────────

DashData    g_data;       // Global shared state (defined in DashData.h as extern)
OBDManager  g_obd(g_data);
DashView    g_view(g_data);

// ── Button handling ───────────────────────────────────────────────────────────

static volatile unsigned long btnPressMs = 0;
static volatile bool          btnPending = false;

void IRAM_ATTR btnISR() {
    if (digitalRead(PAGE_BTN_GPIO) == LOW) {
        btnPressMs = millis();
    } else {
        btnPending = true;   // release — process in main task
    }
}

static void handleButton() {
    if (!btnPending) return;
    btnPending = false;

    unsigned long held = millis() - btnPressMs;
    if (held > 800) {
        // Long press: reset peaks
        g_data.resetPeaks();
    } else {
        // Short press: next page
        g_view.nextPage();
    }
}

// ── Render task (core 0) ──────────────────────────────────────────────────────

static void renderTask(void*) {
    g_view.begin();

    for (;;) {
        g_view.draw();
        vTaskDelay(pdMS_TO_TICKS(33));   // ~30 fps
    }
}

// ── Arduino setup / loop ──────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    Serial.println("[SFEDash] Starting...");

    // Backlight on
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);

    // Page cycle button (active LOW, internal pull-up)
    pinMode(PAGE_BTN_GPIO, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PAGE_BTN_GPIO), btnISR, CHANGE);

    // Render task on core 0 (priority 1, 6KB stack)
    xTaskCreatePinnedToCore(renderTask, "Render", 6144, nullptr, 1, nullptr, 0);

    // OBD task launches internally on core 1
    g_obd.start();
}

void loop() {
    handleButton();
    delay(20);
}
