#include "NetworkService.h"
#include <Maintenance.h>
#include "WebPage.h"
#include "version.h"
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <cctype>
#include <cstring>

namespace tv4 {
namespace {
constexpr uint64_t second = 1000000;
void randomHex(char *dest, unsigned chars) {
    static constexpr char hex[] = "0123456789abcdef";
    for (unsigned i = 0; i < chars; i += 8) {
        uint32_t r = esp_random();
        for (unsigned j = 0; j < 8 && i + j < chars; ++j, r >>= 4)
            dest[i + j] = hex[r & 15];
    }
    dest[chars] = 0;
}
bool hexDigest(const String &value) {
    if (value.length() != 64)
        return false;
    for (unsigned i = 0; i < 64; ++i)
        if (!std::isxdigit(static_cast<unsigned char>(value[i])))
            return false;
    return true;
}
bool sameSubnet(IPAddress peer, IPAddress local, IPAddress mask) {
    return uint32_t(local) && (uint32_t(peer) & uint32_t(mask)) == (uint32_t(local) & uint32_t(mask));
}
const char *sensorName(const Measurement &m, bool demo) {
    if (demo)
        return "Симуляция";
    switch (m.signal) {
    case Signal::Tracking:
        return m.rotating ? "Вращение" : "Ниже порога";
    case Signal::FirstPulse:
        return "Первый импульс";
    case Signal::Silent:
        return "Нет импульсов";
    case Signal::Overrun:
        return "Потеря импульсов";
    default:
        return "Ожидание импульсов";
    }
}
struct Station {
    char ssid[33], password[65];
};
} // namespace
void NetworkService::begin() {
    if (xTaskCreatePinnedToCore(task, "tv4-network", 8192, this, 1, nullptr, 0) != pdPASS) {
        strlcpy(shared_.status, "Нет RAM для Wi-Fi", sizeof(shared_.status));
        Serial.println("[net] task creation failed");
    }
}
void NetworkService::task(void *p) { static_cast<NetworkService *>(p)->run(); }
void NetworkService::sync(MachineState &s) {
    Telemetry t;
    t.motion = s.motion;
    t.totals = s.totals;
    t.session = s.sessionUs;
    t.uptime = s.uptimeUs;
    t.power = s.poweredUs;
    t.freeRam = s.diagnostic.freeHeap;
    t.demo = s.settings.mode == Mode::Demo;
    t.touch = s.diagnostic.touchBus;
    if (s.history)
        t.graph = s.history->view(120, s.uptimeUs);
    portENTER_CRITICAL(&lock_);
    readout_ = t;
    s.network = shared_;
    portEXIT_CRITICAL(&lock_);
}
void NetworkService::publish() {
    portENTER_CRITICAL(&lock_);
    shared_ = net_;
    portEXIT_CRITICAL(&lock_);
}
void NetworkService::note(const char *msg) {
    strlcpy(net_.status, msg, sizeof(net_.status));
    publish();
}
bool NetworkService::quiet() {
    const uint64_t now = esp_timer_get_time(), edge = input_.lastPhysicalEdge();
    bool liveMoving;
    portENTER_CRITICAL(&lock_);
    liveMoving = !readout_.demo && readout_.motion.hasRpm && readout_.motion.rpm > 0;
    portEXIT_CRITICAL(&lock_);
    return inputQuietForUpdate(now, edge, liveMoving);
}
bool NetworkService::maintenance() {
    return net_.armed && !(requests_.load() & 4) && uint64_t(esp_timer_get_time()) < armedUntil_ &&
           input_.lastPhysicalEdge() == armedEdge_ && quiet();
}
void NetworkService::startPortal() {
    WiFi.mode(WIFI_AP_STA);
    if (!WiFi.softAP(net_.apName, net_.apKey)) {
        note("Ошибка точки доступа");
        return;
    }
    net_.portal = true;
    portalUntil_ = esp_timer_get_time() + 600 * second;
    note("Портал на 10 минут");
    radioAt_ = 0;
    Serial.println("[net] local setup AP started");
}
void NetworkService::stopPortal() {
    if (net_.portal)
        WiFi.softAPdisconnect(true);
    net_.portal = false;
    portalUntil_ = 0;
    if (!ssid_[0])
        WiFi.mode(WIFI_OFF);
    radioAt_ = 0;
}
void NetworkService::updateRadio(uint64_t now) {
    if (now < radioAt_)
        return;
    radioAt_ = now + second;
    const bool wasConnected = net_.connected;
    net_.connected = WiFi.status() == WL_CONNECTED;
    if (!net_.connected && ssid_[0] && now >= retryStationAt_ && !net_.updating) {
        retryStationAt_ = now + 30 * second;
        WiFi.begin(ssid_, password_);
    }
    if (net_.connected != wasConnected && !net_.armed && !net_.portal)
        note(net_.connected ? "STA подключена" : "Поиск сети STA");
    net_.rssi = net_.connected ? WiFi.RSSI() : 0;
    strlcpy(net_.ssid, net_.connected ? ssid_ : "", sizeof(net_.ssid));
    const auto ip = net_.portal ? WiFi.softAPIP() : net_.connected ? WiFi.localIP() : IPAddress();
    strlcpy(net_.ip, ip.toString().c_str(), sizeof(net_.ip));
    const bool available = net_.portal || net_.connected;
    if (available && !listening_) {
        server_.begin();
        listening_ = true;
        Serial.printf("[net] HTTP %s\n", net_.ip);
    }
    if (!available && listening_) {
        server_.stop();
        listening_ = false;
    }
    if (net_.portal && now >= portalUntil_ && !net_.updating) {
        stopPortal();
        note("Портал закрыт");
    }
    publish();
}
void NetworkService::run() {
    WiFi.persistent(false);
    // RF entropy is enabled before provisioning per-device credentials.
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    prefsOk_ = prefs_.begin("tv4-net", false);
    const uint64_t id = ESP.getEfuseMac();
    snprintf(net_.apName, sizeof(net_.apName), "TV4-%06lX", (unsigned long)(id & 0xffffff));
    auto key = [&](const char *k, char *dst, unsigned n) {
        if (!prefsOk_ || !prefs_.isKey(k) || prefs_.getBytesLength(k) != n ||
            prefs_.getBytes(k, dst, n) != n || !std::all_of(dst, dst + n, [](char c) {
                return std::isxdigit(static_cast<unsigned char>(c));
            })) {
            randomHex(dst, n);
            if (prefsOk_)
                prefs_.putBytes(k, dst, n);
        }
        dst[n] = 0;
    };
    key("apkey", net_.apKey, 16);
    key("admin", net_.adminKey, 20);
    randomHex(token_, 32);
    Station saved{};
    if (prefsOk_ && prefs_.isKey("station") && prefs_.getBytesLength("station") == sizeof(saved) &&
        prefs_.getBytes("station", &saved, sizeof(saved)) == sizeof(saved)) {
        saved.ssid[32] = 0;
        saved.password[64] = 0;
        strlcpy(ssid_, saved.ssid, sizeof(ssid_));
        strlcpy(password_, saved.password, sizeof(password_));
    }
    setupRoutes();
    setupOTA();
    if (ssid_[0]) {
        retryStationAt_ = esp_timer_get_time() + 30 * second;
        WiFi.begin(ssid_, password_);
        note("Подключение STA");
    } else
        startPortal();
    for (;;) {
        const uint64_t now = esp_timer_get_time();
        const unsigned cmd = requests_.exchange(0);
        if (cmd & 1)
            startPortal();
        if (cmd & 4) {
            armedUntil_ = 0;
            net_.armed = false;
            stopPortal();
            note("Доступ закрыт");
        }
        if (cmd & 2) {
            const auto edge = input_.lastPhysicalEdge();
            if (quiet() && (net_.connected || net_.portal)) {
                armedEdge_ = edge;
                armedUntil_ = now + 300 * second;
                net_.armed = true;
                net_.progress = 0;
                note("OTA разрешено");
            } else
                note("OTA: нет сети или импульсы");
        }
        if (net_.armed && !maintenance()) {
            net_.armed = false;
            note("Окно OTA закрыто");
        }
        if (webWriting_ && (!maintenance() || now - uploadActivity_ > 20 * second))
            stopUpload("Transfer expired");
        updateRadio(now);
        if (listening_)
            server_.handleClient();
        const bool allow = maintenance() && !webWriting_ && !rebootAt_ && now >= blockedUntil_;
        if (allow && !otaStarted_) {
            ArduinoOTA.begin();
            otaStarted_ = true;
        }
        if (!allow && otaStarted_) {
            ArduinoOTA.end();
            otaStarted_ = false;
        }
        if (allow)
            ArduinoOTA.handle();
        if (rebootAt_ && now >= rebootAt_) {
            if (maintenance())
                ESP.restart();
            if (esp_ota_set_boot_partition(esp_ota_get_running_partition()) != ESP_OK)
                Serial.println("[OTA] boot selection restore failed");
            rebootAt_ = 0;
            net_.armed = false;
            note("Перезапуск отменён");
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
bool NetworkService::localClient() {
    const auto addr = server_.client().remoteIP();
    return (net_.portal && sameSubnet(addr, WiFi.softAPIP(), IPAddress(255, 255, 255, 0))) ||
           (net_.connected && sameSubnet(addr, WiFi.localIP(), WiFi.subnetMask()));
}
bool NetworkService::authenticate(bool challenge) {
    const uint64_t now = esp_timer_get_time();
    if (!localClient()) {
        if (challenge)
            server_.send(403, "text/plain", "Local network only");
        return false;
    }
    if (now < blockedUntil_) {
        if (challenge)
            server_.send(429, "text/plain", "Retry in one minute");
        return false;
    }
    if (server_.header("Authorization").startsWith("Digest ") &&
        server_.authenticate("admin", net_.adminKey))
        return true;
    if (server_.hasHeader("Authorization")) {
        if (now - failSince_ > 60 * second) {
            failSince_ = now;
            failures_ = 0;
        }
        if (++failures_ >= 5) {
            blockedUntil_ = now + 60 * second;
            failures_ = 0;
        }
    }
    if (challenge)
        server_.requestAuthentication(DIGEST_AUTH, "TV4");
    return false;
}
bool NetworkService::csrf() { return server_.header("X-TV4-CSRF") == token_; }
void NetworkService::setupRoutes() {
    static const char *hdr[] = {"X-TV4-CSRF", "X-TV4-Size", "X-TV4-SHA256", "Authorization"};
    server_.collectHeaders(hdr, 4);
    server_.on("/", HTTP_GET, [this] {
        if (!localClient()) {
            server_.send(403);
            return;
        }
        server_.sendHeader("Cache-Control", "no-store");
        server_.sendHeader("X-Frame-Options", "DENY");
        server_.sendHeader("X-Content-Type-Options", "nosniff");
        server_.send_P(200, "text/html; charset=utf-8", dashboardHtml);
    });
    server_.on("/api/status", HTTP_GET, [this] {
        if (localClient())
            statusJson();
        else
            server_.send(403);
    });
    server_.on("/admin/session", HTTP_GET, [this] {
        if (!authenticate())
            return;
        server_.sendHeader("Cache-Control", "no-store");
        char json[65];
        snprintf(json, sizeof(json), "{\"csrf\":\"%s\"}", token_);
        server_.send(200, "application/json", json);
    });
    server_.on("/admin/wifi", HTTP_POST, [this] {
        if (!authenticate())
            return;
        if (!net_.portal || !csrf()) {
            server_.send(403);
            return;
        }
        auto s = server_.arg("ssid"), p = server_.arg("password");
        if (!s.length() || s.length() > 32 || p.length() > 63 || (p.length() > 0 && p.length() < 8) ||
            strlen(s.c_str()) != s.length() || strlen(p.c_str()) != p.length()) {
            server_.send(400, "text/plain", "Invalid SSID or password");
            return;
        }
        Station next{};
        strlcpy(next.ssid, s.c_str(), sizeof(next.ssid));
        strlcpy(next.password, p.c_str(), sizeof(next.password));
        if (!prefsOk_ || prefs_.putBytes("station", &next, sizeof(next)) != sizeof(next)) {
            server_.send(500, "text/plain", "NVS error");
            return;
        }
        strlcpy(ssid_, next.ssid, sizeof(ssid_));
        strlcpy(password_, next.password, sizeof(password_));
        server_.send(200, "text/plain; charset=utf-8",
                     "Сеть сохранена. Адрес STA появится на панели после закрытия точки доступа.");
        WiFi.begin(ssid_, password_);
        note("Подключение STA");
    });
    server_.on("/admin/update", HTTP_POST, [this] { finishUpload(); }, [this] { upload(); });
    server_.onNotFound([this] { server_.send(404, "text/plain", "Not found"); });
}
void NetworkService::statusJson() {
    Telemetry t;
    portENTER_CRITICAL(&lock_);
    t = readout_;
    portEXIT_CRITICAL(&lock_);
    char rpm[20] = "null", peak[20] = "null", avg[20] = "null";
    if (t.motion.hasRpm)
        snprintf(rpm, sizeof(rpm), "%.1f", t.motion.rpm);
    if (t.totals.runningUs) {
        snprintf(peak, sizeof(peak), "%u", unsigned(t.totals.peakRpm));
        snprintf(avg, sizeof(avg), "%.1f", t.totals.average());
    }
    char json[1200];
    int n = snprintf(
        json, sizeof(json),
        "{\"version\":\"%s\",\"mode\":\"%s\",\"rpm\":%s,\"signal\":\"%s\",\"max\":%s,\"avg\":%s,"
        "\"starts\":%llu,\"running\":%llu,\"operation\":%llu,\"power\":%llu,\"uptime\":%llu,\"heap\":%u,"
        "\"network\":\"%s %s\",\"rssi\":%d,\"pulses\":%llu,\"lastEdgeUs\":%llu,\"graph\":[",
        TV4_VERSION, t.demo ? "DEMO" : "LIVE", rpm, sensorName(t.motion, t.demo), peak, avg,
        t.totals.starts, t.totals.runningUs / second, t.session / second, t.power / second,
        t.uptime / second, t.freeRam, net_.portal ? "AP" : "STA", net_.ip, net_.rssi, t.motion.pulses,
        t.motion.lastEdgeUs);
    for (unsigned i = 0; i < t.graph.width; ++i) {
        if (t.graph.points[i] < 0)
            n += snprintf(json + n, sizeof(json) - n, "%snull", i ? "," : "");
        else
            n += snprintf(json + n, sizeof(json) - n, "%s%d", i ? "," : "", t.graph.points[i]);
    }
    strlcpy(json + n, "]}", sizeof(json) - n);
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "application/json", json);
}
void NetworkService::stopUpload(const char *msg) {
    Serial.printf("[OTA] %s (%u bytes)\n", msg, unsigned(received_));
    if (webWriting_)
        Update.abort();
    if (hashOpen_) {
        mbedtls_sha256_free(&sha_);
        hashOpen_ = false;
    }
    webDone_ = webWriting_ = false;
    net_.updating = false;
    strlcpy(uploadError_, msg, sizeof(uploadError_));
    note(msg);
}
void NetworkService::upload() {
    auto &u = server_.upload();
    uploadActivity_ = esp_timer_get_time();
    if (u.status == UPLOAD_FILE_START) {
        if (webWriting_ || webDone_) {
            stopUpload("One file per request");
            return;
        }
        uploadError_[0] = 0;
        if (!authenticate(false) || !csrf() || !maintenance() || rebootAt_) {
            stopUpload("Access or maintenance denied");
            return;
        }
        const auto hash = server_.header("X-TV4-SHA256"), size = server_.header("X-TV4-Size");
        char *end = nullptr;
        const auto bytes = strtoul(size.c_str(), &end, 10);
        auto *slot = esp_ota_get_next_update_partition(nullptr);
        if (!slot || !bytes || !end || *end || bytes > slot->size || !hexDigest(hash)) {
            stopUpload("Invalid size or SHA-256");
            return;
        }
        if (!Update.begin(bytes, U_FLASH)) {
            stopUpload("Cannot open OTA slot");
            return;
        }
        expected_ = bytes;
        received_ = 0;
        strlcpy(fullHash_, hash.c_str(), sizeof(fullHash_));
        mbedtls_sha256_init(&sha_);
        mbedtls_sha256_starts_ret(&sha_, 0);
        hashOpen_ = true;
        webWriting_ = net_.updating = true;
        Serial.printf("[OTA] receiving %u bytes\n", unsigned(expected_));
        net_.progress = 0;
        note("Запись OTA");
        return;
    }
    if (!webWriting_)
        return;
    if (!maintenance() || u.status == UPLOAD_FILE_ABORTED) {
        stopUpload("Transfer cancelled");
        return;
    }
    if (u.status == UPLOAD_FILE_WRITE) {
        if (!u.currentSize)
            return;
        if (u.currentSize > expected_ - received_ || (!received_ && u.buf[0] != 0xe9) ||
            Update.write(u.buf, u.currentSize) != u.currentSize) {
            stopUpload("Invalid image or write error");
            return;
        }
        mbedtls_sha256_update_ret(&sha_, u.buf, u.currentSize);
        received_ += u.currentSize;
        net_.progress = uint64_t(received_) * 100 / expected_;
        publish();
    } else if (u.status == UPLOAD_FILE_END) {
        uint8_t raw[32];
        char digest[65];
        mbedtls_sha256_finish_ret(&sha_, raw);
        for (unsigned i = 0; i < 32; ++i)
            snprintf(digest + 2 * i, 3, "%02x", raw[i]);
        if (received_ != expected_ || strcasecmp(digest, fullHash_)) {
            stopUpload("Size or SHA-256 mismatch");
            return;
        }
        mbedtls_sha256_free(&sha_);
        hashOpen_ = false;
        webDone_ = true;
    }
}
void NetworkService::finishUpload() {
    // Commit only after the complete authenticated request, including its final multipart boundary.
    const bool verified = webDone_ && webWriting_ && maintenance() && csrf();
    webDone_ = false;
    if (!verified || !Update.end()) {
        if (!uploadError_[0])
            stopUpload("Incomplete or invalid update");
        server_.send(400, "text/plain", uploadError_);
        return;
    }
    webWriting_ = false;
    finishUpdate();
    server_.send(200, "text/plain; charset=utf-8", "SHA-256 проверен. Перезапуск панели.");
}
void NetworkService::finishUpdate() {
    net_.updating = false;
    net_.progress = 100;
    if (!maintenance()) {
        esp_ota_set_boot_partition(esp_ota_get_running_partition());
        note("OTA отменено");
        return;
    }
    rebootAt_ = esp_timer_get_time() + second;
    note("Обновлено");
    Serial.println("[OTA] image verified; restart pending");
}
void NetworkService::setupOTA() {
    ArduinoOTA.setHostname(net_.apName)
        .setPassword(net_.adminKey)
        .setRebootOnSuccess(false)
        .setMdnsEnabled(false);
    ArduinoOTA.setTimeout(1500);
    ArduinoOTA.onStart([this] {
        if (ArduinoOTA.getCommand() != U_FLASH || !maintenance()) {
            Update.abort();
            return;
        }
        net_.updating = true;
        net_.progress = 0;
        note("PlatformIO OTA");
    });
    ArduinoOTA.onProgress([this](unsigned done, unsigned total) {
        if (!maintenance()) {
            Update.abort();
            return;
        }
        net_.progress = total ? uint64_t(done) * 100 / total : 0;
        publish();
    });
    ArduinoOTA.onError([this](ota_error_t e) {
        if (Update.isRunning())
            Update.abort();
        net_.updating = false;
        if (e == OTA_AUTH_ERROR && ++failures_ >= 5) {
            blockedUntil_ = esp_timer_get_time() + 60 * second;
            failures_ = 0;
        }
        note("Ошибка PlatformIO OTA");
    });
    ArduinoOTA.onEnd([this] { finishUpdate(); });
}
} // namespace tv4
