// netbird-lite example: the ESP32 calls an HTTP service on another NetBird peer.
//
// The NetBird policy has to allow this device to reach the peer (for example a policy from the
// ESP32's group to the peer's group on TCP 8080). Replies come back through netbird-lite's
// connection tracking; no policy in the other direction is needed.
//
// To try it, start a web server on the other peer:  python3 -m http.server 8080
#include <HTTPClient.h>
#include <NetBirdLite.h>
#include <WiFi.h>

#include "arduino_secrets.h"

// NetBird address of the other peer, as shown in the NetBird dashboard
static const char *PEER_URL = "http://100.92.0.10:8080/";

void setup()
{
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // lower latency, see the Basic example
    WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
    }
    if (!NetBirdLite.begin(SECRET_NETBIRD_URL, SECRET_NETBIRD_SETUP_KEY, "esp32-client")) {
        Serial.printf("netbird-lite did not start: %s\n", esp_err_to_name(NetBirdLite.lastStartError()));
    }
}

void loop()
{
    // the peer is reachable once the network map is there and its WireGuard session is up
    if (NetBirdLite.connected() && NetBirdLite.peersUp() > 0) {
        HTTPClient http;
        http.setConnectTimeout(5000);
        http.begin(PEER_URL);
        int code = http.GET();
        if (code > 0) {
            Serial.printf("GET %s -> %d, %d bytes\n", PEER_URL, code, http.getSize());
        } else {
            Serial.printf("GET %s failed: %s\n", PEER_URL, http.errorToString(code).c_str());
        }
        http.end();
    } else {
        Serial.printf("waiting for NetBird: %s %s\n", NetBirdLite.stateName(), NetBirdLite.lastError().c_str());
    }
    delay(30000);
}
