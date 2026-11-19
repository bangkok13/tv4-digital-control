#include "SettingsService.h"

namespace tv4 {
void SettingsService::begin(Settings& s,StatisticsService& stats,Workshop* work,DialBook* dial) {
    work_=work;
    open_=store_.open();
    if (!open_) { Serial.println("[NVS] open failed; RAM only"); return; }
    Config config; RunLog log;
    auto cfg=config2_.load(config), hist=log2_.load(log);
    if (dial) dialOK_=dial_.load(*dial)!=Load::Damaged;
    if (cfg==Load::Empty) cfg=settings_.load(s);
    else if (cfg!=Load::Damaged) { s=config.panel; if (work_) *work_=config.work; }
    if (hist==Load::Empty) hist=totals_.load(log.totals);
    settingsOK_=cfg!=Load::Damaged; statsOK_=hist!=Load::Damaged;
    if (cfg==Load::Recovered || hist==Load::Recovered) Serial.println("[NVS] restored surviving generation");
    if (!healthy()) Serial.println("[NVS] damaged record; using defaults for this record");
    priorPower_=log.poweredUs;
    stats.restoreLog(log);
}
void SettingsService::tick(uint64_t now,const Settings& s,StatisticsService& stats) {
    if (!open_) return;
    if (pending_ && now-editedAt_>=2000000 && (!cfgAttempt_ || now-cfgAttempt_>=10000000)) {
        cfgAttempt_=now;
        settingsOK_=config2_.save({s,work_ ? *work_ : Workshop{}});
        pending_=!settingsOK_;
        Serial.println(settingsOK_ ? "[NVS] settings saved" : "[NVS] settings write failed");
    }
    if (now-statsAttempt_>=300000000) {
        statsAttempt_=now;
        statsOK_=log2_.save(stats.snapshot(powered(now)));
        if (statsOK_) stats.markSaved();
        Serial.println(statsOK_ ? "[NVS] motion totals saved" : "[NVS] totals write failed");
    }
}
bool SettingsService::reset(uint64_t now,StatisticsService& stats) {
    statsAttempt_=now;
    RunLog cleared; cleared.poweredUs=powered(now);
    statsOK_=open_ && log2_.save(cleared);
    if (statsOK_) stats.restore({});
    Serial.println(statsOK_ ? "[NVS] LIVE totals reset" : "[NVS] reset failed");
    return statsOK_;
}
}
