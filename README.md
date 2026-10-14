# netbird-lite for Arduino

**A NetBird client for ESP32 boards in the Arduino IDE.**

[![CI](https://github.com/insight-it-gmbh/netbird-esp32-arduino/actions/workflows/ci.yml/badge.svg)](https://github.com/insight-it-gmbh/netbird-esp32-arduino/actions/workflows/ci.yml)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)
[![arduino-esp32 3.2+](https://img.shields.io/badge/arduino--esp32-3.2%2B-00979D.svg)](https://github.com/espressif/arduino-esp32)
[![Made by Insight-IT GmbH](https://img.shields.io/badge/Made%20by-Insight--IT%20GmbH-9DCD5A.svg)](https://www.insight-it.de)

This library puts an ESP32 into a [NetBird](https://netbird.io) network as a regular peer.
Other peers reach it under its NetBird IP, NetBird access policies decide who may talk to it,
and it needs no port forwarding and no public IP. `WebServer`, `WiFiServer`, `WiFiClient`,
`HTTPClient` and MQTT clients work over the tunnel without changes.

It is the Arduino packaging of [netbird-lite](https://github.com/insight-it-gmbh/netbird-esp32),
the ESP-IDF component: the same code, generated from it, plus a small Arduino wrapper.

> netbird-lite is an independent project. It is not affiliated with or endorsed by NetBird GmbH.

## Quick start

1. Install the ESP32 boards package (**arduino-esp32 3.2 or newer**) in the Arduino IDE.
2. Install this library: download the repository as ZIP and use *Sketch → Include Library → Add
   .ZIP Library*, or with arduino-cli:
   `arduino-cli lib install --git-url https://github.com/insight-it-gmbh/netbird-esp32-arduino.git`
   (needs `arduino-cli config set library.enable_unsafe_install true` once).
3. In the NetBird dashboard, create a setup key and a policy that lets your other peers reach
   the ESP32.
4. Open *File → Examples → netbird-lite → Basic*, copy `arduino_secrets.h.example` to `arduino_secrets.h`, fill it in and upload.

```cpp
#include <NetBirdLite.h>
#include <WebServer.h>
#include <WiFi.h>

WebServer server(80);

void setup() {
  Serial.begin(115200);
  WiFi.begin("your-ssid", "your-password");
  while (WiFi.status() != WL_CONNECTED) delay(250);

  NetBirdLite.begin("https://netbird.example.com", "SETUP-KEY", "esp32-demo");

  server.on("/", [] { server.send(200, "text/plain", "hello over NetBird\n"); });
  server.begin();
}

void loop() {
  server.handleClient();
  if (NetBirdLite.connected()) {
    // NetBirdLite.localIP() is the address other peers use
  }
}
```

After a few seconds the device shows up in the NetBird dashboard. The setup key is only needed
for the first registration: the identity is stored in NVS, later starts can pass `nullptr`.

## API

| Call | Meaning |
|---|---|
| `bool begin(url, setupKey = nullptr, hostname = nullptr)` | starts the client in the background; call after Wi-Fi/Ethernet is connected |
| `bool connected()` | in the NetBird network (network map received) |
| `IPAddress localIP()`, `uint8_t prefixLength()` | NetBird address of the device |
| `int peers()`, `int peersUp()` | remote peers in the network map / with an active WireGuard session |
| `bool signalConnected()`, `bool relayConnected()` | connection state to signal and relay |
| `nbl_state_t state()`, `const char *stateName()` | `IDLE`, `WAIT_TIME`, `CONNECTING`, `LOGIN`, `SYNCED`, `RETRY_WAIT`, `FATAL` |
| `String lastError()`, `esp_err_t lastStartError()` | why the client is retrying or stopped; result of `begin()` |
| `String publicKey()` | WireGuard public key, as shown in the dashboard |
| `nbl_status_t status()` | all fields and counters at once |
| `void forget()` | erase the stored identity (call before `begin()`) |
| `void end()` | stop the background tasks (no restart without reboot) |

The C API of the ESP-IDF component (`netbird_lite.h`) is available too.

## Examples

- **Basic**: joins the network and serves a web page over it.
- **Client**: the ESP32 calls an HTTP service on another peer. Replies pass the firewall through
  connection tracking, so the policy only needs to allow the direction ESP32 → peer.
- **TestServer**: the endpoints the end-to-end tests of netbird-esp32 use (`/status`, `/echo`,
  `/blob`, `/fetch`, a filtered TCP port), for checking a board end to end.

The examples use the *Huge APP* partition scheme in CI (*Tools → Partition Scheme*);
on 4 MB boards the default layout is tight (Basic uses 94% of it), so pick Huge APP when in doubt.

## What it does not do

- **No direct peer-to-peer connections.** There is no ICE/STUN: all traffic goes through your
  NetBird relay, like the official client with *Force relay connection*. Remote peers must use
  the same relay server.
- IPv4 only. No DNS, routes or exit nodes, SSH, Rosenpass or lazy connections. Setup keys only
  (no SSO). Port ranges in policies are not delivered to the client; single ports are.
- At most 4 remote peers by default (`CONFIG_NBL_MAX_PEERS`, up to 8).
- ESP32 family only. Classic Arduino boards (AVR, SAMD, Renesas) do not have the memory for
  TLS and WireGuard.

## Tested

| Board family | Result |
|---|---|
| ESP32-C6 (Waveshare ESP32-C6-LCD-1.69, Wi-Fi) | end-to-end against a self-hosted NetBird v0.80.0: registered, ping 52 ms over the relay, HTTP, outbound TCP via connection tracking, deny rules, 12 min soak |
| ESP32, ESP32-S2, ESP32-S3, ESP32-C3 | compile with all warnings on; not hardware-tested yet |

Measured on the ESP32-C6 with the TestServer example: the library adds 76 KB of flash and 22 KB of
static RAM; while connected the client uses about 65 KB of heap. A 200 KB download through the
relay ran at 36 kB/s (the Arduino TCP defaults are smaller than the ESP-IDF example's; the pure
protocol path reaches 145 kB/s, see the netbird-esp32 README).

## Configuration

The ESP-IDF component is configured with Kconfig. In Arduino the defaults are in
[`src/nbl_arduino_config.h`](src/nbl_arduino_config.h); override them with compiler flags, for
example in a file named `build_opt.h` in the sketch folder:

```
-DCONFIG_NBL_MAX_PEERS=8
```

| Option | Default | Meaning |
|---|---|---|
| `CONFIG_NBL_MAX_PEERS` | 4 | remote peers (1-8), about 1.1 KB of RAM each |
| `CONFIG_NBL_MAX_RULES` | 32 | inbound firewall rules |
| `CONFIG_NBL_ACL_ENFORCE` | 1 | enforce the NetBird access policies on inbound traffic |
| `CONFIG_NBL_CT_ENTRIES` | 16 | connection tracking entries |
| `CONFIG_NBL_MTU` | 1280 | WireGuard MTU, unless management sends one |
| `CONFIG_NBL_SNTP`, `CONFIG_NBL_SNTP_SERVER` | 1, "pool.ntp.org" | set the clock if the sketch does not (TLS and WireGuard need it) |
| `CONFIG_NBL_KEYGEN_ENTROPY_SOURCE` | 0 | entropy source for the first key generation, see Security |
| `CONFIG_NBL_CTRL_STACK`, `CONFIG_NBL_RELAY_STACK` | 10240, 6144 | task stacks in bytes |

netbird-lite registers ESP-IDF's eventfd driver. A sketch that also uses eventfd must register it
before `NetBirdLite.begin()`.

## Security

- The firewall is on by default and follows the NetBird policies for inbound traffic. Replies to
  connections the ESP32 opens itself always pass.
- **The WireGuard private key is stored in NVS in plain text.** Production devices should use
  flash and NVS encryption.
- The key is generated on the first start. ESP-IDF only guarantees true random numbers while
  Wi-Fi or Bluetooth is running, which is the normal case in the examples (Wi-Fi is connected
  before `begin()`). On Ethernet-only boards set `-DCONFIG_NBL_KEYGEN_ENTROPY_SOURCE=1`.
- A setup key compiled into firmware can be read from it. Use one-off keys bound to a group, or
  pass the key at run time.
- TLS certificates are verified against the certificate bundle of the ESP32 core.

Please report security issues privately through GitHub's *Report a vulnerability* button.

## How this library is built

Everything in `src/` except `NetBirdLite.h`, `NetBirdLite.cpp` and `nbl_arduino_config.h` is
generated from netbird-esp32 by [`tools/sync_upstream.py`](tools/sync_upstream.py); `UPSTREAM`
records the commit. CI regenerates it and fails on any difference, so fixes are made upstream
once. The generator rewrites include paths for Arduino's single include directory and gives the
vendored code's global symbols an `nblx_` prefix, so the library links next to other libraries
that bundle nanopb or wireguard-lwip. Protocol details and pitfalls:
[netbird-esp32/docs/PROTOCOL-NOTES.md](https://github.com/insight-it-gmbh/netbird-esp32/blob/main/docs/PROTOCOL-NOTES.md).

## About the authors

<a href="https://www.insight-it.de">
  <img src=".assets/brand.png" alt="Insight-IT GmbH" height="60">
</a>

**[Insight-IT GmbH](https://www.insight-it.de)** is an IT service provider in Berlin, Germany.
For over 20 years serving companies, public clients, and government organizations.

Built and maintained by **[Dominic Ballenthin](https://github.com/subbie5000)**.

## License

Apache-2.0, see [LICENSE](LICENSE). If you distribute this library or a modified version of it,
keep the [NOTICE](NOTICE) file with it, as section 4 of the license requires. Third-party
components and their licences are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

NetBird is developed by NetBird GmbH. WireGuard and the WireGuard logo are registered trademarks
of Jason A. Donenfeld. This project is not affiliated with either.

Copyright © 2026 Insight-IT GmbH, Berlin — Dominic Ballenthin.
