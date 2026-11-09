#include "ripple_ha.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace ripple {
namespace ha {

namespace {

// Appends printf-style text to buf at pos. False once it no longer fits.
bool append(char* buf, size_t cap, size_t& pos, const char* fmt, ...) {
  if (pos >= cap) return false;
  va_list ap;
  va_start(ap, fmt);
  const int n = vsnprintf(buf + pos, cap - pos, fmt, ap);
  va_end(ap);
  if (n < 0 || static_cast<size_t>(n) >= cap - pos) {
    pos = cap;
    return false;
  }
  pos += static_cast<size_t>(n);
  return true;
}

// Copies `in` keeping only characters that are safe inside a JSON string and
// in a topic, so an odd firmware version or URL can never break the payload.
void sanitize(const char* in, char* out, size_t cap) {
  size_t i = 0;
  for (; in && in[i] && i + 1 < cap; i++) {
    const char c = in[i];
    const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                    (c >= 'A' && c <= 'Z') || c == '.' || c == '-' || c == '_' ||
                    c == ':' || c == '/';
    out[i] = ok ? c : '_';
  }
  out[i] = 0;
}

const char* topicSuffix(Topic t) {
  switch (t) {
    case Topic::Status: return "status";
    case Topic::Detecting: return "detecting";
    case Topic::Motion: return "motion";
    case Topic::Presence: return "presence";
    case Topic::Rssi: return "rssi";
    case Topic::Fluctuation: return "fluctuation";
    case Topic::Threshold: return "threshold";
    case Topic::ThresholdSet: return "threshold/set";
    default: return "calibrate/set";
  }
}

struct Desc {
  const char* component;
  const char* object;
  const char* name;
  bool hasState;
  Topic state;
  bool hasCommand;
  Topic command;
  bool needsDetecting;  // unavailable until the room is calibrated
  const char* extra;    // raw JSON members specific to this entity
};

const Desc kEntities[kEntityCount] = {
    {"binary_sensor", "motion", "Motion", true, Topic::Motion, false, Topic::Motion, true,
     "\"device_class\":\"motion\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\""},
    {"binary_sensor", "presence", "Presence", true, Topic::Presence, false, Topic::Presence,
     true,
     "\"device_class\":\"occupancy\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\""},
    {"sensor", "rssi", "Signal strength", true, Topic::Rssi, false, Topic::Rssi, false,
     "\"device_class\":\"signal_strength\",\"unit_of_measurement\":\"dBm\","
     "\"state_class\":\"measurement\",\"entity_category\":\"diagnostic\","
     "\"suggested_display_precision\":0"},
    {"sensor", "fluctuation", "Signal fluctuation", true, Topic::Fluctuation, false,
     Topic::Fluctuation, false,
     "\"unit_of_measurement\":\"dB\\u00b2\",\"state_class\":\"measurement\","
     "\"entity_category\":\"diagnostic\",\"icon\":\"mdi:sine-wave\","
     "\"suggested_display_precision\":2"},
    {"number", "threshold", "Motion threshold", true, Topic::Threshold, true,
     Topic::ThresholdSet, true,
     "\"min\":0.5,\"max\":100,\"step\":0.1,\"mode\":\"box\","
     "\"entity_category\":\"config\",\"icon\":\"mdi:tune-vertical\""},
    {"button", "calibrate", "Quick calibration", false, Topic::Status, true,
     Topic::CalibrateSet, false,
     "\"payload_press\":\"PRESS\",\"entity_category\":\"config\",\"icon\":\"mdi:target\""},
};

}  // namespace

void makeDevice(Device& out, uint32_t chipId) {
  snprintf(out.id, sizeof(out.id), "ripple_%06lx", static_cast<unsigned long>(chipId & 0xFFFFFF));
  snprintf(out.base, sizeof(out.base), "ripple/%s", out.id);
  snprintf(out.name, sizeof(out.name), "Ripple %06lx", static_cast<unsigned long>(chipId & 0xFFFFFF));
}

size_t topic(const Device& dev, Topic t, char* out, size_t cap) {
  size_t pos = 0;
  if (!append(out, cap, pos, "%s/%s", dev.base, topicSuffix(t))) {
    if (cap) out[0] = 0;
    return 0;
  }
  return pos;
}

bool discovery(const Device& dev, Entity e, const char* prefix, const char* fw,
               const char* url, char* topicOut, size_t topicCap, char* payloadOut,
               size_t payloadCap) {
  const Desc& d = kEntities[static_cast<uint8_t>(e)];

  char pfx[40], fwS[24], urlS[64];
  sanitize(prefix, pfx, sizeof(pfx));
  sanitize(fw, fwS, sizeof(fwS));
  sanitize(url, urlS, sizeof(urlS));

  size_t pos = 0;
  if (!append(topicOut, topicCap, pos, "%s/%s/%s/%s/config", pfx, d.component, dev.id,
              d.object)) {
    return false;
  }

  char t[72];
  size_t p = 0;
  bool ok = append(payloadOut, payloadCap, p,
                   "{\"name\":\"%s\",\"unique_id\":\"%s_%s\",\"has_entity_name\":true,", d.name,
                   dev.id, d.object);

  if (d.hasState) {
    ok = ok && topic(dev, d.state, t, sizeof(t)) &&
         append(payloadOut, payloadCap, p, "\"state_topic\":\"%s\",", t);
  }
  if (d.hasCommand) {
    ok = ok && topic(dev, d.command, t, sizeof(t)) &&
         append(payloadOut, payloadCap, p, "\"command_topic\":\"%s\",", t);
  }

  char status[72], detecting[72];
  ok = ok && topic(dev, Topic::Status, status, sizeof(status));
  if (d.needsDetecting) {
    ok = ok && topic(dev, Topic::Detecting, detecting, sizeof(detecting)) &&
         append(payloadOut, payloadCap, p,
                "\"availability\":[{\"topic\":\"%s\"},{\"topic\":\"%s\"}],"
                "\"availability_mode\":\"all\",",
                status, detecting);
  } else {
    ok = ok && append(payloadOut, payloadCap, p, "\"availability_topic\":\"%s\",", status);
  }

  ok = ok && append(payloadOut, payloadCap, p, "%s,", d.extra);
  ok = ok && append(payloadOut, payloadCap, p,
                    "\"device\":{\"identifiers\":[\"%s\"],\"name\":\"%s\","
                    "\"manufacturer\":\"Ripple\",\"model\":\"ESP8266 WiFi motion sensor\","
                    "\"sw_version\":\"%s\",\"configuration_url\":\"%s\"}}",
                    dev.id, dev.name, fwS, urlS);
  if (!ok && payloadCap) payloadOut[0] = 0;
  return ok;
}

}  // namespace ha
}  // namespace ripple
