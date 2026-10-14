// SPDX-License-Identifier: Apache-2.0
// netbird-lite for Arduino: puts an ESP32 into a NetBird network as a peer.
//
//   #include <WiFi.h>
//   #include <NetBirdLite.h>
//
//   void setup() {
//     WiFi.begin("ssid", "password");
//     while (WiFi.status() != WL_CONNECTED) delay(200);
//     NetBirdLite.begin("https://netbird.example.com", "SETUP-KEY", "esp32-demo");
//   }
//
// Once connected, the device has its NetBird IP on an lwIP interface of its own: WebServer,
// WiFiServer, WiFiClient, HTTPClient, MQTT clients and so on work over the tunnel unchanged.
// The plain C API of the ESP-IDF component (netbird_lite.h) stays available as well.
#pragma once

#include <Arduino.h>
#include <IPAddress.h>

#include "netbird_lite.h"

#if !defined(ESP_PLATFORM)
#error "netbird-lite runs on the Arduino ESP32 core (arduino-esp32 3.2 or newer) only"
#endif

class NetBirdLiteClass {
  public:
    /** Starts the client and returns at once; the connection is set up in the background.
     *  Call it after Wi-Fi or Ethernet is connected. managementUrl is required, e.g.
     *  "https://netbird.example.com" or "https://netbird.example.com:33073". The setup key is only
     *  needed until the device is registered (its identity is stored in NVS); pass nullptr after
     *  that. hostname is the peer name in the dashboard, nullptr for "esp32-xxxxxx".
     *  Returns false on bad arguments or when the client was started before (see lastStartError()).
     *  The strings are copied, temporaries are fine. */
    bool begin(const char *managementUrl, const char *setupKey = nullptr, const char *hostname = nullptr);

    /** Stops the background tasks. The client cannot be started again without a reboot. */
    void end();

    /** true while the device is in the NetBird network (network map received). */
    bool connected() const;
    nbl_state_t state() const;
    /** "IDLE", "WAIT_TIME", "CONNECTING", "LOGIN", "SYNCED", "RETRY_WAIT" or "FATAL". */
    const char *stateName() const;

    /** NetBird address of this device, 0.0.0.0 until the first network map. */
    IPAddress localIP() const;
    /** Prefix length of the NetBird network (e.g. 16), 0 until the first network map. */
    uint8_t prefixLength() const;
    /** WireGuard public key (base64), as shown in the NetBird dashboard. */
    String publicKey() const;
    /** Last error, empty if none. Check it when state() is NBL_FATAL or NBL_RETRY_WAIT. */
    String lastError() const;

    int peers() const;   /**< remote peers in the network map */
    int peersUp() const; /**< remote peers with an active WireGuard session */
    bool signalConnected() const;
    bool relayConnected() const;

    /** All status fields and counters at once (thread-safe snapshot). */
    nbl_status_t status() const;
    /** Result of the last begin() call. */
    esp_err_t lastStartError() const { return _startErr; }

    /** Erases the stored identity; the next start registers a new peer and needs a setup key.
     *  Call it before begin(). */
    void forget();

  private:
    esp_err_t _startErr = ESP_OK;
};

extern NetBirdLiteClass NetBirdLite;
