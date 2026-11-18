#include "DisplayManager.h"
#include "tv4_board.h"
#include <algorithm>

namespace tv4 {
bool DisplayManager::begin(uint8_t brightness) {
    if (!ledcSetup(board::pwmChannel, 5000, 8)) return false;
    ledcAttachPin(board::backlight, board::pwmChannel);
    setBrightness(10);
    auto& bus=panel_.getSPIinstance();
    bus.begin(TFT_SCLK,TFT_MISO,TFT_MOSI,-1);
    if (!bus.bus()) return false;
    panel_.init();
    panel_.setRotation(0);
    if (panel_.width() != board::width || panel_.height() != board::height) return false;
    panel_.fillScreen(TFT_BLACK);
    setBrightness(brightness);
    Serial.printf("[LCD] %s 240x320; verify pixels/colours visually\n", TV4_PROFILE_NAME);
    return true;
}
void DisplayManager::setBrightness(uint8_t percent) {
    ledcWrite(board::pwmChannel, std::clamp<int>(percent, 10, 100) * 255 / 100);
}
}
