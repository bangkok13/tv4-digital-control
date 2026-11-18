#pragma once
#include "MachineState.h"

namespace tv4 {
class StatisticsService {
public:
    // Called once per completed interval with evidence of rotation at both ends.
    void interval(uint64_t us, double rpm);
    void endRun();
    void restore(Totals value) { RunLog log; log.totals=value; restoreLog(log); }
    void restoreLog(const RunLog& log);
    RunLog snapshot(uint64_t powered) const;
    const Operation* recent() const { return recent_; }
    const Totals& totals() const { return totals_; }
    uint64_t sessionUs() const { return sessionUs_; }
    bool dirty() const { return dirty_; }
    void markSaved() { dirty_ = false; }
private:
    Totals totals_{};
    uint64_t sessionUs_ = 0;
    uint32_t sessionPeak_=0;
    Operation recent_[4]{};
    bool inRun_ = false, dirty_ = false;
};
}
