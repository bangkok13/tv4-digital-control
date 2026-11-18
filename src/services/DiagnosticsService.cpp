#include "DiagnosticsService.h"
#include <esp_system.h>
#include <esp_ota_ops.h>

namespace tv4 {
namespace {
const char* reason(esp_reset_reason_t r) {
    switch (r) {
    case ESP_RST_POWERON: return "Питание";
    case ESP_RST_EXT: return "Внешний";
    case ESP_RST_SW: return "Software";
    case ESP_RST_PANIC: return "Panic";
    case ESP_RST_INT_WDT: return "IRQ WDT";
    case ESP_RST_TASK_WDT: return "Task WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "Deep sleep";
    case ESP_RST_BROWNOUT: return "Brownout";
    case ESP_RST_SDIO: return "SDIO";
    default: return "Н/Д";
    }
}
}
void DiagnosticsService::begin(Diagnostics& d) {
    snprintf(d.chip,sizeof(d.chip),"%s",ESP.getChipModel());
    snprintf(d.resetReason,sizeof(d.resetReason),"%s",reason(esp_reset_reason()));
    d.revision=ESP.getChipRevision(); d.flashBytes=ESP.getFlashChipSize();
    Serial.printf("[boot] TV-4 %s, %s rev %u, flash %u, reset %s\n",
        board::version,d.chip,d.revision,d.flashBytes,d.resetReason);
    if (const auto* slot=esp_ota_get_running_partition())
        Serial.printf("[boot] slot=%s offset=0x%x bytes=%u\n",slot->label,unsigned(slot->address),unsigned(slot->size));
}
void DiagnosticsService::tick(MachineState& s,const TouchManager& touch,bool storageOK) {
    if (s.uptimeUs<nextSample_) return;
    nextSample_=s.uptimeUs+1000000;
    auto& d=s.diagnostic;
    d.freeHeap=ESP.getFreeHeap(); d.minHeap=ESP.getMinFreeHeap();
    d.touchBus=touch.busReady(); d.touchSeen=touch.seen(); d.calibrated=touch.calibrated();
    d.storageOK=storageOK;
    if (s.uptimeUs<nextLog_) return;
    nextLog_=s.uptimeUs+10000000;
    Serial.printf("[%s] heap=%u min=%u signal=%u pulses=%llu reject=%llu lost=%llu NVS=%s cal=%u\n",
        s.settings.mode==Mode::Demo ? "DEMO" : "LIVE",d.freeHeap,d.minHeap,
        unsigned(s.motion.signal),s.motion.pulses,s.motion.rejected,s.motion.lost,storageOK ? "ok" : "error",unsigned(d.calibrated));
}
}
