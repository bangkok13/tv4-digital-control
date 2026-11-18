#include "StatisticsService.h"

namespace tv4 {
void StatisticsService::endRun() {
    if (inRun_ && sessionUs_) {
        std::move_backward(recent_,recent_+3,recent_+4);
        recent_[0]={sessionUs_,sessionPeak_,0}; dirty_=true;
    }
    sessionUs_=0; sessionPeak_=0; inRun_=false;
}
RunLog StatisticsService::snapshot(uint64_t powered) const {
    RunLog log; log.totals=totals_; log.poweredUs=powered;
    std::copy_n(recent_,4,log.recent); log.active={sessionUs_,sessionPeak_,0}; return log;
}
void StatisticsService::restoreLog(const RunLog& log) {
    totals_=log.totals; std::copy_n(log.recent,4,recent_);
    sessionUs_=log.active.us; sessionPeak_=log.active.peak; inRun_=sessionUs_!=0;
    endRun(); dirty_=false;
}
void StatisticsService::interval(uint64_t us, double rpm) {
    if (!us || !std::isfinite(rpm) || rpm <= 0 || rpm > 6000) return;
    if (!inRun_) {
        totals_.starts = saturatedSum(totals_.starts, 1);
        inRun_ = true;
    }
    sessionUs_ = saturatedSum(sessionUs_, us);
    totals_.runningUs = saturatedSum(totals_.runningUs, us);
    const long double weighted = static_cast<long double>(rpm) * us;
    const uint64_t contribution = weighted >= static_cast<long double>(UINT64_MAX) - 0.5L
        ? UINT64_MAX : static_cast<uint64_t>(weighted + 0.5L);
    totals_.rpmTime = saturatedSum(totals_.rpmTime, contribution);
    totals_.peakRpm = std::max(totals_.peakRpm, uint32_t(std::lround(rpm)));
    sessionPeak_=std::max(sessionPeak_,uint32_t(std::lround(rpm)));
    dirty_ = true;
}
}
