#pragma once
#include <DemoService.h>
#include <UIManager.h>
#include "services/DiagnosticsService.h"
#include "services/ModbusService.h"
#include "services/NetworkService.h"
#include "sensors/PulseInput.h"
#include "storage/SettingsService.h"
#include "touch/CalibrationScreen.h"
#include "ui/LvglPort.h"

namespace tv4 {
class Application {
public:
    void begin();
    void tick();
private:
    void apply(Command cmd,uint64_t now);
    void startCalibration();
    void configureCapture();
    DisplayManager lcd_;
    TouchManager touch_;
    CalibrationScreen calibration_{lcd_,touch_};
    LvglPort port_;
    UIManager ui_;
    MachineState state_;
    DemoService demo_;
    StatisticsService liveStats_;
    RPMService rpm_{liveStats_};
    PulseInput input_;
    NetworkService network_{input_};
    RpmHistory histories_[2];
    SettingsService storage_;
    DiagnosticsService diagnostics_;
    ModbusService modbus_;
    uint64_t nextRender_=0, nextCalRead_=0, nextNetwork_=0;
    bool displayReady_=false, calibrationStoreOK_=true;
};
}
