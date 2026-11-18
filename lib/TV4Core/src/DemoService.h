#pragma once
#include "StatisticsService.h"

namespace tv4 {
class DemoService {
public:
    void begin(uint64_t now);
    void tick(uint64_t now, const Settings& settings);
    void resetStats() { stats_.restore({}); }
    void manual(int target) { target_=target<0 ? -1 : std::min(6000,target); }
    void resume(uint64_t now) { previous_=now; stats_.endRun(); }
    const Measurement& motion() const { return motion_; }
    const StatisticsService& stats() const { return stats_; }
private:
    uint64_t epoch_ = 0, previous_ = 0;
    double pulseRemainder_ = 0;
    int target_=-1;
    Measurement motion_{};
    StatisticsService stats_;
};
}
