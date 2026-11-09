#include "mqtt.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncMqttClient.h>
#include <ESP8266WiFi.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ripple_ha.h"
#include "storage.h"
#include "web.h"

namespace mqtt {

namespace {

using ripple::ha::Entity;
using ripple::ha::Topic;

enum class State : uint8_t { Disabled, Waiting, Connecting, Connected, Failed };

constexpr uint32_t kConnectTimeoutMs = 15000;
constexpr uint32_t kInitialBackoffMs = 5000;
constexpr uint32_t kMaxBackoffMs = 60000;
constexpr uint32_t kCloseWaitMs = 3000;       // max wait for a connection to finish closing
constexpr uint32_t kPeriodicMs = 10000;       // rssi / fluctuation / threshold
constexpr uint32_t kDiscoveryGapMs = 80;      // spread the discovery messages out
constexpr float kThresholdChange = 0.04f;

// AsyncMqttClient keeps *pointers* to the host, credentials and Last Will, so
// everything it is given lives in static storage.
AsyncMqttClient client;
storage::Mqtt cfg;
ripple::ha::Device dev;
char fwVersion[16] = "";
char url[48] = "";
char statusTopic[72] = "";
char thresholdSetTopic[72] = "";
char calibrateSetTopic[72] = "";
constexpr char kOffline[] = "offline";

State st = State::Disabled;
char error[40] = "";
uint32_t nextAttemptMs = 0;
uint32_t connectStartedMs = 0;
uint32_t backoffMs = kInitialBackoffMs;

// AsyncMqttClient ignores connect() until a connection we closed has finished
// closing, and reports that end through onDisconnect. We must not mistake that
// callback for a failure of the next attempt, so we wait for it.
bool awaitingClose = false;
uint32_t closeRequestedMs = 0;

void closeConnection(bool force, uint32_t now) {
  awaitingClose = true;
  closeRequestedMs = now;
  client.disconnect(force);
}

struct Published {
  int8_t detecting = -1;
  int8_t motion = -1;
  int8_t presence = -1;
  float threshold = -1;
  uint32_t periodicMs = 0;
  bool periodicSent = false;
};
Published last;
uint8_t discoveryIdx = 0;
uint32_t lastDiscoveryMs = 0;
float peak = 0;

bool configured() { return cfg.enabled && cfg.host[0]; }

void refreshUrl() {
  snprintf(url, sizeof(url), "http://%s/", WiFi.localIP().toString().c_str());
}

bool pub(Topic t, const char* payload, bool retain) {
  char topic[72];
  if (!ripple::ha::topic(dev, t, topic, sizeof(topic))) return false;
  return client.publish(topic, 0, retain, payload, strlen(payload)) != 0;
}

void applyToClient() {
  client.setServer(cfg.host, cfg.port);
  client.setClientId(dev.id);
  if (cfg.user[0]) client.setCredentials(cfg.user, cfg.pass[0] ? cfg.pass : nullptr);
  else client.setCredentials(nullptr, nullptr);
}

void fail(const char* message, uint32_t now) {
  strlcpy(error, message, sizeof(error));
  st = State::Failed;
  nextAttemptMs = now + backoffMs;
  backoffMs = backoffMs * 2 > kMaxBackoffMs ? kMaxBackoffMs : backoffMs * 2;
}

const char* reasonText(AsyncMqttClientDisconnectReason r, bool& authProblem) {
  authProblem = false;
  switch (r) {
    case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED: return "Broker unreachable";
    case AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION:
      return "Protocol not supported";
    case AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED: return "Client id rejected";
    case AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE: return "Broker unavailable";
    case AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS:
      authProblem = true;
      return "Malformed credentials";
    case AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED:
      authProblem = true;
      return "Wrong user or password";
    case AsyncMqttClientDisconnectReason::ESP8266_NOT_ENOUGH_SPACE: return "Out of memory";
    default: return "Connection failed";
  }
}

void onConnect(bool) {
  st = State::Connected;
  error[0] = 0;
  backoffMs = kInitialBackoffMs;
  discoveryIdx = 0;
  lastDiscoveryMs = 0;
  last = Published();
  client.subscribe(thresholdSetTopic, 0);
  client.subscribe(calibrateSetTopic, 0);
  pub(Topic::Status, "online", true);
}

void onDisconnect(AsyncMqttClientDisconnectReason reason) {
  if (awaitingClose) {  // the end of a connection we closed ourselves
    awaitingClose = false;
    return;
  }
  if (st != State::Connecting && st != State::Connected) return;
  bool auth = false;
  const char* text = reasonText(reason, auth);
  if (auth) backoffMs = kMaxBackoffMs;  // do not hammer a broker that says no
  fail(text, millis());
}

void onMessage(char* topic, char* payload, AsyncMqttClientMessageProperties, size_t len,
               size_t index, size_t total) {
  // Commands are tiny; ignore anything fragmented or oversized.
  if (index != 0 || len != total || len == 0 || len > 31) return;
  char buf[32];
  memcpy(buf, payload, len);
  buf[len] = 0;

  if (!strcmp(topic, thresholdSetTopic)) {
    char* end = nullptr;
    const float v = strtof(buf, &end);
    if (end != buf && *end == 0 && v >= 0.5f && v <= 100.0f) {
      web::Command c;
      c.cmd = web::Cmd::SetThreshold;
      c.value = v;
      web::post(c);
    }
  } else if (!strcmp(topic, calibrateSetTopic) && !strcmp(buf, "PRESS")) {
    web::Command c;
    c.cmd = web::Cmd::QuickCalibrate;
    web::post(c);
  }
}

void publishDiscovery(uint32_t now) {
  if (now - lastDiscoveryMs < kDiscoveryGapMs) return;
  lastDiscoveryMs = now;
  char topic[128];
  char payload[800];
  if (!ripple::ha::discovery(dev, static_cast<Entity>(discoveryIdx), cfg.prefix, fwVersion, url,
                             topic, sizeof(topic), payload, sizeof(payload))) {
    discoveryIdx++;  // cannot be built: skip it rather than block everything behind it
    return;
  }
  if (client.publish(topic, 0, true, payload, strlen(payload)) != 0) discoveryIdx++;
}

void publishStates(const Inputs& in, uint32_t now) {
  const int8_t detecting = in.running ? 1 : 0;
  if (last.detecting != detecting &&
      pub(Topic::Detecting, in.running ? "online" : "offline", true)) {
    last.detecting = detecting;
  }
  const int8_t motion = in.motion ? 1 : 0;
  if (last.motion != motion && pub(Topic::Motion, in.motion ? "ON" : "OFF", true)) {
    last.motion = motion;
  }
  const int8_t presence = in.presence ? 1 : 0;
  if (last.presence != presence && pub(Topic::Presence, in.presence ? "ON" : "OFF", true)) {
    last.presence = presence;
  }

  char buf[16];
  // The threshold can change at any time (dashboard, wizard): publish it at once.
  if (in.running && fabsf(in.threshold - last.threshold) > kThresholdChange) {
    snprintf(buf, sizeof(buf), "%.1f", in.threshold);
    if (pub(Topic::Threshold, buf, true)) last.threshold = in.threshold;
  }

  if (!last.periodicSent || now - last.periodicMs >= kPeriodicMs) {
    bool ok = true;
    if (in.linkOk) {
      snprintf(buf, sizeof(buf), "%d", in.rssi);
      ok = pub(Topic::Rssi, buf, false) && ok;
    }
    snprintf(buf, sizeof(buf), "%.2f", peak);
    ok = pub(Topic::Fluctuation, buf, false) && ok;
    if (ok) {
      last.periodicMs = now;
      last.periodicSent = true;
      peak = 0;  // peak-hold restarts after each report
    }
  }
}

bool isHost(const char* s) {
  size_t n = strlen(s);
  if (n == 0 || n > 63) return false;
  for (size_t i = 0; i < n; i++) {
    const char c = s[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          c == '.' || c == '-' || c == '_')) {
      return false;
    }
  }
  return true;
}

// Printable ASCII, no quote or backslash: safe to print back inside JSON.
bool isPlain(const char* s, size_t maxLen) {
  const size_t n = strlen(s);
  if (n > maxLen) return false;
  for (size_t i = 0; i < n; i++) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x20 || c > 0x7E || c == '"' || c == '\\') return false;
  }
  return true;
}

