#pragma once
#include <MachineState.h>

namespace tv4 {
class TelemetryReader {
public:
    virtual ~TelemetryReader() = default;
    virtual DriveTelemetry read() const = 0;
};
class ModbusService final : public TelemetryReader {
public:
    DriveTelemetry read() const override { return {}; }
};
}
