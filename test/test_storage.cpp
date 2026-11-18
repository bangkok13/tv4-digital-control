#include "SettingsService.h"
#include <cstdlib>
using namespace tv4;
namespace {
void check(bool value,const char* message) {
    if (!value) { std::fprintf(stderr,"storage: %s\n",message); std::exit(1); }
}
}
int main() {
    Settings settings;
    StatisticsService live;
    SettingsService service;
    service.begin(settings,live);
    check(service.healthy() && settings.mode==Mode::Demo && Preferences::writes==0,"empty NVS");
    settings.mode=Mode::Live; service.edited(1);
    service.tick(2000000,settings,live); check(Preferences::writes==0,"debounce");
    service.tick(2000001,settings,live); check(Preferences::writes==1,"first settings write");
    settings.adjust(Field::Brightness,1); service.edited(3000000);
    service.tick(11999999,settings,live); check(Preferences::writes==1,"settings rate limit");
    service.tick(12000001,settings,live); check(Preferences::writes==2,"next settings generation");
    for (unsigned sec=13;sec<300;++sec) {
        live.interval(1000000,600);
        service.tick(sec*1000000ULL,settings,live);
    }
    check(Preferences::writes==2,"no periodic flash writes per pulse or per second");
    service.tick(300000000,settings,live); check(Preferences::writes==3 && !live.dirty(),"five-minute checkpoint");
    Settings after; StatisticsService resumed; SettingsService reboot;
    reboot.begin(after,resumed);
    check(after.mode==Mode::Live && after.get(Field::Brightness)==80,"settings after reboot");
    check(resumed.totals().starts==1 && resumed.totals().runningUs==287000000,"totals after reboot");
    Preferences::fail=true;
    check(!reboot.reset(310000000,resumed) && !reboot.healthy() && resumed.totals().starts==1,"failed reset preserves totals");
    Preferences::fail=false;
    check(reboot.reset(320000000,resumed) && !resumed.totals().runningUs,"explicit reset commits");
    Settings finalCfg; StatisticsService finalStats; SettingsService finalReboot;
    finalReboot.begin(finalCfg,finalStats);
    check(finalStats.totals().runningUs==0 && finalCfg.mode==Mode::Live,"reset survives reboot");
    finalCfg.mode=Mode::Demo; finalReboot.edited(1);
    Preferences::fail=true; finalReboot.tick(3000000,finalCfg,finalStats);
    check(!finalReboot.healthy(),"failed config storage visible");
    Preferences::fail=false; finalReboot.tick(13000001,finalCfg,finalStats);
    check(finalReboot.healthy(),"failed write retried");
    std::puts("OK: production NVS scheduler, write bounds, retry, reset and recovery on simulated reboot");
}
