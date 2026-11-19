#pragma once
#include <cstdint>
#include <optional>

namespace tv4 {
// Millimetres are stored as hundredths. Cutting speed is stored as whole m/min.
enum class Input : uint8_t {
    Diameter,
    Material,
    Tool,
    Speed,
    ThreadDiameter,
    Pitch,
    Internal,
    Left,
    Start,
    Finish,
    Depth,
    Allowance,
    Feed,
    Count
};
struct InputSpec {
    const char *name;
    const char *unit;
    uint16_t low, high;
    bool hundredths;
};
extern const InputSpec inputSpecs[];
struct Workshop {
    uint16_t value[unsigned(Input::Count)]{2000, 0, 0, 0, 1000, 100, 0, 0, 3000, 2000, 100, 20, 0};
    unsigned get(Input f) const { return value[unsigned(f)]; }
    double mm(Input f) const { return get(f) / 100.0; }
    bool set(Input f, unsigned v);
    bool valid() const;
};
const char *choice(Input f, unsigned v);
void formatInput(const Workshop &w, Input f, char *dest, unsigned size);
std::optional<unsigned> parseInput(Input f, const char *text);
constexpr unsigned pitches[]{80, 100, 125};
constexpr unsigned feeds[]{8, 10, 12};
bool supportedPitch(unsigned hundredths);
std::optional<double> turningRpm(double diameter, double vc);
std::optional<double> cuttingSpeed(double diameter, double rpm);
std::optional<double> feedTravel(double feed, double rpm);
struct Thread {
    double height, pitchDiameter, externalRoot, internalMinor, drill;
};
std::optional<Thread> threadGeometry(double diameter, double pitch);
// Positions are the direction of the handle tip, viewed from the front.
enum class LeverPos { DownRight, Down, DownLeft, Up, UpRight, UpLeft };
const char* positionName(LeverPos position);
std::optional<LeverPos> pitchPosition(unsigned pitchHundredths);
struct SpindleGear { unsigned rpm; LeverPos first, second; };
extern const SpindleGear spindleGears[6];
struct ThreadSpeed {
    double metresPerMinute;
    unsigned calculated, target, gear;
    bool belowPlate;
};
std::optional<ThreadSpeed> threadSpeed(const Workshop& work);
void formatMillimetres(unsigned hundredths, char* text, unsigned size);

struct DialPoint { uint16_t rpm = 0; uint8_t percent = 0, reserved = 0; };
struct DialPair {
    DialPoint points[2]{};
    bool valid() const;
    std::optional<unsigned> percentFor(unsigned rpm) const;
};
struct DialBook {
    DialPair gears[6]{};
    bool valid() const;
    bool record(unsigned gear, unsigned point, unsigned percent, unsigned measuredRpm);
};
static_assert(sizeof(DialBook)==48, "Keep dial journal layout compatible");
struct Passes {
    double diameterRemoval, radialRemoval, finishing;
    unsigned rough;
};
std::optional<Passes> roughing(double start, double finish, double depth, double allowance);
struct Control {
    unsigned number;
    const char *letter;
    const char *name;
    const char *purpose;
    const char *threading;
    const char *feeding;
};
extern const Control controls[9];
} // namespace tv4
