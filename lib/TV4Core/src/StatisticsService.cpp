#include "StatisticsService.h"

namespace tv4 {
void StatisticsService::interval(uint64_t us, double rpm) {
    if (!us || !std::isfinite(rpm) || rpm <= 0 || rpm > 6000) return;
    if (!inRun_) {
        totals_.starts = saturatedSum(totals_.starts, 1);
        inRun_ = true;
    }
    sessionUs_ = saturatedSum(sessionUs_, us);
    totals_.runningUs = saturatedSum(totals_.runningUs, us);
    totals_.rpmTime = saturatedSum(totals_.rpmTime, uint64_t(std::llround(rpm*us)));
    totals_.peakRpm = std::max(totals_.peakRpm, uint32_t(std::lround(rpm)));
    dirty_ = true;
}
}
