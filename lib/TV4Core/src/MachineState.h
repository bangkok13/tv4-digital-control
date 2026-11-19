#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include "Workshop.h"
#include "RpmHistory.h"

namespace tv4 {
enum class Mode : uint8_t { Demo, Live };
enum class Signal : uint8_t { Waiting, FirstPulse, Tracking, Silent, Overrun };
enum class Field : uint8_t { Brightness, Threshold, Pulses, Refresh, Smoothing, Scale, Count };
struct FieldSpec { int minimum, maximum, step; const char* label; const char* unit; };
inline constexpr FieldSpec fields[] = {
    {10,100,5,"Яркость","%"}, {1,200,1,"Порог вращения","RPM"},
    {1,32,1,"Импульсов на оборот",""}, {50,1000,50,"Обновление RPM","мс"},
    {5,100,5,"Вес нового отсчёта","%"}, {500,6000,250,"Максимум шкалы","RPM"}
};
struct Settings {
    uint16_t values[6]{75,10,1,150,35,1500};
    Mode mode = Mode::Demo;
    uint8_t reserved = 0;
    int get(Field f) const { return values[static_cast<unsigned>(f)]; }
    bool adjust(Field f, int direction) {
        const auto index = static_cast<unsigned>(f);
        const auto& spec = fields[index];
        const auto next = std::clamp(int(values[index])+direction*spec.step, spec.minimum,spec.maximum);
        if (next == values[index]) return false;
        values[index] = uint16_t(next);
        return true;
    }
    bool valid() const {
        if (mode != Mode::Demo && mode != Mode::Live) return false;
        for (unsigned i=0; i<6; ++i)
            if (values[i] < fields[i].minimum || values[i] > fields[i].maximum) return false;
        return true;
    }
};
struct Measurement {
    Signal signal = Signal::Waiting;
    double rpm = 0;
    bool hasRpm = false, rotating = false;
    uint64_t pulses = 0, rejected = 0, lost = 0, lastEdgeUs = 0;
};
struct Totals {
    uint64_t runningUs = 0, rpmTime = 0, starts = 0;
    uint32_t peakRpm = 0, reserved = 0;
    double average() const { return runningUs ? double(rpmTime)/runningUs : 0; }
    bool valid() const { return peakRpm <= 6000 && (runningUs || (!rpmTime && !peakRpm && !starts)); }
};
enum class DriveDirection { Forward, Reverse };
struct DriveTelemetry {
    bool connected = false;
    std::optional<float> outputHz, motorCurrentA;
    std::optional<DriveDirection> direction;
    std::optional<uint16_t> statusWord, faultCode;
};
static_assert(sizeof(Settings)==14 && sizeof(Totals)==32, "Bump NVS schema if the record layout changes");
struct Diagnostics {
    uint32_t freeHeap = 0, minHeap = 0, flashBytes = 0;
    uint16_t revision = 0;
    char chip[32] = "", resetReason[28] = "";
    bool touchBus = false, touchSeen = false, calibrated = false, storageOK = true;
};
struct Operation { uint64_t us=0; uint32_t peak=0, reserved=0; };
struct RunLog {
    Totals totals{};
    Operation recent[4]{}, active{};
    uint64_t poweredUs=0;
    bool valid() const {
        if (!totals.valid() || active.peak>6000) return false;
        for (const auto& op:recent) if (op.peak>6000) return false;
        return true;
    }
};
struct NetworkInfo {
    char ssid[33]{}, ip[16]{}, apName[24]{}, apKey[20]{}, adminKey[22]{};
    int rssi=0, progress=0;
    bool connected=false, portal=false, armed=false, updating=false;
    char status[64]="Wi-Fi выключен";
};
struct MachineState {
    Settings settings{};
    Measurement motion{};
    Totals totals{};
    uint64_t uptimeUs = 0, sessionUs = 0;
    Diagnostics diagnostic{};
    DriveTelemetry drive{};
    Workshop workshop{};
    DialBook dial{};
    const RpmHistory* history=nullptr;
    Operation recent[4]{};
    uint64_t poweredUs=0;
    int demoTarget=-1;
    NetworkInfo network{};
};
inline std::optional<unsigned> liveSpindleRpm(const MachineState& s) {
    if (s.settings.mode!=Mode::Live || s.motion.signal!=Signal::Tracking || !s.motion.hasRpm ||
        !std::isfinite(s.motion.rpm) || s.motion.rpm<1 || s.motion.rpm>6000) return {};
    return unsigned(std::lround(s.motion.rpm));
}
inline uint64_t saturatedSum(uint64_t a, uint64_t b) {
    return UINT64_MAX-a < b ? UINT64_MAX : a+b;
}
}
