// Pure helpers for WiFi provisioning (the captive portal lives in
// src/wifi_setup.cpp). No Arduino headers, so they are unit tested on the
// computer.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ripple {
namespace wifi {

// Network name: 1..32 bytes, no control characters. UTF-8 is fine.
bool validSsid(const char* ssid, const char** whyNot = nullptr);

// Password: empty (open network), 8..63 printable ASCII (WPA2 passphrase), or
// exactly 64 hex digits (raw WPA2 key).
bool validPassword(const char* pass, const char** whyNot = nullptr);

// "Ripple-4f7ddc": the setup network the board opens.
void accessPointName(uint32_t chipId, char* out, size_t cap);

// "ripple-4f7ddc": the mDNS / DHCP host name.
void hostName(uint32_t chipId, char* out, size_t cap);

// Writes `in` as the inside of a JSON string (quotes, backslashes and control
// characters escaped). Returns the length, or 0 if it does not fit.
size_t escapeJson(const char* in, char* out, size_t cap);

}  // namespace wifi
}  // namespace ripple
