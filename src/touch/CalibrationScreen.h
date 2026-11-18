#pragma once
#include "../display/DisplayManager.h"
#include "TouchManager.h"

namespace tv4 {
class CalibrationScreen {
public:
    CalibrationScreen(DisplayManager& display, TouchManager& touch) : display_(display), touch_(touch) {}
    void start(const char* mode);
    void tick(uint32_t now, const Contact& c);
    bool active() const { return active_; }
    bool saved() const { return saved_; }
private:
    void draw(const char* hint);
    DisplayManager& display_;
    TouchManager& touch_;
    Calibration candidate_{};
    Point anchor_{};
    const char* mode_ = "";
    uint32_t since_ = 0;
    unsigned step_ = 0;
    bool active_ = false, waitingRelease_ = true, tracking_ = false, saved_ = false;
};
}
