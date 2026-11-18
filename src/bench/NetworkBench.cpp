// Hardware integration tests on an isolated bench. The release excludes the USB test commands.
#ifdef TV4_NETWORK_BENCH
#include "../services/NetworkService.h"
#include <HTTPClient.h>
#include <MD5Builder.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_ota_ops.h>
namespace tv4 {
namespace {
std::atomic<bool> busy{false};
bool useArduinoProtocol = false;
String md5(const String &text) {
    MD5Builder h;
    h.begin();
    h.add(text);
    h.calculate();
    return h.toString();
}
String quoted(const String &source, const char *key) {
    const String tag = String(key) + "=\"";
    int p = source.indexOf(tag);
    if (p < 0)
        return {};
    p += tag.length();
    const int end = source.indexOf('"', p);
    return end < 0 ? String() : source.substring(p, end);
}
struct Login {
    String nonce, realm, opaque, secret;
    unsigned sequence = 0;
    String header(const char *method, const char *path) {
        char nc[9], cn[9];
        snprintf(nc, sizeof(nc), "%08x", ++sequence);
        snprintf(cn, sizeof(cn), "%08x", esp_random());
        auto a = md5(String("admin:") + realm + ":" + secret), b = md5(String(method) + ":" + path);
        auto answer = md5(a + ":" + nonce + ":" + nc + ":" + cn + ":auth:" + b);
        return String("Digest username=\"admin\", realm=\"") + realm + "\", nonce=\"" + nonce +
               "\", uri=\"" + path + "\", qop=auth, nc=" + nc + ", cnonce=\"" + cn + "\", response=\"" +
               answer + "\", opaque=\"" + opaque + "\"";
    }
};
bool perform(NetworkService &service) {
    service.openPortal();
    delay(1200);
    auto addr = WiFi.softAPIP();
    String base = "http://" + addr.toString();
    HTTPClient http;
    http.setReuse(false);
    http.setTimeout(6000);
    http.begin(base + "/api/status");
    int status = http.GET();
    bool data = status == 200 && http.getString().indexOf("\"graph\"") >= 0;
    http.end();
    Serial.printf("[bench] HTTP telemetry %d valid=%d\n", status, data);
    if (!data)
        return false;
    Login auth;
    Preferences keys;
    if (!keys.begin("tv4-net", true))
        return false;
    char secret[21]{};
    auto n = keys.getBytes("admin", secret, 20);
    keys.end();
    if (n != 20)
        return false;
    auth.secret = secret;
    const char *collect[] = {"WWW-Authenticate"};
    http.begin(base + "/admin/session");
    http.collectHeaders(collect, 1);
    int challenge = http.GET();
    String h = http.header("WWW-Authenticate");
    http.end();
    if (challenge != 401)
        return false;
    auth.realm = quoted(h, "realm");
    auth.nonce = quoted(h, "nonce");
    auth.opaque = quoted(h, "opaque");
    http.begin(base + "/admin/session");
    http.addHeader("Authorization", auth.header("GET", "/admin/session"));
    int login = http.GET();
    String reply = http.getString();
    http.end();
    int pos = reply.indexOf("\"csrf\":\"");
    if (login != 200 || pos < 0) {
        Serial.printf("[bench] login HTTP %d\n", login);
        return false;
    }
    String csrf = reply.substring(pos + 8, pos + 40);
    auto *src = esp_ota_get_running_partition();
    size_t size = ESP.getSketchSize();
    uint8_t buf[1024], digest[32];
    char hex[65];
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts_ret(&sha, 0);
    for (size_t off = 0; off < size; off += sizeof(buf)) {
        auto len = std::min(sizeof(buf), size - off);
        if (esp_partition_read(src, off, buf, len) != ESP_OK)
            return false;
        mbedtls_sha256_update_ret(&sha, buf, len);
        delay(1);
    }
    mbedtls_sha256_finish_ret(&sha, digest);
    mbedtls_sha256_free(&sha);
    for (unsigned i = 0; i < 32; ++i)
        snprintf(hex + 2 * i, 3, "%02x", digest[i]);
    service.arm();
    delay(500);
    WiFiClient client;
    client.setTimeout(20);
    if (!client.connect(addr, 80, 5000))
        return false;
    // The sketch itself is the uploaded file. A fixed literal separator would also occur in its .rodata.
    char boundary[41];
    snprintf(boundary, sizeof(boundary), "TV4-%08x%08x%08x%08x", esp_random(), esp_random(),
             esp_random(), esp_random());
    String start = String("--") + boundary +
                   "\r\nContent-Disposition: form-data; name=\"firmware\"; "
                   "filename=\"bench.bin\"\r\nContent-Type: application/octet-stream\r\n\r\n";
    String end = String("\r\n--") + boundary + "--\r\n";
    client.printf("POST /admin/update HTTP/1.1\r\nHost: %s\r\nAuthorization: %s\r\nX-TV4-CSRF: "
                  "%s\r\nX-TV4-Size: %u\r\nX-TV4-SHA256: %s\r\nContent-Type: multipart/form-data; "
                  "boundary=%s\r\nContent-Length: %u\r\nConnection: close\r\n\r\n",
                  addr.toString().c_str(), auth.header("POST", "/admin/update").c_str(), csrf.c_str(),
                  unsigned(size), hex, boundary, unsigned(start.length() + size + end.length()));
    client.print(start);
    for (size_t off = 0; off < size; off += sizeof(buf)) {
        auto len = std::min(sizeof(buf), size - off);
        if (esp_partition_read(src, off, buf, len) != ESP_OK)
            return false;
        size_t sent = 0;
        while (sent < len && client.connected()) {
            const auto count = client.write(buf + sent, len - sent);
            if (!count)
                break;
            sent += count;
        }
        if (sent != len) {
            Serial.printf("[bench] transfer stopped at %u, reply %.60s\n", unsigned(off + sent),
                          client.readStringUntil('\n').c_str());
            return false;
        }
        if (off % (256 * 1024) == 0)
            Serial.printf("[bench] OTA %u / %u\n", unsigned(off), unsigned(size));
        delay(1);
    }
    client.print(end);
    const String line = client.readStringUntil('\n');
    client.stop();
    Serial.printf("[bench] Web OTA %.48s from %s\n", line.c_str(), src->label);
    return line.indexOf(" 200 ") >= 0;
}
String udpReply(WiFiUDP &udp) {
    const auto start = millis();
    while (!udp.parsePacket()) {
        if (millis() - start > 4000)
            return {};
        delay(2);
    }
    char reply[82]{};
    int n = udp.read(reinterpret_cast<uint8_t *>(reply), sizeof(reply) - 1);
    return n > 0 ? String(reply) : String();
}
bool platformTransfer(NetworkService &service) {
    service.openPortal();
    delay(1000);
    service.arm();
    delay(600);
    const auto addr = WiFi.softAPIP();
    const auto *source = esp_ota_get_running_partition();
    WiFiServer sender(3233);
    sender.begin();
    WiFiUDP control;
    if (!control.begin(3234))
        return false;
    const unsigned size = ESP.getSketchSize();
    const auto sketchHash = ESP.getSketchMD5();
    if (!control.beginPacket(addr, 3232))
        return false;
    control.printf("0 3233 %u %s\n", size, sketchHash.c_str());
    control.endPacket();
    const String first = udpReply(control);
    if (!first.startsWith("AUTH "))
        return false;
    String nonce = first.substring(5);
    nonce.trim();
    char key[21]{};
    Preferences prefs;
    if (!prefs.begin("tv4-net", true))
        return false;
    const auto bytes = prefs.getBytes("admin", key, 20);
    prefs.end();
    if (bytes != 20)
        return false;
    const String clientNonce = md5(String(esp_random(), HEX));
    const auto response = md5(md5(key) + ":" + nonce + ":" + clientNonce);
    control.beginPacket(addr, 3232);
    control.printf("200 %s %s\n", clientNonce.c_str(), response.c_str());
    control.endPacket();
    if (!udpReply(control).startsWith("OK"))
        return false;
    WiFiClient link;
    const auto waiting = millis();
    while (!(link = sender.available())) {
        if (millis() - waiting > 5000)
            return false;
        delay(2);
    }
    uint8_t buf[1024];
    String tail;
    for (unsigned off = 0; off < size; off += sizeof(buf)) {
        const size_t length = std::min<size_t>(sizeof(buf), size - off);
        if (esp_partition_read(source, off, buf, length) != ESP_OK || link.write(buf, length) != length)
            return false;
        const auto sent = millis();
        while (!link.available()) {
            if (millis() - sent > 6000)
                return false;
            delay(1);
        }
        tail = "";
        while (link.available() && tail.length() < 80)
            tail += char(link.read());
        if (off % (256 * 1024) == 0)
            Serial.printf("[bench] espota %u / %u\n", off, size);
    }
    const auto finish = millis();
    while (tail.indexOf("OK") < 0 && millis() - finish < 4000) {
        if (link.available())
            tail += char(link.read());
        else
            delay(1);
    }
    const bool passed = tail.indexOf("OK") >= 0;
    link.stop();
    sender.end();
    control.stop();
    Serial.printf("[bench] espota result=%u from %s\n", unsigned(passed), source->label);
    return passed;
}
void execute(void *arg) {
    auto &service = *static_cast<NetworkService *>(arg);
    bool ok = useArduinoProtocol ? platformTransfer(service) : perform(service);
    Serial.printf("[bench] %s\n", ok ? "PASS" : "FAIL");
    busy = false;
    vTaskDelete(nullptr);
}
} // namespace
void startNetworkBench(NetworkService &service, bool platform) {
    if (busy.exchange(true))
        return;
    useArduinoProtocol = platform;
    if (xTaskCreatePinnedToCore(execute, "tv4-bench", 8192, &service, 1, nullptr, 1) != pdPASS)
        busy = false;
}
} // namespace tv4
#endif
