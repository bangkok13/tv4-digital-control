#pragma once
#include "StatisticsService.h"

namespace tv4 {
class RPMService {
public:
    explicit RPMService(StatisticsService& stats) : stats_(stats) {}
    void configure(const Settings& settings);
    void restart() { breakChain(Signal::Waiting); }
    void pulse(uint64_t timeUs);
    void tick(uint64_t now);
    void overrun(uint64_t discarded);
    const Measurement& motion() const { return motion_; }
private:
    void breakChain(Signal signal);
    uint64_t timeout() const;
    StatisticsService& stats_;
    Measurement motion_{};
    uint64_t period_=0;
    int ppr_=1, threshold_=10, weight_=35;
    bool seeded_=false;
};
}
