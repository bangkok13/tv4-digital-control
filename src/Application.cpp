#include "Application.h"
#include <esp_timer.h>

namespace tv4 {
void Application::begin() {
    Serial.begin(115200);
    diagnostics_.begin(state_.diagnostic);
    storage_.begin(state_.settings,liveStats_);
    demo_.begin(esp_timer_get_time());
    configureCapture();
    const bool panel=lcd_.begin(state_.settings.get(Field::Brightness));
    if (!touch_.begin()) Serial.println("[error] touch SPI");
    displayReady_=panel && port_.begin(lcd_,touch_);
    if (displayReady_) ui_.begin();
    else Serial.println("[error] display initialization; diagnostics continue over USB");
}
void Application::configureCapture() {
    input_.setEnabled(state_.settings.mode==Mode::Live);
    rpm_.configure(state_.settings);
    rpm_.restart();
}
void Application::startCalibration() {
    if (displayReady_) calibration_.start(state_.settings.mode==Mode::Demo ? "DEMO" : "LIVE");
}
void Application::apply(Command cmd,uint64_t now) {
    auto& settings=state_.settings;
    switch(cmd.action) {
    case Action::None: return;
    case Action::Calibrate: startCalibration(); break;
    case Action::ToggleMode:
        settings.mode=settings.mode==Mode::Demo ? Mode::Live : Mode::Demo;
        configureCapture();
        state_.motion={};
        storage_.edited(now);
        break;
    case Action::Decrease: case Action::Increase:
        if (settings.adjust(cmd.field,cmd.action==Action::Increase ? 1 : -1)) {
            if (cmd.field==Field::Brightness) lcd_.setBrightness(settings.get(Field::Brightness));
            if (cmd.field==Field::Pulses) configureCapture();
            else if (cmd.field==Field::Threshold || cmd.field==Field::Smoothing) rpm_.configure(settings);
            storage_.edited(now);
        }
        break;
    case Action::Reset:
        if (settings.mode==Mode::Demo) demo_.resetStats();
        else if (storage_.reset(now,liveStats_)) configureCapture();
        else ui_.resetFailed();
        break;
    }
    nextRender_=0;
}
void Application::tick() {
    const uint64_t now=esp_timer_get_time();
    const uint32_t ms=millis();
    state_.uptimeUs=now;
    const bool demo=state_.settings.mode==Mode::Demo;
    if (demo) {
        demo_.tick(now,state_.settings);
        state_.motion=demo_.motion();
    } else {
        input_.drain(rpm_);
        rpm_.tick(now);
        state_.motion=rpm_.motion();
    }
    const auto& stats=demo ? demo_.stats() : liveStats_;
    state_.totals=stats.totals(); state_.sessionUs=stats.sessionUs();
    state_.drive=modbus_.read();
    storage_.tick(now,state_.settings,liveStats_);
    diagnostics_.tick(state_,touch_,storage_.healthy() && calibrationStoreOK_);
    if (Serial.available() && Serial.read()=='c') startCalibration();
    if (!displayReady_) return;
    if (calibration_.active()) {
        if (now>=nextCalRead_) {
            nextCalRead_=now+12000;
            calibration_.tick(ms,touch_.read());
            if (!calibration_.active()) {
                calibrationStoreOK_=calibration_.saved();
                port_.ignoreUntilReleased();
                ui_.redraw(); nextRender_=0;
            }
        }
        return;
    }
    if (now>=nextRender_ || ui_.needsUpdate()) {
        ui_.update(state_);
        nextRender_=now+1000ULL*state_.settings.get(Field::Refresh);
    }
    port_.tick(ms);
    if (port_.takeCalibrationRequest()) startCalibration();
    apply(ui_.take(),now);
}
}
