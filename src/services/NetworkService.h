#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <Preferences.h>
#include <MachineState.h>
#include "../sensors/PulseInput.h"
#include <atomic>
#include <mbedtls/sha256.h>

namespace tv4 {
class NetworkService {
  public:
    explicit NetworkService(PulseInput &input) : input_(input) {}
    void begin();
    void sync(MachineState &state);
    void openPortal() { requests_.fetch_or(1); }
    void arm() { requests_.fetch_or(2); }
    void close() { requests_.fetch_or(4); }

  private:
    struct Telemetry {
        Measurement motion{};
        Totals totals{};
        uint64_t session = 0, uptime = 0, power = 0;
        uint32_t freeRam = 0;
        bool demo = true, touch = false;
        RpmHistory::View graph{};
    };
    static void task(void *p);
    void run();
    void setupRoutes();
    void setupOTA();
    void updateRadio(uint64_t now);
    void publish();
    void statusJson();
    void upload();
    void finishUpload();
    void stopUpload(const char *message);
    void finishUpdate();
    bool authenticate(bool challenge = true);
    bool localClient();
    bool csrf();
    bool maintenance();
    bool quiet();
    void startPortal();
    void stopPortal();
    void note(const char *msg);
    PulseInput &input_;
    WebServer server_{80};
    Preferences prefs_;
    portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
    Telemetry readout_{};
    NetworkInfo shared_{}, net_{};
    std::atomic<unsigned> requests_{0};
    char ssid_[33]{}, password_[65]{}, token_[33]{}, fullHash_[65]{}, uploadError_[80]{};
    uint64_t portalUntil_ = 0, armedUntil_ = 0, armedEdge_ = 0, radioAt_ = 0, blockedUntil_ = 0,
             failSince_ = 0, rebootAt_ = 0, uploadActivity_ = 0;
    uint64_t retryStationAt_ = 0;
    uint32_t expected_ = 0, received_ = 0;
    unsigned failures_ = 0;
    bool prefsOk_ = false, listening_ = false, otaStarted_ = false, webWriting_ = false,
         hashOpen_ = false, webDone_ = false;
    mbedtls_sha256_context sha_{};
};
} // namespace tv4
