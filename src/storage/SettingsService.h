#pragma once
#include <Journal.h>
#include <StatisticsService.h>
#include <Preferences.h>

namespace tv4 {
class NvsStore final : public BlobStore {
public:
    bool open() { return prefs_.begin("tv4-panel",false); }
    size_t length(const char* k) override { return prefs_.isKey(k) ? prefs_.getBytesLength(k) : 0; }
    bool read(const char* k,void* p,size_t n) override { return prefs_.getBytes(k,p,n)==n; }
    bool write(const char* k,const void* p,size_t n) override { return prefs_.putBytes(k,p,n)==n; }
private: Preferences prefs_;
};
class SettingsService {
public:
    void begin(Settings& settings,StatisticsService& stats,Workshop* workshop=nullptr);
    void edited(uint64_t now) { editedAt_=now; pending_=true; }
    void tick(uint64_t now,const Settings& settings,StatisticsService& stats);
    bool reset(uint64_t now,StatisticsService& stats);
    bool healthy() const { return open_ && settingsOK_ && statsOK_; }
    uint64_t powered(uint64_t now) const { return saturatedSum(priorPower_,now); }
private:
    struct Config { Settings panel{}; Workshop work{}; bool valid() const { return panel.valid()&&work.valid(); } };
    NvsStore store_;
    Journal<Settings> settings_{store_,"cfg-a","cfg-b",0x53455401};
    Journal<Totals> totals_{store_,"run-a","run-b",0x52554e01};
    Journal<Config> config2_{store_,"cfg2-a","cfg2-b",0x53455402};
    Journal<RunLog> log2_{store_,"run2-a","run2-b",0x52554e02};
    Workshop* work_=nullptr;
    uint64_t priorPower_=0;
    uint64_t editedAt_=0, cfgAttempt_=0, statsAttempt_=0;
    bool pending_=false, open_=false, settingsOK_=true, statsOK_=true;
};
}
