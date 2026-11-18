#include <Calibration.h>
#include <DemoService.h>
#include <Journal.h>
#include <Maintenance.h>
#include <RPMService.h>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>
using namespace tv4;
namespace {
unsigned checks;
void check(bool value,const char* expr,int line) {
    ++checks;
    if (!value) { std::fprintf(stderr,"line %d failed: %s\n",line,expr); std::exit(1); }
}
#define CHECK(expr) check((expr),#expr,__LINE__)
bool near(double x,double y) { return std::abs(x-y)<0.01; }
struct Bench {
    Settings cfg;
    StatisticsService stats;
    RPMService rpm{stats};
    Bench() { rpm.configure(cfg); }
};
void steadyNoiseAndStop() {
    Bench b;
    b.rpm.tick(999999); CHECK(!b.rpm.motion().hasRpm);
    const uint64_t start=UINT32_MAX-10000ULL;
    b.rpm.pulse(start); CHECK(b.rpm.motion().signal==Signal::FirstPulse);
    for (unsigned i=1;i<=25;++i) {
        b.rpm.pulse(start+i*48000ULL-47500);
        b.rpm.pulse(start+i*48000ULL);
    }
    const auto& m=b.rpm.motion();
    CHECK(near(m.rpm,1250)); CHECK(m.rotating); CHECK(m.pulses==26); CHECK(m.rejected==25);
    CHECK(b.stats.totals().runningUs==1200000); CHECK(b.stats.totals().starts==1);
    CHECK(b.stats.totals().peakRpm==1250); CHECK(near(b.stats.totals().average(),1250));
    b.rpm.tick(m.lastEdgeUs+350001);
    CHECK(!m.hasRpm && m.signal==Signal::Silent); CHECK(!b.stats.sessionUs());
    CHECK(b.stats.totals().runningUs==1200000);
    b.rpm.pulse(start+5000000); CHECK(!m.hasRpm);
    b.rpm.pulse(start+5060000); CHECK(b.stats.totals().starts==2); CHECK(near(m.rpm,1000));
    CHECK(b.stats.sessionUs()==60000); CHECK(near(b.stats.totals().average(),1560000000.0/1260000));
}
void slowAndMultipleTargets() {
    Bench b;
    b.cfg.values[unsigned(Field::Threshold)]=1;
    b.rpm.configure(b.cfg);
    b.rpm.pulse(0); b.rpm.tick(59999999); b.rpm.pulse(60000000);
    CHECK(b.rpm.motion().hasRpm && near(b.rpm.motion().rpm,1));
    b.rpm.tick(209999999); CHECK(b.rpm.motion().rotating);
    b.rpm.tick(210000001); CHECK(!b.rpm.motion().hasRpm);
    CHECK(b.stats.totals().runningUs==60000000);
    b.cfg.values[unsigned(Field::Pulses)]=4;
    b.rpm.configure(b.cfg); b.rpm.pulse(300000000); b.rpm.pulse(300015000);
    CHECK(near(b.rpm.motion().rpm,1000));
    b.rpm.pulse(300045000); CHECK(near(b.rpm.motion().rpm,825));
    b.rpm.overrun(180); CHECK(b.rpm.motion().lost==180 && !b.rpm.motion().hasRpm);
    const auto accounted=b.stats.totals().runningUs;
    b.rpm.pulse(302000000); CHECK(b.stats.totals().runningUs==accounted);
    b.rpm.pulse(302015000); CHECK(b.stats.totals().runningUs==accounted+15000);
    b.rpm.pulse(302015000); CHECK(b.rpm.motion().rejected==1);
    b.rpm.pulse(310000000); CHECK(b.rpm.motion().signal==Signal::FirstPulse);
}
void thresholdAndLongRun() {
    Bench b;
    b.rpm.pulse(100); b.rpm.pulse(12000100);
    CHECK(near(b.rpm.motion().rpm,5) && !b.rpm.motion().rotating); CHECK(!b.stats.dirty());
    CHECK(saturatedSum(UINT64_MAX-2,8)==UINT64_MAX);
    StatisticsService lifetime;lifetime.interval(UINT64_MAX,6000);
    CHECK(lifetime.totals().runningUs==UINT64_MAX&&lifetime.totals().rpmTime==UINT64_MAX);
    Bench fast; fast.cfg.values[unsigned(Field::Pulses)]=32; fast.rpm.configure(fast.cfg);
    uint64_t t=8000000000000ULL; fast.rpm.pulse(t);
    for (unsigned i=0;i<2000000;++i) fast.rpm.pulse(t+=1250);
    CHECK(near(fast.rpm.motion().rpm,1500)); CHECK(fast.stats.totals().starts==1);
    CHECK(fast.stats.totals().runningUs==2500000000ULL);
}
class Memory final : public BlobStore {
public:
    std::map<std::string,std::vector<uint8_t>> records;
    bool failWrite=false;
    size_t length(const char* k) override { return records[k].size(); }
    bool read(const char* k,void* dest,size_t size) override {
        auto& v=records[k]; if (v.size()!=size) return false;
        std::memcpy(dest,v.data(),size); return true;
    }
    bool write(const char* k,const void* data,size_t size) override {
        const auto* bytes=static_cast<const uint8_t*>(data);
        records[k]={bytes,bytes+(failWrite ? size/2 : size)};
        return !failWrite;
    }
};
void journalRecovery() {
    Memory memory; Settings cfg,restored;
    Journal<Settings> j(memory,"a","b",1);
    CHECK(j.load(restored)==Load::Empty); CHECK(restored.mode==Mode::Demo);
    CHECK(j.save(cfg)); cfg.mode=Mode::Live; CHECK(j.save(cfg));
    CHECK(j.load(restored)==Load::Valid && restored.mode==Mode::Live);
    memory.records["b"].back()^=0x80;
    CHECK(j.load(restored)==Load::Recovered && restored.mode==Mode::Demo);
    memory.failWrite=true; CHECK(!j.save(cfg));
    Journal<Settings> reboot(memory,"a","b",1);
    CHECK(reboot.load(restored)==Load::Recovered && restored.mode==Mode::Demo);
    memory.failWrite=false; CHECK(reboot.save(cfg)); CHECK(j.load(restored)==Load::Valid && restored.mode==Mode::Live);
    cfg.values[0]=65535; CHECK(!j.save(cfg));
    Journal<Settings> wrongSchema(memory,"a","b",2); CHECK(wrongSchema.load(restored)==Load::Damaged);
    Totals t{1000000,600000000,1,600,0}, read{};
    Journal<Totals> hist(memory,"h1","h2",8);
    CHECK(hist.save(t)); CHECK(hist.save({})); CHECK(hist.load(read)==Load::Valid && !read.runningUs);
    CHECK(j.load(restored)==Load::Valid && restored.mode==Mode::Live);
}
void touchAndDemo() {
    DriveTelemetry drive{};
    CHECK(!drive.connected && !drive.outputHz && !drive.motorCurrentA && !drive.direction && !drive.statusWord && !drive.faultCode);
    Calibration swapped{{Point{3700,300},Point{3700,3700},Point{300,300}}};
    CHECK(swapped.valid());
    for (unsigned i=0;i<3;++i) {
        auto p=swapped.project(swapped.raw[i]); CHECK(p.x==targets[i].x && p.y==targets[i].y);
    }
    CHECK(!Calibration{}.valid());
    DemoService demo; Settings cfg; demo.begin(0);
    for (unsigned i=1;i<=800;++i) {
        demo.tick(i*50000ULL,cfg);
        CHECK(demo.motion().hasRpm && demo.motion().rpm>=0 && demo.motion().rpm<=1500);
        if (i==160 || i==240) CHECK(near(demo.motion().rpm,1500));
        if (i==720) CHECK(near(demo.motion().rpm,0));
    }
    CHECK(demo.stats().totals().runningUs>7200000000ULL);
    demo.resetStats(); CHECK(demo.stats().totals().runningUs==0);
    demo.manual(250);demo.tick(40100000,cfg);CHECK(near(demo.motion().rpm,250));
    demo.manual(0);demo.tick(40200000,cfg);CHECK(!demo.motion().rotating);
}
void workshopAndGraphs() {
    CHECK(!inputQuietForUpdate(5000000,0,false));CHECK(inputQuietForUpdate(5000001,0,false));
    CHECK(!inputQuietForUpdate(90000000,30000000,false));CHECK(inputQuietForUpdate(95000001,30000000,false));
    CHECK(!inputQuietForUpdate(100000000,30000000,true));CHECK(!inputQuietForUpdate(5,10,false));
    CHECK(near(*turningRpm(20,100),1591.54943));CHECK(near(*cuttingSpeed(20,1000),62.83185));
    CHECK(near(*feedTravel(.12,710),85.2));CHECK(!turningRpm(0,5));CHECK(!cuttingSpeed(10,NAN));CHECK(!feedTravel(-.1,50));
    CHECK(supportedPitch(80)&&supportedPitch(100)&&supportedPitch(125)&&!supportedPitch(150));
    const auto g=threadGeometry(10,1.25);CHECK(g && near(g->height,1.08253)&&near(g->pitchDiameter,9.1881));
    CHECK(near(g->externalRoot,8.4664)&&near(g->internalMinor,8.6468)&&near(g->drill,8.75));CHECK(!threadGeometry(1,2));
    auto p=roughing(30,20,1,.2);CHECK(p && p->rough==5 && near(p->radialRemoval,5)&&near(p->finishing,.2));
    CHECK(roughing(20,20,1,0)->rough==0);CHECK(!roughing(20,22,1,0));CHECK(!roughing(22,20,0,0));CHECK(!roughing(22,20,1,2));
    CHECK(parseInput(Input::Pitch,"1,25")==125);CHECK(!parseInput(Input::Pitch,"1.254"));CHECK(!parseInput(Input::Pitch,"-1"));
    CHECK(!parseInput(Input::Diameter,"655.36"));CHECK(!parseInput(Input::Diameter,"nan"));CHECK(parseInput(Input::Depth,".02")==2);
    RpmHistory h; CHECK(!h.view(30,0).count);
    for(unsigned i=0;i<650;++i)h.sample(i*1000000ULL,i!=640,double(i));
    auto slice=h.view(30,649000000);CHECK(slice.width==30&&slice.count==29&&slice.minimum==620&&slice.maximum==649);
    CHECK(slice.points[20]==-1 && near(slice.mean,(30*634.5-640)/29));
    CHECK(h.view(120,649000000).count==119&&h.view(600,649000000).count==599);
    h.sample(2000000000ULL,true,0);slice=h.view(600,2000000000ULL);CHECK(slice.count==1 && !slice.minimum);
    StatisticsService s;s.interval(100000,600);s.endRun();s.interval(200000,300);
    auto saved=s.snapshot(12345);StatisticsService boot;boot.restoreLog(saved);
    CHECK(boot.recent()[0].us==200000&&boot.recent()[1].peak==600&&!boot.sessionUs());
    CHECK(boot.totals().starts==2&&boot.snapshot(23456).poweredUs==23456);
}
}
int main() {
    steadyNoiseAndStop(); slowAndMultipleTargets(); thresholdAndLongRun(); journalRecovery(); touchAndDemo();workshopAndGraphs();
    std::printf("OK: %u checks, including 2,000,000 pulse intervals\n",checks);
}
