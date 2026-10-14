// netbird-lite example: join a NetBird network and serve a web page over it.
//
// 1. Copy arduino_secrets.h.example to arduino_secrets.h and fill it in (Wi-Fi, NetBird management URL, setup key).
// 2. Upload, open the serial monitor (115200 baud) and wait for "SYNCED".
// 3. From another peer that a NetBird policy allows to reach this one on TCP 80:
//      curl http://<netbird-ip-of-the-esp32>/
#include <NetBirdLite.h>
#include <WebServer.h>
#include <WiFi.h>

#include "arduino_secrets.h"

WebServer server(80); // listens on all interfaces, the NetBird one included

void setup()
{
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    // Wi-Fi modem sleep delays every received packet by up to a beacon interval. Measured on an
    // ESP32-C6 through a NetBird relay: 107 ms ping with power save, 48 ms without.
    WiFi.setSleep(false);
    WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);
    Serial.print("Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
        Serial.print('.');
    }
    Serial.printf(" connected, LAN address %s\n", WiFi.localIP().toString().c_str());

    if (!NetBirdLite.begin(SECRET_NETBIRD_URL, SECRET_NETBIRD_SETUP_KEY, "esp32-arduino")) {
        Serial.printf("netbird-lite did not start: %s\n", esp_err_to_name(NetBirdLite.lastStartError()));
    }

    server.on("/", [] {
        String body = "Hello from " + NetBirdLite.localIP().toString() + " over NetBird\n";
        server.send(200, "text/plain", body);
    });
    server.begin();
}

void loop()
{
    server.handleClient();

    static uint32_t last = 0;
    if (millis() - last >= 10000) {
        last = millis();
        Serial.printf("netbird: %s, address %s, peers %d/%d %s\n", NetBirdLite.stateName(),
                      NetBirdLite.localIP().toString().c_str(), NetBirdLite.peersUp(), NetBirdLite.peers(),
                      NetBirdLite.lastError().c_str());
    }
}
