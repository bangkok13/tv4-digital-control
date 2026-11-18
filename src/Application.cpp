#include "Application.h"
#include <esp_timer.h>

namespace tv4 {
#ifdef TV4_NETWORK_BENCH
void startNetworkBench(NetworkService&,bool);
#endif
void Application::begin() {
    Serial.begin(115200);
    diagnostics_.begin(state_.diagnostic);
    storage_.begin(state_.settings,liveStats_,&state_.workshop);
    demo_.begin(esp_timer_get_time());
    input_.setEnabled(true);
    configureCapture();
    const bool panel=lcd_.begin(state_.settings.get(Field::Brightness));
    if (!touch_.begin()) Serial.println("[error] touch SPI");
    displayReady_=panel && port_.begin(lcd_,touch_);
    if (displayReady_) {
        ui_.begin();
        Serial.printf("[UI] 6 VOLT GARAGE at %lu ms\n", millis());
    } else Serial.println("[error] display initialization; diagnostics continue over USB");
    network_.begin();
}
void Application::configureCapture() {
    input_.clearQueue();
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
        demo_.resume(now);
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
    case Action::SetInput:
        if (cmd.value>=0 && state_.workshop.set(cmd.input,unsigned(cmd.value))) storage_.edited(now);
        break;
    case Action::DemoSpeed:
        if(settings.mode==Mode::Demo) {state_.demoTarget=cmd.value;demo_.manual(cmd.value);}break;
    case Action::Portal: network_.openPortal();break;
    case Action::ArmOTA: network_.arm();break;
    case Action::CloseNetwork: network_.close();break;
    }
    nextRender_=0;
}
void Application::tick() {
    const uint64_t now=esp_timer_get_time();
    const uint32_t ms=millis();
    state_.uptimeUs=now;
    const bool demo=state_.settings.mode==Mode::Demo;
    if (demo) {
        input_.clearQueue();
        demo_.tick(now,state_.settings);
        state_.motion=demo_.motion();
    } else {
        input_.drain(rpm_);
        rpm_.tick(now);
        state_.motion=rpm_.motion();
    }
    const auto& stats=demo ? demo_.stats() : liveStats_;
    state_.totals=stats.totals(); state_.sessionUs=stats.sessionUs();
    std::copy_n(stats.recent(),4,state_.recent);
    state_.poweredUs=storage_.powered(now);
    auto& history=histories_[demo?0:1]; history.sample(now,state_.motion.hasRpm,state_.motion.rpm);
    state_.history=&history;
    state_.drive=modbus_.read();
    storage_.tick(now,state_.settings,liveStats_);
    diagnostics_.tick(state_,touch_,storage_.healthy() && calibrationStoreOK_);
    if (now>=nextNetwork_) {nextNetwork_=now+250000;network_.sync(state_);}
    if (Serial.available()) {
        const int key=Serial.read();
        if(key=='c')startCalibration();
#ifdef TV4_NETWORK_BENCH
        if(key=='t'||key=='a')startNetworkBench(network_,key=='a');
#endif
    }
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
    const bool showingSplash = ui_.screen() == Screen::Splash;
    if (showingSplash) port_.ignoreUntilReleased();
    if (now>=nextRender_ || ui_.needsUpdate()) {
        ui_.update(state_);
        if (showingSplash && ui_.screen() == Screen::Home)
            Serial.printf("[UI] tachometer at %lu ms\n", static_cast<unsigned long>(ms));
        nextRender_=now+1000ULL*state_.settings.get(Field::Refresh);
    }
    port_.tick(ms);
    if (port_.takeCalibrationRequest()) startCalibration();
    apply(ui_.take(),now);
}
}
