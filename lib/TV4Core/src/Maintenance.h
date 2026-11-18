#pragma once
#include <cstdint>

namespace tv4 {
// A quiet input is only a veto for a locally confirmed maintenance window.
inline bool inputQuietForUpdate(uint64_t now, uint64_t lastPhysicalEdge, bool liveMeasurement) {
    constexpr uint64_t startupUs = 5000000, slowRevolutionUs = 65000000;
    if (liveMeasurement || now < lastPhysicalEdge)
        return false;
    return now - lastPhysicalEdge > (lastPhysicalEdge ? slowRevolutionUs : startupUs);
}
} // namespace tv4
