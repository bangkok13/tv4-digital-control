#include "PulseInput.h"
#include "tv4_board.h"
#include <esp_timer.h>

namespace tv4 {
void IRAM_ATTR PulseInput::edge(void* arg) {
    auto& in=*static_cast<PulseInput*>(arg);
    const auto time=esp_timer_get_time();
    portENTER_CRITICAL_ISR(&in.lock_);
    const unsigned next=(in.write_+1)%capacity;
    if (next==in.read_) {
        if (in.dropped_!=UINT32_MAX) ++in.dropped_;
    } else {
        in.times_[in.write_]=time;
        in.write_=next;
    }
    portEXIT_CRITICAL_ISR(&in.lock_);
}
void PulseInput::setEnabled(bool enabled) {
    if (enabled_) detachInterrupt(board::rpmInput);
    portENTER_CRITICAL(&lock_);
    read_=write_=0; dropped_=0;
    portEXIT_CRITICAL(&lock_);
    enabled_=enabled;
    if (enabled) {
        pinMode(board::rpmInput,INPUT_PULLUP);
        attachInterruptArg(board::rpmInput,edge,this,FALLING);
    }
}
void PulseInput::drain(RPMService& rpm) {
    for (unsigned batch=0;batch<4;++batch) {
        uint64_t times[32], lost=0;
        unsigned count=0;
        portENTER_CRITICAL(&lock_);
        if (dropped_) {
            lost=uint64_t(dropped_)+(write_+capacity-read_)%capacity;
            read_=write_; dropped_=0;
        } else {
            while (read_!=write_ && count<32) {
                times[count++]=times_[read_];
                read_=(read_+1)%capacity;
            }
        }
        portEXIT_CRITICAL(&lock_);
        if (lost) rpm.overrun(lost);
        for (unsigned i=0;i<count;++i) rpm.pulse(times[i]);
        if (count<32) break;
    }
}
}
