#include "storage.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <string.h>

namespace storage {

namespace {
constexpr const char* kPath = "/calibration.json";
constexpr int kVersion = 1;
}  // namespace

bool begin() {
  if (LittleFS.begin()) return true;
  // First boot on a fresh flash: format, then mount again.
  return LittleFS.format() && LittleFS.begin();
}

bool load(Stored& out) {
  File f = LittleFS.open(kPath, "r");
  if (!f) return false;
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err || doc["v"] != kVersion) return false;

  const float threshold = doc["threshold"] | 0.0f;
  if (!(threshold > 0)) return false;  // also rejects NaN

  Stored s;
  strlcpy(s.source, doc["source"] | "manual", sizeof(s.source));
  s.mean = doc["mean"] | 0.0f;
  s.sigma = doc["sigma"] | 0.0f;
  s.max = doc["max"] | 0.0f;
  s.threshold = threshold;
  s.movingCoverage = doc["moving"] | 0.0f;
  s.stillCoverage = doc["still"] | 0.0f;
  s.holdoutFalseSeconds = doc["holdout"] | 0;
  s.quality = static_cast<ripple::Calibrator::Quality>(doc["quality"] | 0);
  s.stillDetectable = doc["stillOk"] | false;
  out = s;
  return true;
}

bool save(const Stored& in) {
  JsonDocument doc;
  doc["v"] = kVersion;
  doc["source"] = in.source;
  doc["mean"] = in.mean;
  doc["sigma"] = in.sigma;
  doc["max"] = in.max;
  doc["threshold"] = in.threshold;
  doc["moving"] = in.movingCoverage;
  doc["still"] = in.stillCoverage;
  doc["holdout"] = in.holdoutFalseSeconds;
  doc["quality"] = static_cast<uint8_t>(in.quality);
  doc["stillOk"] = in.stillDetectable;

  File f = LittleFS.open(kPath, "w");
  if (!f) return false;
  const size_t n = serializeJson(doc, f);
  f.close();
  return n > 0;
}

void clear() {
  LittleFS.remove(kPath);
}

namespace {
constexpr const char* kMqttPath = "/mqtt.json";
}

bool loadMqtt(Mqtt& out) {
  File f = LittleFS.open(kMqttPath, "r");
  if (!f) return false;
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  Mqtt m;
  m.enabled = doc["enabled"] | false;
  strlcpy(m.host, doc["host"] | "", sizeof(m.host));
  const int port = doc["port"] | 1883;
  m.port = (port > 0 && port <= 65535) ? static_cast<uint16_t>(port) : 1883;
  strlcpy(m.user, doc["user"] | "", sizeof(m.user));
  strlcpy(m.pass, doc["pass"] | "", sizeof(m.pass));
  strlcpy(m.prefix, doc["prefix"] | "homeassistant", sizeof(m.prefix));
  out = m;
  return true;
}

bool saveMqtt(const Mqtt& in) {
  JsonDocument doc;
  doc["enabled"] = in.enabled;
  doc["host"] = in.host;
  doc["port"] = in.port;
  doc["user"] = in.user;
  doc["pass"] = in.pass;
  doc["prefix"] = in.prefix;

  File f = LittleFS.open(kMqttPath, "w");
  if (!f) return false;
  const size_t n = serializeJson(doc, f);
  f.close();
  return n > 0;
}

namespace {
constexpr const char* kWifiPath = "/wifi.json";
}

bool loadWifi(Wifi& out) {
  File f = LittleFS.open(kWifiPath, "r");
  if (!f) return false;
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;
  Wifi w;
  strlcpy(w.ssid, doc["ssid"] | "", sizeof(w.ssid));
  strlcpy(w.pass, doc["pass"] | "", sizeof(w.pass));
  if (!w.ssid[0]) return false;
  out = w;
  return true;
}

bool saveWifi(const Wifi& in) {
  JsonDocument doc;
  doc["ssid"] = in.ssid;
  doc["pass"] = in.pass;
  File f = LittleFS.open(kWifiPath, "w");
  if (!f) return false;
  const size_t n = serializeJson(doc, f);
  f.close();
  return n > 0;
}

void clearWifi() {
  LittleFS.remove(kWifiPath);
}

}  // namespace storage
