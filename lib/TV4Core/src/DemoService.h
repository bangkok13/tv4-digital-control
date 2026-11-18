#pragma once
#include "StatisticsService.h"

namespace tv4 {
class DemoService {
public:
    void begin(uint64_t now);
    void tick(uint64_t now, const Settings& settings);
    void resetStats() { stats_.restore({}); }
    const Measurement& motion() const { return motion_; }
    const StatisticsService& stats() const { return stats_; }
private:
    uint64_t epoch_ = 0, previous_ = 0;
    double pulseRemainder_ = 0;
    Measurement motion_{};
    StatisticsService stats_;
};
}