bool isPrefix(const char* s) {
  const size_t n = strlen(s);
  if (n == 0 || n > 31 || s[0] == '/' || s[n - 1] == '/') return false;
  for (size_t i = 0; i < n; i++) {
    const char c = s[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          c == '_' || c == '-' || c == '/')) {
      return false;
    }
  }
  return true;
}

bool reject(char* err, size_t cap, const char* message) {
  if (err && cap) strlcpy(err, message, cap);
  return false;
}

// Moves to the new settings: say goodbye politely, then reconnect from scratch.
void reconfigure(uint32_t now) {
  if (client.connected()) {
    pub(Topic::Status, kOffline, true);
    closeConnection(false, now);
  } else if (st == State::Connecting) {
    closeConnection(true, now);
  }
  error[0] = 0;
  backoffMs = kInitialBackoffMs;
  nextAttemptMs = now;  // the loop waits for the old connection to finish closing
  applyToClient();
  st = configured() ? State::Waiting : State::Disabled;
}

}  // namespace

void begin(const char* firmwareVersion) {
  strlcpy(fwVersion, firmwareVersion, sizeof(fwVersion));
  ripple::ha::makeDevice(dev, ESP.getChipId());
  ripple::ha::topic(dev, Topic::Status, statusTopic, sizeof(statusTopic));
  ripple::ha::topic(dev, Topic::ThresholdSet, thresholdSetTopic, sizeof(thresholdSetTopic));
  ripple::ha::topic(dev, Topic::CalibrateSet, calibrateSetTopic, sizeof(calibrateSetTopic));
  refreshUrl();

  storage::loadMqtt(cfg);
  client.onConnect(onConnect);
  client.onDisconnect(onDisconnect);
  client.onMessage(onMessage);
  client.setKeepAlive(30);
  client.setCleanSession(true);
  client.setWill(statusTopic, 1, true, kOffline, sizeof(kOffline) - 1);
  applyToClient();
  st = configured() ? State::Waiting : State::Disabled;
}

