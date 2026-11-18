#pragma once
#include "MachineState.h"

namespace tv4 {
class StatisticsService {
public:
    // Called once per completed interval with evidence of rotation at both ends.
    void interval(uint64_t us, double rpm);
    void endRun() { inRun_ = false; sessionUs_ = 0; }
    void restore(Totals value) { totals_ = value; endRun(); dirty_ = false; }
    const Totals& totals() const { return totals_; }
    uint64_t sessionUs() const { return sessionUs_; }
    bool dirty() const { return dirty_; }
    void markSaved() { dirty_ = false; }
private:
    Totals totals_{};
    uint64_t sessionUs_ = 0;
    bool inRun_ = false, dirty_ = false;
};
}
