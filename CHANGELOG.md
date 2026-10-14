# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/).

## [0.3.0] - 2026-10-14

First public release. Developed by Insight-IT GmbH.

The Arduino packaging of [netbird-lite](https://github.com/insight-it-gmbh/netbird-esp32) 0.3.0:
a NetBird client for ESP32, generated from the ESP-IDF component. Registration with a setup key,
network map from management, offers/answers over signal, WireGuard over the NetBird relay,
stateful inbound firewall from the NetBird policies. Wrapper class `NetBirdLite`, examples
Basic, Client and TestServer. Compiles for esp32, esp32s2, esp32s3, esp32c3 and esp32c6
(arduino-esp32 3.2+); end-to-end tested on ESP32-C6.

[0.3.0]: https://github.com/insight-it-gmbh/netbird-esp32-arduino/releases/tag/v0.3.0
