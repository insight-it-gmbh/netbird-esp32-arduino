// SPDX-License-Identifier: Apache-2.0
// netbird-lite build configuration for the Arduino ESP32 core.
//
// The ESP-IDF component takes these values from Kconfig (idf.py menuconfig -> netbird-lite) and
// from compile definitions in its CMakeLists.txt. The Arduino core has neither for libraries, so
// this file supplies them. Every generated source file includes it first.
//
// To change a value, pass it as a compiler flag, for example -DCONFIG_NBL_MAX_PEERS=8:
//   Arduino IDE / arduino-cli: put the flag into a file named build_opt.h in the sketch folder
//   PlatformIO:                build_flags = -DCONFIG_NBL_MAX_PEERS=8
// The meaning of each option is documented in components/netbird_lite/Kconfig of netbird-esp32.
#pragma once

#include "esp_idf_version.h"
#include "sdkconfig.h"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 4, 0)
#error "netbird-lite needs arduino-esp32 3.2 or newer (ESP-IDF 5.4 or newer)"
#endif

/* ---- Kconfig options, with the defaults of the ESP-IDF component ---- */

/* HTTP/2: only the built-in client; the nghttp2 backend is not part of the Arduino core */
#define CONFIG_NBL_H2_LITE 1
#undef CONFIG_NBL_H2_NGHTTP2

#ifndef CONFIG_NBL_MAX_PEERS
#define CONFIG_NBL_MAX_PEERS 4 /* remote peers, about 1.1 KB of RAM each */
#endif
#ifndef CONFIG_NBL_MAX_RULES
#define CONFIG_NBL_MAX_RULES 32 /* inbound firewall rules */
#endif
#ifndef CONFIG_NBL_ACL_ENFORCE
#define CONFIG_NBL_ACL_ENFORCE 1 /* enforce the NetBird access policies on inbound traffic */
#endif
#ifndef CONFIG_NBL_CT_ENTRIES
#define CONFIG_NBL_CT_ENTRIES 16 /* connection tracking entries, 16 bytes each */
#endif
#ifndef CONFIG_NBL_MTU
#define CONFIG_NBL_MTU 1280 /* WireGuard MTU unless management sends one */
#endif
#ifndef CONFIG_NBL_SNTP
#define CONFIG_NBL_SNTP 1 /* start SNTP if nobody else did (TLS and WireGuard need the time) */
#endif
#ifndef CONFIG_NBL_SNTP_SERVER
#define CONFIG_NBL_SNTP_SERVER "pool.ntp.org"
#endif
#ifndef CONFIG_NBL_KEYGEN_ENTROPY_SOURCE
#define CONFIG_NBL_KEYGEN_ENTROPY_SOURCE 0 /* see the Security section of the README */
#endif
#ifndef CONFIG_NBL_CTRL_STACK
#define CONFIG_NBL_CTRL_STACK 10240 /* control task stack, bytes */
#endif
#ifndef CONFIG_NBL_RELAY_STACK
#define CONFIG_NBL_RELAY_STACK 6144 /* relay task stack, bytes */
#endif

/* ranges as in Kconfig */
#if CONFIG_NBL_MAX_PEERS < 1 || CONFIG_NBL_MAX_PEERS > 8
#error "CONFIG_NBL_MAX_PEERS must be between 1 and 8"
#endif
#if CONFIG_NBL_MAX_RULES < 4 || CONFIG_NBL_MAX_RULES > 256
#error "CONFIG_NBL_MAX_RULES must be between 4 and 256"
#endif
#if CONFIG_NBL_CT_ENTRIES < 4 || CONFIG_NBL_CT_ENTRIES > 64
#error "CONFIG_NBL_CT_ENTRIES must be between 4 and 64"
#endif
#if CONFIG_NBL_MTU < 576 || CONFIG_NBL_MTU > 1420
#error "CONFIG_NBL_MTU must be between 576 and 1420"
#endif
#if CONFIG_NBL_CTRL_STACK < 6144 || CONFIG_NBL_RELAY_STACK < 4096
#error "CONFIG_NBL_CTRL_STACK must be at least 6144 and CONFIG_NBL_RELAY_STACK at least 4096"
#endif

/* ---- compile definitions of the ESP-IDF build (target_compile_definitions) ---- */

/* nanopb tests this with #if defined(), so it enables 32-bit field descriptors, as upstream */
#ifndef PB_FIELD_32BIT
#define PB_FIELD_32BIT 0
#endif
#ifndef WIREGUARD_MAX_PEERS
#define WIREGUARD_MAX_PEERS CONFIG_NBL_MAX_PEERS
#endif

/* prefixes for the global symbols of the vendored code (generated) */
#include "nbl_arduino_symbols.h"
