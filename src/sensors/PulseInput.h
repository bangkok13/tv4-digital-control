#pragma once
#include <Arduino.h>
#include <RPMService.h>

namespace tv4 {
class PulseInput {
public:
    void setEnabled(bool enabled);
    void drain(RPMService& rpm);
private:
    static void IRAM_ATTR edge(void* arg);
    static constexpr unsigned capacity=128;
    portMUX_TYPE lock_=portMUX_INITIALIZER_UNLOCKED;
    uint64_t times_[capacity]{};
    volatile unsigned read_=0, write_=0;
    volatile uint32_t dropped_=0;
    bool enabled_=false;
};
}
