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
    Preferences::data.clear();NvsStore old;old.open();
    Journal<Settings> oldConfig(old,"cfg-a","cfg-b",0x53455401);
    Journal<Totals> oldRun(old,"run-a","run-b",0x52554e01);
    Settings first;first.mode=Mode::Live;first.values[0]=90;
    check(oldConfig.save(first)&&oldRun.save({1000000,850000000,1,850,0}),"seed v1");
    Workshop w;Settings cfg;StatisticsService machine;SettingsService upgrade;upgrade.begin(cfg,machine,&w);
    check(cfg.mode==Mode::Live&&cfg.get(Field::Brightness)==90&&machine.totals().peakRpm==850,"migrate v1");
    w.set(Input::Pitch,125);upgrade.edited(1);upgrade.tick(3000000,cfg,machine);
    machine.interval(100000,600);upgrade.tick(300000000,cfg,machine);
    Workshop restored;SettingsService again;again.begin(cfg,machine,&restored);
    check(restored.get(Input::Pitch)==125&&machine.recent()[0].us==100000&&again.powered(2000000)==302000000,"v2 state on restart");
    DialBook dial;
    SettingsService on21;on21.begin(cfg,machine,&restored,&dial);
    check(dial.valid()&&!dial.gears[0].percentFor(120)&&restored.get(Input::Pitch)==125,"v2.0.x upgrade");
    check(dial.record(2,0,25,110)&&dial.record(2,1,75,210)&&on21.saveDial(dial),"store measured dial");
    Preferences::fail=true;DialBook changed=dial;changed.record(2,1,80,230);
    check(!on21.saveDial(changed),"failed dial write");Preferences::fail=false;
    SettingsService dialBoot;DialBook recovered;dialBoot.begin(cfg,machine,&restored,&recovered);
    check(recovered.gears[2].percentFor(160)==50 && !recovered.gears[1].percentFor(160),"per-gear dial survives reboot");
    std::puts("OK: production NVS scheduler, write bounds, retry, reset and recovery on simulated reboot");
}
