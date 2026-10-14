# Contributing

Thanks for helping. netbird-lite for Arduino is generated from
[netbird-esp32](https://github.com/insight-it-gmbh/netbird-esp32): everything in `src/` except
`NetBirdLite.h`, `NetBirdLite.cpp` and `nbl_arduino_config.h` comes from there. **Please make
protocol, client and firewall changes upstream in netbird-esp32** and then regenerate
(`tools/sync_upstream.py <checkout-of-netbird-esp32>`, record the commit in `UPSTREAM`); CI fails
if `src/` and the upstream checkout differ.

## Before you send a change

- Compile the examples for esp32c6 with all warnings:
  `arduino-cli compile --fqbn esp32:esp32:esp32c6:PartitionScheme=huge_app --warnings all --library "$PWD" examples/TestServer`
- If you touched the generator: regenerate from upstream and commit the result.
- Changes to the wrapper and the examples belong here; keep the wrapper's API small and stable.

## Licensing of contributions

netbird-lite is licensed under Apache-2.0. Unless you say otherwise, a contribution you submit is
licensed under the same terms (section 5 of the license).

## Reporting bugs

Please include the board, the arduino-esp32 core version, the NetBird server version and the
serial log (tags `nbl`, `nbl_relay`, `nbl_h2`). Security issues: please use GitHub's private
*Report a vulnerability* function instead of a public issue.
