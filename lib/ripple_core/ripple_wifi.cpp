#include "ripple_wifi.h"

#include <stdio.h>
#include <string.h>

namespace ripple {
namespace wifi {

namespace {

bool setWhy(const char** whyNot, const char* message) {
  if (whyNot) *whyNot = message;
  return false;
}

bool isHex(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

}  // namespace

bool validSsid(const char* ssid, const char** whyNot) {
  if (!ssid || !ssid[0]) return setWhy(whyNot, "Network name is required");
  const size_t n = strlen(ssid);
  if (n > 32) return setWhy(whyNot, "Network name is too long (max 32 bytes)");
  for (size_t i = 0; i < n; i++) {
    const unsigned char c = static_cast<unsigned char>(ssid[i]);
    if (c < 0x20 || c == 0x7F) return setWhy(whyNot, "Network name has invalid characters");
  }
  return true;
}

bool validPassword(const char* pass, const char** whyNot) {
  if (!pass || !pass[0]) return true;  // open network
  const size_t n = strlen(pass);
  if (n == 64) {
    for (size_t i = 0; i < n; i++) {
      if (!isHex(pass[i])) return setWhy(whyNot, "A 64-character key must be hexadecimal");
    }
    return true;
  }
  if (n < 8) return setWhy(whyNot, "Password must be at least 8 characters");
  if (n > 63) return setWhy(whyNot, "Password is too long (max 63 characters)");
  for (size_t i = 0; i < n; i++) {
    const unsigned char c = static_cast<unsigned char>(pass[i]);
    if (c < 0x20 || c > 0x7E) return setWhy(whyNot, "Password has invalid characters");
  }
  return true;
}

void accessPointName(uint32_t chipId, char* out, size_t cap) {
  snprintf(out, cap, "Ripple-%06lx", static_cast<unsigned long>(chipId & 0xFFFFFF));
}

void hostName(uint32_t chipId, char* out, size_t cap) {
  snprintf(out, cap, "ripple-%06lx", static_cast<unsigned long>(chipId & 0xFFFFFF));
}

size_t escapeJson(const char* in, char* out, size_t cap) {
  if (!cap) return 0;
  size_t pos = 0;
  for (; in && *in; in++) {
    const unsigned char c = static_cast<unsigned char>(*in);
    char tmp[8];
    size_t len;
    if (c == '"' || c == '\\') {
      tmp[0] = '\\';
      tmp[1] = static_cast<char>(c);
      len = 2;
    } else if (c < 0x20 || c == 0x7F) {
      len = static_cast<size_t>(snprintf(tmp, sizeof(tmp), "\\u%04x", c));
    } else {
      tmp[0] = static_cast<char>(c);
      len = 1;
    }
    if (pos + len + 1 > cap) {
      out[0] = 0;
      return 0;
    }
    memcpy(out + pos, tmp, len);
    pos += len;
  }
  out[pos] = 0;
  return pos;
}

}  // namespace wifi
}  // namespace ripple
