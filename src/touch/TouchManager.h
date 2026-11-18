#pragma once
#include <Calibration.h>
#include <SPI.h>
#include <Preferences.h>
#include "tv4_board.h"

namespace tv4 {
struct Contact { Point raw{}; Point pixel{}; uint16_t pressure = 0; bool down = false; };
class TouchManager {
public:
    bool begin();
    Contact read();
    bool apply(const Calibration& calibration);
    bool seen() const { return seen_; }
    bool calibrated() const { return calibrated_; }
    bool busReady() const { return busReady_; }
private:
    uint16_t adc(uint8_t command);
    SPIClass spi_{VSPI};
    Calibration calibration_{{Point{350,350}, Point{3750,350}, Point{350,3750}}};
    bool seen_ = false, calibrated_ = false, busReady_ = false;
};
}
