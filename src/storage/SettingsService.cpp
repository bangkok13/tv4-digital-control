#include "SettingsService.h"

namespace tv4 {
void SettingsService::begin(Settings& s,StatisticsService& stats) {
    open_=store_.open();
    if (!open_) { Serial.println("[NVS] open failed; RAM only"); return; }
    Totals t{};
    auto cfg=settings_.load(s), hist=totals_.load(t);
    settingsOK_=cfg!=Load::Damaged; statsOK_=hist!=Load::Damaged;
    if (cfg==Load::Recovered || hist==Load::Recovered) Serial.println("[NVS] restored surviving generation");
    if (!healthy()) Serial.println("[NVS] damaged record; using defaults for this record");
    stats.restore(t);
}
void SettingsService::tick(uint64_t now,const Settings& s,StatisticsService& stats) {
    if (!open_) return;
    if (pending_ && now-editedAt_>=2000000 && (!cfgAttempt_ || now-cfgAttempt_>=10000000)) {
        cfgAttempt_=now;
        settingsOK_=settings_.save(s);
        pending_=!settingsOK_;
        Serial.println(settingsOK_ ? "[NVS] settings saved" : "[NVS] settings write failed");
    }
    if (stats.dirty() && now-statsAttempt_>=300000000) {
        statsAttempt_=now;
        statsOK_=totals_.save(stats.totals());
        if (statsOK_) stats.markSaved();
        Serial.println(statsOK_ ? "[NVS] motion totals saved" : "[NVS] totals write failed");
    }
}
bool SettingsService::reset(uint64_t now,StatisticsService& stats) {
    statsAttempt_=now;
    statsOK_=open_ && totals_.save(Totals{});
    if (statsOK_) stats.restore({});
    Serial.println(statsOK_ ? "[NVS] LIVE totals reset" : "[NVS] reset failed");
    return statsOK_;
}
}
