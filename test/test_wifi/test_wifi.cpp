#include <ArduinoJson.h>
#include <string.h>
#include <unity.h>

#include <string>

#include "ripple_wifi.h"

using namespace ripple::wifi;

void setUp() {}
void tearDown() {}

void test_ssid_rules() {
  const char* why = nullptr;
  TEST_ASSERT_TRUE(validSsid("Home WiFi"));
  TEST_ASSERT_TRUE(validSsid("Caf\xC3\xA9 5G"));          // UTF-8 is fine
  TEST_ASSERT_TRUE(validSsid("x"));
  TEST_ASSERT_TRUE(validSsid("12345678901234567890123456789012"));  // exactly 32

  TEST_ASSERT_FALSE(validSsid("", &why));
  TEST_ASSERT_EQUAL_STRING("Network name is required", why);
  TEST_ASSERT_FALSE(validSsid(nullptr));
  TEST_ASSERT_FALSE(validSsid("123456789012345678901234567890123", &why));  // 33
  TEST_ASSERT_EQUAL_STRING("Network name is too long (max 32 bytes)", why);
  TEST_ASSERT_FALSE(validSsid("bad\nname", &why));
  TEST_ASSERT_EQUAL_STRING("Network name has invalid characters", why);
  TEST_ASSERT_FALSE(validSsid("tab\there"));
}

void test_password_rules() {
  const char* why = nullptr;
  TEST_ASSERT_TRUE(validPassword(""));          // open network
  TEST_ASSERT_TRUE(validPassword(nullptr));
  TEST_ASSERT_TRUE(validPassword("12345678"));  // shortest WPA2
  TEST_ASSERT_TRUE(validPassword("p@ss \"w\\rd!"));  // symbols, space, quote
  TEST_ASSERT_TRUE(validPassword(std::string(63, 'a').c_str()));
  TEST_ASSERT_TRUE(validPassword(std::string(64, 'f').c_str()));  // raw hex key

  TEST_ASSERT_FALSE(validPassword("short", &why));
  TEST_ASSERT_EQUAL_STRING("Password must be at least 8 characters", why);
  TEST_ASSERT_FALSE(validPassword(std::string(64, 'z').c_str(), &why));  // 64 but not hex
  TEST_ASSERT_EQUAL_STRING("A 64-character key must be hexadecimal", why);
  TEST_ASSERT_FALSE(validPassword(std::string(65, 'a').c_str(), &why));
  TEST_ASSERT_EQUAL_STRING("Password is too long (max 63 characters)", why);
  TEST_ASSERT_FALSE(validPassword("tab\tpassword", &why));
  TEST_ASSERT_EQUAL_STRING("Password has invalid characters", why);
  TEST_ASSERT_FALSE(validPassword("caf\xC3\xA9password"));  // WPA2 passphrases are ASCII
}

void test_names_come_from_the_chip_id() {
  char ap[24], host[24];
  accessPointName(0x4f7ddc, ap, sizeof(ap));
  hostName(0x4f7ddc, host, sizeof(host));
  TEST_ASSERT_EQUAL_STRING("Ripple-4f7ddc", ap);
  TEST_ASSERT_EQUAL_STRING("ripple-4f7ddc", host);
  accessPointName(0xAB000012, ap, sizeof(ap));  // only the low 24 bits, zero padded
  TEST_ASSERT_EQUAL_STRING("Ripple-000012", ap);
}

void test_escaped_ssids_stay_valid_json() {
  const char* nasty[] = {"plain", "with \"quotes\"", "back\\slash", "line\nbreak", "tab\there",
                         "ctrl\x01\x1f", "Caf\xC3\xA9", "{\"a\":1}", ""};
  for (const char* s : nasty) {
    char esc[256];
    escapeJson(s, esc, sizeof(esc));
    std::string doc = std::string("{\"ssid\":\"") + esc + "\"}";
    JsonDocument parsed;
    TEST_ASSERT_TRUE_MESSAGE(!deserializeJson(parsed, doc), doc.c_str());
    TEST_ASSERT_EQUAL_STRING(s, parsed["ssid"] | "x");  // round trips exactly
  }
}

void test_escape_reports_overflow() {
  char tiny[6];
  TEST_ASSERT_EQUAL(0, escapeJson("this is far too long", tiny, sizeof(tiny)));
  TEST_ASSERT_EQUAL('\0', tiny[0]);
  TEST_ASSERT_EQUAL(0, escapeJson("x", tiny, 0));
  char exact[4];
  TEST_ASSERT_EQUAL(3, escapeJson("abc", exact, sizeof(exact)));  // fits with its terminator
  char escOverflow[4];
  TEST_ASSERT_EQUAL(0, escapeJson("\"\"", escOverflow, sizeof(escOverflow)));  // 4 chars once escaped
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_ssid_rules);
  RUN_TEST(test_password_rules);
  RUN_TEST(test_names_come_from_the_chip_id);
  RUN_TEST(test_escaped_ssids_stay_valid_json);
  RUN_TEST(test_escape_reports_overflow);
  return UNITY_END();
}
