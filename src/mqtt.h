// MQTT client with Home Assistant discovery.
//
// Settings are stored in flash and edited from the dashboard (GET/POST
// /api/mqtt). Once connected, the board announces itself to Home Assistant
// (see lib/ripple_core/ripple_ha.h for the topics), publishes motion /
// presence / signal and accepts two commands: a new threshold and a quick
// calibration. The connection is fully asynchronous, so an unreachable broker
// never stalls the detector.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace mqtt {

// What the main loop feeds in on every sample.
struct Inputs {
  bool running = false;  // calibrated and detecting
  bool motion = false;
  bool presence = false;
  bool linkOk = false;
  int rssi = 0;
  float variance = 0;
  float threshold = 0;
};

void begin(const char* firmwareVersion);
void loop(const Inputs& in, uint32_t nowMs);

// "disabled" | "connecting" | "connected" | "failed"
const char* state();
const char* lastError();

// JSON for GET /api/mqtt. Never includes the password.
size_t configJson(char* out, size_t cap);

// Applies a JSON body from POST /api/mqtt: any subset of enabled, host, port,
// user, pass, prefix. A missing "pass" keeps the stored password. Returns false
// and fills `err` for invalid input.
bool applyConfig(const char* body, size_t len, char* err, size_t errCap);

}  // namespace mqtt
