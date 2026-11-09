// Persistent calibration in LittleFS (/calibration.json). Survives reboots and
// firmware updates; only a filesystem upload or "reset calibration" removes it.
#pragma once

#include <stdint.h>

#include "ripple_calibrator.h"

namespace storage {

struct Stored {
  char source[8] = "";  // "wizard", "quick" or "manual"
  float mean = 0;
  float sigma = 0;
  float max = 0;        // highest empty-room window variance
  float threshold = 0;
  float movingCoverage = 0;
  float stillCoverage = 0;
  uint16_t holdoutFalseSeconds = 0;
  ripple::Calibrator::Quality quality = ripple::Calibrator::Quality::None;
  bool stillDetectable = false;
};

bool begin();
bool load(Stored& out);
bool save(const Stored& in);
void clear();

// MQTT settings (/mqtt.json). Stored in plain text in the board's flash, like
// WiFi credentials: use a dedicated broker account with limited permissions.
struct Mqtt {
  bool enabled = false;
  char host[64] = "";
  uint16_t port = 1883;
  char user[32] = "";
  char pass[64] = "";
  char prefix[32] = "homeassistant";  // Home Assistant discovery prefix
};

bool loadMqtt(Mqtt& out);
bool saveMqtt(const Mqtt& in);

// WiFi network to join (/wifi.json), written by the setup portal only after a
// successful connection. Plain text in flash, like the MQTT password.
struct Wifi {
  char ssid[33] = "";
  char pass[65] = "";
};

bool loadWifi(Wifi& out);
bool saveWifi(const Wifi& in);
void clearWifi();

}  // namespace storage
