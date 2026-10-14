// netbird-lite test firmware: the endpoints the end-to-end tests of netbird-esp32 expect
// (tools/e2e_test.sh and tools/soak.sh there), so that the Arduino build is tested exactly like
// the ESP-IDF build.
//
//   GET  /            greeting with the NetBird address
//   GET  /status      JSON with the client status, counters, heap and stack reserves
//   POST /echo        echoes the request body (send it as text/plain, up to 1 KB)
//   GET  /blob?n=N    N bytes of the pattern i % 251, for throughput and integrity tests
//   GET  /fetch?ip=A.B.C.D&port=N
//                     the device itself opens a TCP connection, sends "GET / HTTP/1.0" and
//                     reports the status line (replies to outgoing connections must pass)
//   TCP  8081         echo server that the test policy deliberately does NOT allow
#include <NetBirdLite.h>
#include <WebServer.h>
#include <WiFi.h>

#include "arduino_secrets.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

WebServer server(80);
WiFiServer echoServer(8081);

static unsigned stackReserve(const char *taskName)
{
    TaskHandle_t t = xTaskGetHandle(taskName);
    return t ? (unsigned)uxTaskGetStackHighWaterMark(t) : 0;
}

static void handleRoot()
{
    char buf[160];
    snprintf(buf, sizeof(buf), "hello from ESP32 over NetBird (%s), free heap: %u bytes (Arduino)\n",
             NetBirdLite.status().ip, (unsigned)esp_get_free_heap_size());
    server.send(200, "text/plain", buf);
}

static void handleStatus()
{
    nbl_status_t st = NetBirdLite.status();
    char buf[640];
    snprintf(buf, sizeof(buf),
             "{\"state\":%d,\"ip\":\"%s\",\"pubkey\":\"%s\",\"signal\":%d,\"relay\":%d,\"peers\":%d,\"peers_up\":%d,"
             "\"rules\":%d,\"relay_rx\":%u,\"relay_tx\":%u,\"relay_dropped\":%u,\"relay_reconnects\":%u,"
             "\"acl_pass\":%u,\"acl_drop\":%u,\"heap\":%u,\"heap_min\":%u,\"heap_largest\":%u,"
             "\"stack_ctrl\":%u,\"stack_relay\":%u,\"stack_tcpip\":%u,\"stack_loop\":%u}\n",
             st.state, st.ip, st.pubkey, st.signal_up, st.relay_up, st.peers, st.peers_up, st.acl_rules,
             (unsigned)st.relay_rx, (unsigned)st.relay_tx, (unsigned)st.relay_dropped, (unsigned)st.relay_reconnects,
             (unsigned)st.acl_pass, (unsigned)st.acl_drop, (unsigned)esp_get_free_heap_size(),
             (unsigned)esp_get_minimum_free_heap_size(), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
             stackReserve("nbl_ctrl"), stackReserve("nbl_relay"), stackReserve("tiT"), stackReserve("loopTask"));
    server.send(200, "application/json", buf);
}

static void handleEcho()
{
    // WebServer keeps a text/plain body in the "plain" argument
    String body = server.arg("plain");
    if (body.length() > 1024) {
        server.send(400, "text/plain", "body too large (max 1024 bytes)\n");
        return;
    }
    server.send(200, "text/plain", body);
}

static void handleBlob()
{
    long n = server.arg("n").toInt();
    if (n < 0 || n > 4L * 1024 * 1024) {
        n = 0;
    }
    server.setContentLength((size_t)n);
    server.send(200, "application/octet-stream", "");
    static char chunk[1024];
    for (long off = 0; off < n;) {
        size_t m = (size_t)(n - off) < sizeof(chunk) ? (size_t)(n - off) : sizeof(chunk);
        for (size_t i = 0; i < m; i++) {
            chunk[i] = (char)((off + (long)i) % 251);
        }
        server.sendContent(chunk, m);
        if (!server.client().connected()) {
            return;
        }
        off += (long)m;
    }
}

static void handleFetch()
{
    IPAddress ip;
    long port = server.arg("port").toInt();
    if (!ip.fromString(server.arg("ip")) || port < 1 || port > 65535) {
        server.send(400, "text/plain", "usage: /fetch?ip=A.B.C.D&port=N\n");
        return;
    }
    WiFiClient c;
    char res[160];
    if (!c.connect(ip, (uint16_t)port, 5000)) {
        snprintf(res, sizeof(res), "{\"ok\":false,\"error\":\"connect failed\"}\n");
    } else {
        c.setTimeout(5000);
        c.print("GET / HTTP/1.0\r\n\r\n");
        String line = c.readStringUntil('\n');
        line.trim();
        size_t total = line.length();
        while (c.connected() || c.available()) {
            if (c.available()) {
                c.read();
                total++;
            } else {
                delay(1);
            }
        }
        if (line.length()) {
            snprintf(res, sizeof(res), "{\"ok\":true,\"bytes\":%u,\"status\":\"%.60s\"}\n", (unsigned)total, line.c_str());
        } else {
            snprintf(res, sizeof(res), "{\"ok\":false,\"error\":\"no response\"}\n");
        }
    }
    c.stop();
    server.send(200, "application/json", res);
}

static void serveEcho8081()
{
    WiFiClient c = echoServer.accept();
    if (!c) {
        return;
    }
    uint32_t t0 = millis();
    while (c.connected() && millis() - t0 < 5000) {
        while (c.available()) {
            c.write((uint8_t)c.read());
            t0 = millis();
        }
        delay(1);
    }
    c.stop();
}

void setup()
{
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
    }
    Serial.printf("Wi-Fi connected, LAN address %s, free heap before netbird-lite: %u bytes\n",
                  WiFi.localIP().toString().c_str(), (unsigned)esp_get_free_heap_size());

    server.on("/", HTTP_GET, handleRoot);
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/echo", HTTP_POST, handleEcho);
    server.on("/blob", HTTP_GET, handleBlob);
    server.on("/fetch", HTTP_GET, handleFetch);
    server.begin();
    echoServer.begin();

    if (!NetBirdLite.begin(SECRET_NETBIRD_URL, SECRET_NETBIRD_SETUP_KEY, SECRET_NETBIRD_HOSTNAME)) {
        Serial.printf("netbird-lite did not start: %s\n", esp_err_to_name(NetBirdLite.lastStartError()));
    }
}

void loop()
{
    server.handleClient();
    serveEcho8081();

    static uint32_t last = 0;
    if (millis() - last >= 10000) {
        last = millis();
        nbl_status_t st = NetBirdLite.status();
        Serial.printf("state=%s ip=%s signal=%d relay=%d peers=%d/%d acl=%u/%u heap=%u min=%u err='%s'\n",
                      NetBirdLite.stateName(), st.ip, st.signal_up, st.relay_up, st.peers_up, st.peers,
                      (unsigned)st.acl_pass, (unsigned)st.acl_drop, (unsigned)esp_get_free_heap_size(),
                      (unsigned)esp_get_minimum_free_heap_size(), st.last_error);
    }
}
