#include "DemoService.h"

namespace tv4 {
void DemoService::begin(uint64_t now) {
    epoch_ = previous_ = now;
    pulseRemainder_ = 0;
    motion_ = {};
    stats_.restore({7200000000ULL, 7200000000000ULL, 12, 1480, 0});
}
void DemoService::tick(uint64_t now, const Settings& settings) {
    const uint64_t dt = now-previous_;
    if (dt < 50000) return;
    previous_ = now;
    const double t = double((now-epoch_) % 40000000ULL)/1000000;
    double rpm = 0;
    if (t < 8) rpm = 1500*t/8;
    else if (t < 16) rpm = 1500;
    else if (t < 26) rpm = 1375 + 125*std::cos((t-16)*0.628318530718);
    else if (t < 34) rpm = 1500*(34-t)/8;
    motion_.rpm = rpm;
    motion_.hasRpm = true;
    motion_.rotating = rpm >= settings.get(Field::Threshold);
    motion_.signal = rpm > 0 ? Signal::Tracking : Signal::Silent;
    if (dt > 1000000) { stats_.endRun(); return; }
    pulseRemainder_ += rpm*dt*settings.get(Field::Pulses)/60000000.0;
    const auto pulses = uint64_t(pulseRemainder_);
    pulseRemainder_ -= pulses;
    if (pulses) motion_.lastEdgeUs = now;
    motion_.pulses = saturatedSum(motion_.pulses, pulses);
    if (motion_.rotating) stats_.interval(dt, rpm);
    else stats_.endRun();
}
}
