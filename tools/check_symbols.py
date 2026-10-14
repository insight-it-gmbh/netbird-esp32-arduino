#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Fails if the compiled library exports a global symbol without a netbird-lite prefix.

Usage: tools/check_symbols.py BUILD_PATH [NM]

BUILD_PATH is an arduino-cli --build-path after compiling any example. The library's objects are
in BUILD_PATH/libraries/<library>/. NM defaults to the nm found on PATH; GNU nm reads the symbol
tables of Xtensa and RISC-V objects as well.

Allowed are names starting with nbl_ (netbird-lite), nblx_ (vendored code, see
tools/vendor_symbols.txt), management_ / signalexchange_ (generated protobuf descriptors) and the
Arduino wrapper (NetBirdLite, NetBirdLiteClass methods). Anything else would collide with other
libraries that bundle the same code; add it to tools/vendor_symbols.txt and regenerate src/.
"""
import os
import re
import subprocess
import sys

ALLOWED = re.compile(r"^(nbl_|nblx_|management_|signalexchange_|NetBirdLite$|_ZN16NetBirdLiteClass|_ZNK16NetBirdLiteClass)")


def main():
    if len(sys.argv) not in (2, 3):
        print(__doc__.strip().split("\n\n")[0])
        sys.exit(2)
    build, nm = sys.argv[1], (sys.argv[2] if len(sys.argv) == 3 else "nm")
    libdir = os.path.join(build, "libraries")
    objs = []
    for dirpath, _, names in os.walk(libdir):
        if os.path.basename(dirpath) == "libraries":
            continue
        objs += [os.path.join(dirpath, n) for n in names if n.endswith((".o", ".a"))]
    # The library's own directory below libraries/ (name = folder or repo name, not necessarily
    # "netbird"): take every directory except the ESP32 core's bundled libraries.
    BUNDLED = {"BLE", "BlueDisplay", "EEPROM", "ESP32", "ESP_SR", "ESPmDNS", "Ethernet", "FFat",
               "FS", "Hash", "HTTPClient", "HTTPUpdate", "HTTPUpdateServer", "Insights",
               "LittleFS", "Network", "NetworkClientSecure", "Preferences", "RainMaker", "SD",
               "SD_MMC", "SPI", "SPIFFS", "SimpleBLE", "Ticker", "USB", "Update", "WebServer",
               "WebSockets", "WiFi", "WiFiProv", "Wire", "Zigbee", "esp_modbus", "fysetc", "genie", "rmaker"}
    objs = [o for o in objs if os.path.relpath(o, libdir).split(os.sep)[0] not in BUNDLED]
    if not objs:
        sys.exit("check_symbols: no library objects below %s" % libdir)
    bad, total = {}, 0
    for o in objs:
        out = subprocess.run([nm, "-g", "--defined-only", o], capture_output=True, text=True)
        if out.returncode != 0:
            sys.exit("check_symbols: %s failed on %s: %s" % (nm, o, out.stderr.strip()))
        for line in out.stdout.splitlines():
            parts = line.split()
            if len(parts) != 3:
                continue
            total += 1
            if not ALLOWED.match(parts[2]):
                bad.setdefault(parts[2], os.path.relpath(o, build))
    if bad:
        print("global symbols without a netbird-lite prefix:")
        for s, o in sorted(bad.items()):
            print("  %-40s %s" % (s, o))
        sys.exit(1)
    print("check_symbols: %d global symbols in %d objects, all prefixed" % (total, len(objs)))


if __name__ == "__main__":
    main()
