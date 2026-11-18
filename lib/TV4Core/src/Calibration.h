#pragma once
#include <array>
#include <cstdint>

namespace tv4 {
struct Point { int16_t x, y; };
constexpr std::array<Point, 4> targets{{{22,32}, {217,32}, {22,287}, {190,254}}};
struct Calibration {
    std::array<Point, 3> raw{};
    bool valid() const;
    Point project(Point point) const;
};
}
