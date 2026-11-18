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
struct Passes {
    double diameterRemoval, radialRemoval, finishing;
    unsigned rough;
};
std::optional<Passes> roughing(double start, double finish, double depth, double allowance);
struct Control {
    unsigned number;
    const char *name;
    const char *purpose;
    const char *threading;
    const char *feeding;
};
extern const Control controls[9];
} // namespace tv4
