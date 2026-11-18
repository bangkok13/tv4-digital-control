#include "Calibration.h"
#include <algorithm>
#include <cstdlib>

namespace tv4 {
namespace {
int32_t cross(Point a, Point b, Point c) {
    return int32_t(b.x-a.x)*(c.y-a.y) - int32_t(b.y-a.y)*(c.x-a.x);
}
}
bool Calibration::valid() const {
    for (auto p : raw) {
        if (p.x < 1 || p.x > 4094 || p.y < 1 || p.y > 4094) return false;
    }
    return std::abs(cross(raw[0], raw[1], raw[2])) >= 500000;
}
Point Calibration::project(Point p) const {
    const auto det = cross(raw[0], raw[1], raw[2]);
    if (!det) return {0, 0};
    const auto x = targets[0].x + int64_t(targets[1].x-targets[0].x)*cross(raw[0], p, raw[2])/det;
    const auto y = targets[0].y + int64_t(targets[2].y-targets[0].y)*cross(raw[0], raw[1], p)/det;
    return {int16_t(std::clamp<int64_t>(x, 0, 239)), int16_t(std::clamp<int64_t>(y, 0, 319))};
}
}
