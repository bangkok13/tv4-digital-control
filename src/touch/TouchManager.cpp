#include "TouchManager.h"
#include <algorithm>

namespace tv4 {
bool TouchManager::begin() {
    Preferences nvs;
    if (nvs.begin("tv4-touch", true)) {
        Calibration saved{};
        if (nvs.getBytesLength(TV4_CAL_KEY) == sizeof(saved) &&
            nvs.getBytes(TV4_CAL_KEY, &saved, sizeof(saved)) == sizeof(saved) && saved.valid()) {
            calibration_ = saved;
            calibrated_ = true;
        }
        nvs.end();
    }
    pinMode(board::touchCs, OUTPUT);
    digitalWrite(board::touchCs, HIGH);
    pinMode(board::touchIrq, INPUT);
    spi_.begin(board::touchClock, board::touchMiso, board::touchMosi, board::touchCs);
    busReady_ = spi_.bus() != nullptr;
    if (busReady_) {
        spi_.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
        digitalWrite(board::touchCs, LOW);
        adc(0x90); // Arm PENIRQ even after a warm reset of the ESP32.
        digitalWrite(board::touchCs, HIGH);
        spi_.endTransaction();
    }
    return busReady_;
}
uint16_t TouchManager::adc(uint8_t command) {
    spi_.transfer(command);
    return (spi_.transfer16(0) >> 3) & 0xfff;
}
Contact TouchManager::read() {
    Contact c{};
    if (!busReady_ || digitalRead(board::touchIrq) != LOW) return c;
    spi_.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    digitalWrite(board::touchCs, LOW);
    // Differential 12-bit conversions. Drop the first coordinate after each mux change.
    auto coordinate = [this](uint8_t cmd) {
        adc(cmd);
        std::array<uint16_t, 3> samples{adc(cmd), adc(cmd), adc(cmd)};
        std::sort(samples.begin(), samples.end());
        return int16_t(samples[1]);
    };
    const auto z1 = adc(0xb1), z2 = adc(0xc1);
    c.raw = {coordinate(0xd1), coordinate(0x91)};
    adc(0x90); // Power down and re-enable PENIRQ.
    digitalWrite(board::touchCs, HIGH);
    spi_.endTransaction();
    c.pressure = uint16_t(z1 + 4095 - z2);
    c.down = z1 > 0 && z2 < 4095 && c.pressure >= 300 &&
             c.raw.x > 0 && c.raw.x < 4095 && c.raw.y > 0 && c.raw.y < 4095;
    if (c.down) {
        c.pixel = calibration_.project(c.raw);
        seen_ = true;
    }
    return c;
}
bool TouchManager::apply(const Calibration& value) {
    if (!value.valid()) return false;
    calibration_ = value;
    calibrated_ = true;
    Preferences nvs;
    if (!nvs.begin("tv4-touch", false)) return false;
    const bool saved = nvs.putBytes(TV4_CAL_KEY, &value, sizeof(value)) == sizeof(value);
    nvs.end();
    return saved;
}
}
