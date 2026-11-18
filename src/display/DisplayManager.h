#pragma once
#include <TFT_eSPI.h>

namespace tv4 {
class DisplayManager {
public:
    bool begin(uint8_t brightness);
    void setBrightness(uint8_t percent);
    TFT_eSPI& panel() { return panel_; }
private:
    TFT_eSPI panel_;
};
}
