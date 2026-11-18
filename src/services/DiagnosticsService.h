#pragma once
#include <MachineState.h>
#include "../touch/TouchManager.h"

namespace tv4 {
class DiagnosticsService {
public:
    void begin(Diagnostics& d);
    void tick(MachineState& state,const TouchManager& touch,bool storageOK);
private:
    uint64_t nextSample_=0, nextLog_=0;
};
}
