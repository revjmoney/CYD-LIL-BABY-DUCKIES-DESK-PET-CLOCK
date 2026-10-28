// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Rev. J. Money
// Duck Pet's own loop: touch, the settings screen, serial commands, and one pond
// step per pass. Core 1 runs the pond and hands each step over as a snapshot;
// core 0 draws the newest one at full resolution, strip by strip, straight to
// the panel. Hold the screen for 2 seconds for the settings. The SD card is
// never touched.
#include <Arduino.h>
#include <Preferences.h>
#include "board.h"
#include "net.h"
#include "pet.h"
#include "render.h"
#include "led.h"
#include "duckworld.h"
#include "font8x16.h"

bool kbdType(const char* title, String& text, int maxLen, const char* hint);  // wifikbd.cpp

namespace cp {

const uint8_t* font16 = vga_font8x16;
TouchState     touch;
bool           splatterRed = true;
bool           deskClock = true;

static Preferences prefs;
static uint32_t    updateUs, steps, statMs;



// The settings screens draw through this, usually straight onto the panel. For a
// screenshot (serial P) they draw into a 40-line sprite instead, band by band,
// since this board can't read the panel back.
struct Canvas {
    lgfx::LovyanGFX* g  = &tft;
    int              oy = 0;  // the band's top line
    void fillScreen(uint16_t c) { g->fillRect(0, -oy, SW, SH, c); }
    void fillRect(int x, int y, int w, int h, uint16_t c) { g->fillRect(x, y - oy, w, h, c); }
    void fillRoundRect(int x, int y, int w, int h, int r, uint16_t c) { g->fillRoundRect(x, y - oy, w, h, r, c); }
    void drawRect(int x, int y, int w, int h, uint16_t c) { g->drawRect(x, y - oy, w, h, c); }
    void setTextSize(int s) { g->setTextSize(s); }
    void setTextColor(uint16_t f, uint16_t b) { g->setTextColor(f, b); }
    void setTextDatum(textdatum_t d) { g->setTextDatum(d); }
    void drawString(const char* s, int x, int y) { g->drawString(s, x, y - oy); }
};
static Canvas C;

static void button(int x, int y, int w, int h, const char* s, uint16_t fg = TFT_WHITE, uint16_t bg = 0x2945) {
    C.fillRoundRect(x + 1, y + 1, w - 2, h - 2, 4, bg);
    C.setTextColor(fg, bg);
    C.setTextDatum(textdatum_t::middle_center);
    C.drawString(s, x + w / 2, y + h / 2 + 1);
    C.setTextDatum(textdatum_t::top_left);
}

// a tap: where the finger came down, reported once it lifts again
static void tap(int& x, int& y) {
    int tx, ty;
    while (boardTouch(tx, ty)) delay(10);
    while (!boardTouch(x, y)) {
        if (Serial.available() && Serial.peek() == 'T') {  // a serial tap, for testing
            Serial.read();
            x = Serial.parseInt();
            y = Serial.parseInt();
            return;
        }
        if (Serial.available()) Serial.read();
        delay(10);
    }
    delay(25);  // let the resistive reading settle, then take a second look
    if (boardTouch(tx, ty)) x = tx, y = ty;
    while (boardTouch(tx, ty)) delay(10);
    delay(30);
}

// ---------------------------------------------------------------- settings
constexpr int TOP = 30, ROW = 28;  // seven rows of settings under the title bar
constexpr int ABOUT_X = SW - 140, ABOUT_W = 72;  // in the title bar, left of DONE

// who made it and where to find more
static void drawAbout() {
    C.fillScreen(TFT_BLACK);
    C.fillRect(0, 0, SW, 26, 0x0329);
    C.setTextSize(1);
    C.setTextColor(TFT_WHITE, 0x0329);
    C.drawString("ABOUT DUCK PET", 6, 9);
    C.setTextSize(2);
    C.setTextColor(0xFEA0, TFT_BLACK);  // duckling yellow
    C.drawString("DUCK PET 1.0", 8, 36);
    C.setTextSize(1);
    C.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    C.drawString("Glossy toy ducklings on a pond.", 8, 58);
    C.drawString("Nature picks yours. Raise it into a duck.", 8, 70);
    C.setTextColor(TFT_WHITE, TFT_BLACK);
    C.drawString("Made by Rev. J. Money", 8, 90);
    C.setTextSize(2);
    C.setTextColor(0xFBE3, TFT_BLACK);  // the store's orange
    C.drawString("jmscnc.com", 8, 108);
    C.setTextSize(1);
    C.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    C.drawString("Ready-to-run desk pets, gadgets and more", 8, 128);
    C.setTextSize(2);
    C.setTextColor(0x5D7F, TFT_BLACK);
    C.drawString("rotatocam.com", 8, 146);
    C.setTextSize(1);
    C.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    C.drawString("RotatoCAM: 4- and 3-axis CAM for DIY CNC", 8, 166);
    C.setTextColor(TFT_DARKGREY, TFT_BLACK);
    C.drawString("Code on GitHub: github.com/revjmoney", 8, 186);
    C.drawString("Code GPL-3.0. VGA font by VileR, CC BY-SA 4.0.", 8, 198);
    C.setTextColor(TFT_WHITE, TFT_BLACK);
    C.drawString("Tap anywhere to go back.", 8, 222);
}
static void about() {  // any tap goes back
    drawAbout();
    int x, y;
    tap(x, y);
}

static void drawSettings() {
    char s[48];
    C.fillScreen(TFT_BLACK);
    C.setTextSize(1);
    C.fillRect(0, 0, SW, 26, 0x0329);
    C.setTextColor(TFT_WHITE, 0x0329);
    C.drawString("DUCK PET SETTINGS", 6, 9);
    button(SW - 64, 0, 64, 26, "DONE", TFT_WHITE, 0x0460);
    button(ABOUT_X, 0, ABOUT_W, 26, "ABOUT", TFT_WHITE, 0x4208);
    snprintf(s, sizeof s, "DESK CLOCK: %s", deskClock ? "ON (big time, date, weather)" : "OFF");
    button(0, TOP, SW, ROW, s);
    snprintf(s, sizeof s, "<   TIME ZONE: %s   >", netTzName(netTz()));
    button(0, TOP + ROW, SW, ROW, s);
    bool paused = duckData().paused;
    button(0, TOP + ROW * 2, SW, ROW, paused ? "EVOLVING: PAUSED (tap to resume)" : "EVOLVING: ON (tap to pause)", paused ? TFT_YELLOW : TFT_WHITE);
    String zip = netZip();
    if (C.g != &tft) zip = "";  // a screenshot never shows where the owner lives
    snprintf(s, sizeof s, "WEATHER ZIP: %s", zip.length() ? zip.c_str() : "OFF (tap to set)");
    button(0, TOP + ROW * 3, SW, ROW, s);
    button(0, TOP + ROW * 4, SW / 2, ROW, "WIFI: TYPE PASSWORD");
    button(SW / 2, TOP + ROW * 4, SW / 2, ROW, "WIFI: ROUTER WPS");
    button(0, TOP + ROW * 5, SW / 2, ROW, "SET THE CLOCK NOW");
    button(SW / 2, TOP + ROW * 5, SW / 2, ROW, "GENESIS (NEW CLUTCH)", TFT_WHITE, 0x6000);
    button(0, TOP + ROW * 6, SW, ROW, splatterRed ? "SPLATTER: RED (tap for feathers only)" : "SPLATTER: FEATHERS ONLY (tap for red)");
    C.setTextColor(TFT_DARKGREY, TFT_BLACK);
    C.drawString("Tap water: peas. Evil duck: bop. Your duck: cheer.", 4, TOP + ROW * 7 + 2);
}

// GENESIS asks first: the pet is gone for good after this
static bool confirmGenesis() {
    C.fillScreen(TFT_BLACK);
    C.setTextSize(2);
    C.setTextColor(TFT_ORANGE, TFT_BLACK);
    C.drawString("START A NEW CLUTCH?", 8, 70);
    C.setTextSize(1);
    C.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    C.drawString("Your duck will be gone for good.", 8, 100);
    C.drawString("The pond starts over with 8 new ducklings.", 8, 114);
    button(0, 180, SW / 2, 60, "KEEP MY DUCK", TFT_WHITE, 0x0460);
    button(SW / 2, 180, SW / 2, 60, "START OVER", TFT_WHITE, 0x8000);
    int x, y;
    do tap(x, y);
    while (y < 180);
    return x >= SW / 2;
}

// the drawing is paused meanwhile, so the settings can use the panel directly
static void settings() {
    renderPause(true);
    for (drawSettings();; drawSettings()) {
        int x, y;
        tap(x, y);
        int row = (y - TOP) / ROW;
        if (y < 26) {
            if (x >= SW - 64) break;
            if (x >= ABOUT_X && x < ABOUT_X + ABOUT_W) about();
        } else if (row == 0) {
            deskClock = !deskClock;
            prefs.putBool("desk", deskClock);
        } else if (row == 1) {
            netSetTz(netTz() + (x < SW / 2 ? -1 : 1));
        } else if (row == 2) {
            duckPause(!duckData().paused);
        } else if (row == 3) {
            String zip = netZip();
            if (kbdType("WEATHER ZIP CODE", zip, 5, "US ZIP, blank turns weather off")) {
                netSetZip(zip);
                duckSave();
                netRequestSync();  // reboots and fetches it
            }
        } else if (row == 4) {
            duckSave();
            if (x < SW / 2) netRequestKeyboard();
            else netRequestWps();
        } else if (row == 5) {
            if (x < SW / 2) {
                duckSave();
                netRequestSync();
            } else if (confirmGenesis()) {
                duckGenesis();
                break;
            }
        } else if (row == 6) {
            splatterRed = !splatterRed;
            prefs.putUChar("splat", splatterRed ? 0 : 1);
        }
    }
    touch = {};
    renderPause(false);
}

// ---------------------------------------------------------------- serial
// i info, S screenshot (SHOT, u16 w, u16 h, then RGB888 rows), T x y a tap,
// M the settings (as if held 2 s).
// Debug only, never on screen: + six hours of evolving, n next stage, t traits,
// r a new clutch, w a fast round one, h hold round one (no crowning),
// F sun hunt plump forces the traits shown (not saved), q the next silly
// achievement now.
static int serialTapX = -1, serialTapY = -1;

static void serialCommands() {
    while (Serial.available()) {
        switch (char c = Serial.read()) {
            case 'i': {
                uint32_t f, us, span = max(1UL, millis() - statMs);
                renderStats(f, us);
                Serial.printf("%.1f fps drawn, %.1f steps/s, step %lu us, draw %lu us, heap %u, max block %u\n",
                              f * 1000.0f / span, steps * 1000.0f / span, (unsigned long)(updateUs / max(1UL, (unsigned long)steps)),
                              (unsigned long)(us / max(1UL, (unsigned long)f)), ESP.getFreeHeap(),
                              heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
                statMs = millis(), updateUs = steps = 0;
                break;
            }
            case 'S': renderShot(); break;
            case 'P': {  // P0 the settings, P1 the about screen, sent like S
                int        which = Serial.parseInt();
                LGFX_Sprite band(&tft);
                band.setColorDepth(16);
                if (!band.createSprite(SW, 40)) break;
                uint8_t* line = (uint8_t*)malloc(SW * 3);  // only while it's sending
                if (!line) break;
                Serial.write("SHOT", 4);
                uint16_t wh[2] = {SW, SH};
                Serial.write((uint8_t*)wh, 4);
                C.g = &band;
                for (C.oy = 0; C.oy < SH; C.oy += 40) {
                    band.fillScreen(TFT_BLACK);
                    if (which == 1) drawAbout();
                    else drawSettings();
                    for (int y = 0; y < 40; y++) {
                        for (int x = 0; x < SW; x++) {
                            uint16_t c = band.readPixel(x, y);
                            line[x * 3] = (c >> 8) & 0xF8, line[x * 3 + 1] = (c >> 3) & 0xFC, line[x * 3 + 2] = (c << 3) & 0xF8;
                        }
                        Serial.write(line, SW * 3);
                    }
                }
                free(line);
                C.g = &tft, C.oy = 0;
                band.deleteSprite();
                break;
            }
            case 'M': settings(); break;
            case 'T':
                serialTapX = Serial.parseInt();
                serialTapY = Serial.parseInt();
                break;
            case '+':
            case 'n':
            case 't':
            case 'r':
            case 'w':
            case 'h':
            case 'q':
            case 'Z':
            case 'K':
            case '.':
            case 'E':
            case 'N':
            case 'V':
            case 'X': duckDebug(c); break;
            case 'F': {
                float sun = Serial.parseFloat(), hunt = Serial.parseFloat(), plump = Serial.parseFloat();
                duckForce(sun, hunt, plump);
                break;
            }
        }
    }
}

// ---------------------------------------------------------------- touch
static void readTouch() {
    static bool     was;
    static uint32_t downMs;
    static int      sx, sy;
    int  x, y;
    bool down = boardTouch(x, y);
    uint32_t now = millis();
    if (serialTapX >= 0) {  // a serial tap: one frame of finger
        x = serialTapX, y = serialTapY, down = true, was = false;
        serialTapX = -1;
    }
    if (down) {
        if (!was) downMs = now, sx = x, sy = y, touch.pressed = true;
        touch.x = x, touch.y = y, touch.lastMs = now;
        // a still finger held for 2 seconds opens the settings
        if (abs(x - sx) < 20 && abs(y - sy) < 20) {
            if (now - downMs > 2000) {
                settings();
                was = false;
                return;
            }
        } else
            downMs = now, sx = x, sy = y;
    }
    touch.down = was = down;
}


// one pond step per pass, at most 40 a second; core 0 draws whatever is newest
static void petLoop() {
    static uint32_t t0 = millis(), last;
    serialCommands();
    ledUpdate();
    readTouch();
    uint32_t u = micros();
    if (touch.pressed) touch.pressed = false, duckTap(touch.x, touch.y);
    duckStep(millis() - t0);
    updateUs += micros() - u;
    steps++;
    if (!statMs) statMs = millis();
    uint32_t now = millis();
    if (now - last < 25) delay(25 - (now - last));
    last = millis();
}

// takes over the board for good: the panel and Serial are already up
void petMain() {
    ledInit();
    prefs.begin("cellpet");  // named after the firmware Duck Pet grew out of, so a reflashed board keeps its settings
    deskClock = prefs.getBool("desk", true);
    splatterRed = prefs.getUChar("splat", 0) == 0;
    tft.setRotation(1);  // the pond is landscape
    renderInit();  // the display strips before WiFi, so the radio can't fragment the space they need
    netSyncAtBoot();
    Serial.printf("\nDuck Pet, heap left %u (max block %u)\n", ESP.getFreeHeap(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    duckBegin();
    renderTaskStart();
    for (;;) petLoop();
}

}  // namespace cp