void loop(const Inputs& in, uint32_t now) {
  if (in.variance > peak) peak = in.variance;
  if (!configured()) {
    st = State::Disabled;
    return;
  }
  if (WiFi.status() != WL_CONNECTED) return;

  switch (st) {
    case State::Waiting:
    case State::Failed:
      if (awaitingClose) {
        if (now - closeRequestedMs <= kCloseWaitMs) break;
        awaitingClose = false;  // no callback came: stop waiting
      }
      if (static_cast<int32_t>(now - nextAttemptMs) >= 0) {
        refreshUrl();
        st = State::Connecting;
        connectStartedMs = now;
        client.connect();
      }
      break;
    case State::Connecting:
      if (now - connectStartedMs > kConnectTimeoutMs) {
        fail("Connection timed out", now);
        closeConnection(true, now);
      }
      break;
    case State::Connected:
      if (discoveryIdx < ripple::ha::kEntityCount) publishDiscovery(now);
      else publishStates(in, now);
      break;
    default:
      break;
  }
}

const char* state() {
  switch (st) {
    case State::Disabled: return "disabled";
    case State::Connected: return "connected";
    case State::Failed: return "failed";
    default: return "connecting";
  }
}

const char* lastError() {
  return error;
}

size_t configJson(char* out, size_t cap) {
  const int n = snprintf(out, cap,
                         "{\"enabled\":%d,\"host\":\"%s\",\"port\":%u,\"user\":\"%s\","
                         "\"hasPass\":%d,\"prefix\":\"%s\",\"state\":\"%s\",\"error\":\"%s\","
                         "\"id\":\"%s\",\"base\":\"%s\"}",
                         cfg.enabled, cfg.host, static_cast<unsigned>(cfg.port), cfg.user,
                         cfg.pass[0] ? 1 : 0, cfg.prefix, state(), error, dev.id, dev.base);
  return (n > 0 && static_cast<size_t>(n) < cap) ? static_cast<size_t>(n) : 0;
}

bool applyConfig(const char* body, size_t len, char* err, size_t errCap) {
  JsonDocument doc;
  if (deserializeJson(doc, body, len)) return reject(err, errCap, "Invalid JSON");

  storage::Mqtt next = cfg;

  if (!doc["enabled"].isNull()) next.enabled = doc["enabled"].as<bool>();

  if (doc["host"].is<const char*>()) {
    const char* h = doc["host"];
    if (h[0] && !isHost(h)) {
      return reject(err, errCap, "Host may only contain letters, digits, dots, dashes and underscores");
    }
    strlcpy(next.host, h, sizeof(next.host));
  }
  if (!doc["port"].isNull()) {
    const long p = doc["port"].as<long>();
    if (p < 1 || p > 65535) return reject(err, errCap, "Port must be between 1 and 65535");
    next.port = static_cast<uint16_t>(p);
  }
  if (doc["user"].is<const char*>()) {
    const char* u = doc["user"];
    if (!isPlain(u, sizeof(next.user) - 1)) {
      return reject(err, errCap, "Invalid user name (max 31 plain characters)");
    }
    strlcpy(next.user, u, sizeof(next.user));
  }
  if (doc["pass"].is<const char*>()) {  // absent keeps the stored password
    const char* p = doc["pass"];
    if (strlen(p) > sizeof(next.pass) - 1) return reject(err, errCap, "Password too long (max 63)");
    for (const char* c = p; *c; c++) {
      if (static_cast<unsigned char>(*c) < 0x20) return reject(err, errCap, "Invalid password");
    }
    strlcpy(next.pass, p, sizeof(next.pass));
  }
  if (doc["prefix"].is<const char*>()) {
    const char* pf = doc["prefix"];
    if (!isPrefix(pf)) {
      return reject(err, errCap, "Discovery prefix must be letters, digits, _ - or / (max 31)");
    }
    strlcpy(next.prefix, pf, sizeof(next.prefix));
  }
  if (next.enabled && !next.host[0]) return reject(err, errCap, "Host is required");

  if (!storage::saveMqtt(next)) return reject(err, errCap, "Could not save the settings");
  cfg = next;
  reconfigure(millis());
  return true;
}

}  // namespace mqtt
