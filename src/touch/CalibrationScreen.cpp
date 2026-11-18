#include "CalibrationScreen.h"
#include <cstdlib>

namespace tv4 {
void CalibrationScreen::start(const char* mode) {
    mode_ = mode;
    step_ = 0;
    active_ = waitingRelease_ = true;
    tracking_ = saved_ = false;
    draw("Hold each cross, then lift");
}
void CalibrationScreen::draw(const char* hint) {
    auto& lcd = display_.panel();
    lcd.fillScreen(TFT_BLACK);
    lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.drawString("TV-4 DIGITAL", 120, 116, 2);
    lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
    lcd.drawString(mode_, 120, 140, 2);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.drawString(hint, 120, 165, 2);
    lcd.drawString(TV4_PROFILE_NAME, 120, 195, 2);
    for (int i = 0; i < 3; ++i) {
        const uint16_t colours[] = {TFT_RED, TFT_GREEN, TFT_BLUE};
        lcd.fillRect(60 + 40*i, 215, 40, 10, colours[i]);
    }
    auto p = targets[step_];
    lcd.drawCircle(p.x, p.y, 10, TFT_CYAN);
    lcd.drawFastVLine(p.x, p.y-14, 29, TFT_WHITE);
    lcd.drawFastHLine(p.x-14, p.y, 29, TFT_WHITE);
}
void CalibrationScreen::tick(uint32_t now, const Contact& c) {
    if (!active_) return;
    if (!c.down) {
        tracking_ = waitingRelease_ = false;
        return;
    }
    if (waitingRelease_) return;
    if (!tracking_ || std::abs(c.raw.x-anchor_.x)>90 || std::abs(c.raw.y-anchor_.y)>90) {
        anchor_ = c.raw;
        since_ = now;
        tracking_ = true;
        return;
    }
    if (now-since_ < 60) return;
    waitingRelease_ = true;
    if (step_ < 3) {
        candidate_.raw[step_++] = c.raw;
        if (step_ == 3 && !candidate_.valid()) {
            step_ = 0;
            draw("Retry: touch cross centres");
        } else draw(step_ == 3 ? "Check final cross" : "Hold cross, then lift");
        return;
    }
    const auto measured = candidate_.project(c.raw);
    if (std::abs(measured.x-targets[3].x)>14 || std::abs(measured.y-targets[3].y)>14) {
        step_ = 0;
        draw("Check failed: try again");
        return;
    }
    saved_ = touch_.apply(candidate_);
    Serial.println(saved_ ? "[touch] calibration saved" : "[touch] calibration in RAM; NVS error");
    active_ = false;
}
}
