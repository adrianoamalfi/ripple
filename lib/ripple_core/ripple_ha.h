// MQTT topics and Home Assistant discovery messages for Ripple.
//
// Pure string building, no network and no Arduino headers, so it is unit
// tested on the computer. The MQTT client lives in src/mqtt.cpp.
//
// Topic layout, with <base> = "ripple/<id>":
//   <base>/status          online / offline (retained, also the Last Will)
//   <base>/detecting       online while calibrated and detecting, else offline
//   <base>/motion          ON / OFF            (retained)
//   <base>/presence        ON / OFF            (retained)
//   <base>/rssi            dBm
//   <base>/fluctuation     peak window variance since the last update, dB^2
//   <base>/threshold       current motion threshold
//   <base>/threshold/set   command: new threshold
//   <base>/calibrate/set   command: "PRESS" starts a quick calibration
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ripple {
namespace ha {

struct Device {
  char id[24] = "";    // "ripple_<chip id>", also the HA unique id prefix
  char base[40] = "";  // "ripple/<id>"
  char name[24] = "";  // "Ripple <chip id>"
};

// chipId is the ESP's unique chip id; any 32-bit value works in tests.
void makeDevice(Device& out, uint32_t chipId);

enum class Topic : uint8_t {
  Status,
  Detecting,
  Motion,
  Presence,
  Rssi,
  Fluctuation,
  Threshold,
  ThresholdSet,
  CalibrateSet,
};

enum class Entity : uint8_t {
  Motion,
  Presence,
  Rssi,
  Fluctuation,
  Threshold,
  Calibrate,
};
constexpr uint8_t kEntityCount = 6;

// Writes the topic into `out`. Returns its length, or 0 if it does not fit.
size_t topic(const Device& dev, Topic t, char* out, size_t cap);

// Builds one discovery message: the config topic and its JSON payload.
// `prefix` is the HA discovery prefix ("homeassistant"); `fw` and `url` are
// shown in the device info. Returns false if a buffer is too small.
bool discovery(const Device& dev, Entity e, const char* prefix, const char* fw,
               const char* url, char* topicOut, size_t topicCap, char* payloadOut,
               size_t payloadCap);

}  // namespace ha
}  // namespace ripple
