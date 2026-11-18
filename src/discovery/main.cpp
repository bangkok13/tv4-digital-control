#include "../touch/CalibrationScreen.h"
using namespace tv4;

namespace {
DisplayManager display;
TouchManager touch;
CalibrationScreen calibration(display, touch);
bool displayReady, sawPress;
uint32_t lastRead, lastLog;
void showTest() {
    auto& lcd = display.panel();
    lcd.fillScreen(TFT_BLACK);
    lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.drawString("TV-4 DIGITAL", 120, 42, 2);
    lcd.drawString(TV4_PROFILE_NAME, 120, 66, 2);
    lcd.drawString(calibration.saved() ? "Calibration saved" : "Calibration in RAM", 120, 90, 2);
    lcd.drawString("Touch anywhere / USB: c", 120, 118, 2);
}
}
void setup() {
    Serial.begin(115200);
    displayReady = display.begin(75);
    if (!displayReady) Serial.println("[error] display initialization failed");
    if (!touch.begin()) Serial.println("[error] touch SPI initialization failed");
    if (displayReady) calibration.start("DISPLAY / TOUCH TEST");
}
void loop() {
    const uint32_t now = millis();
    if (now-lastRead < 12) { yield(); return; }
    lastRead = now;
    auto c = touch.read();
    if (c.down && (!sawPress || now-lastLog >= 200)) {
        Serial.printf("[touch] raw=%d,%d z=%u pixel=%d,%d\n", c.raw.x,c.raw.y,c.pressure,c.pixel.x,c.pixel.y);
        lastLog = now;
    }
    sawPress = c.down;
    if (!displayReady) return;
    if (Serial.available() && Serial.read() == 'c') calibration.start("DISPLAY / TOUCH TEST");
    if (calibration.active()) {
        calibration.tick(now,c);
        if (!calibration.active()) showTest();
    } else if (c.down) {
        auto& lcd = display.panel();
        lcd.fillCircle(c.pixel.x, c.pixel.y, 2, TFT_CYAN);
        char line[40];
        snprintf(line, sizeof(line), "RAW %4d %4d    XY %3d %3d", c.raw.x,c.raw.y,c.pixel.x,c.pixel.y);
        lcd.drawString(line, 120, 300, 2);
    }
}
