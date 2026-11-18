#include "RPMService.h"

namespace tv4 {
void RPMService::configure(const Settings& s) {
    if (ppr_!=s.get(Field::Pulses)) restart();
    ppr_=s.get(Field::Pulses); threshold_=s.get(Field::Threshold); weight_=s.get(Field::Smoothing);
}
void RPMService::breakChain(Signal signal) {
    seeded_=false; period_=0;
    motion_.rpm=0; motion_.hasRpm=false; motion_.rotating=false; motion_.signal=signal;
    stats_.endRun();
}
uint64_t RPMService::timeout() const {
    // First pair can be as slow as 1 RPM. Once locked, wait 2.5 periods.
    return period_ ? std::max<uint64_t>(350000,period_*5/2) : 65000000u/ppr_;
}
void RPMService::tick(uint64_t now) {
    if (seeded_ && now>=motion_.lastEdgeUs && now-motion_.lastEdgeUs>timeout()) breakChain(Signal::Silent);
}
void RPMService::overrun(uint64_t discarded) {
    motion_.lost=saturatedSum(motion_.lost,discarded);
    breakChain(Signal::Overrun);
}
void RPMService::pulse(uint64_t now) {
    if (seeded_ && (now<=motion_.lastEdgeUs || now-motion_.lastEdgeUs<(10000u+ppr_-1)/ppr_)) {
        motion_.rejected=saturatedSum(motion_.rejected,1);
        return;
    }
    tick(now);
    const auto dt=now-motion_.lastEdgeUs;
    motion_.pulses=saturatedSum(motion_.pulses,1);
    motion_.lastEdgeUs=now;
    if (!seeded_) {
        seeded_=true; motion_.signal=Signal::FirstPulse;
        return;
    }
    period_=dt;
    const double rpm=60000000.0/(double(dt)*ppr_);
    motion_.rpm=motion_.hasRpm ? motion_.rpm+(rpm-motion_.rpm)*weight_/100.0 : rpm;
    motion_.hasRpm=true; motion_.signal=Signal::Tracking;
    motion_.rotating=rpm>=threshold_;
    if (motion_.rotating) stats_.interval(dt,rpm);
    else stats_.endRun();
}
}
