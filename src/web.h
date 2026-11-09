// HTTP + WebSocket interface.
//
//   GET /            calibration / live view page
//   GET /api/state   current state as JSON
//   WS  /ws          state pushed ~5 times a second; commands as JSON text:
//                    {"cmd":"wizard_start"} {"cmd":"wizard_ready"}
//                    {"cmd":"wizard_cancel"} {"cmd":"quick_calibrate"}
//                    {"cmd":"set_threshold","value":3.5} {"cmd":"reset_calibration"}
//
// Commands are queued here and applied by the main loop, so the detector is
// only ever touched from one place.
#pragma once

#include <stdint.h>

#include "ripple_calibrator.h"
#include "ripple_core.h"

namespace web {

enum class Cmd : uint8_t {
  None,
  WizardStart,
  WizardReady,
  WizardCancel,
  QuickCalibrate,
  SetThreshold,
  ResetCalibration,
};

struct Command {
  Cmd cmd = Cmd::None;
  float value = 0;
};

struct Snapshot {
  uint32_t ms = 0;
  int rssi = 0;
  bool linkOk = false;
  float varPeak = 0;  // highest window variance since the previous snapshot
  const ripple::Output* out = nullptr;
  ripple::Calibrator::State wizState = ripple::Calibrator::State::Idle;
  ripple::Calibrator::Stage wizStage = ripple::Calibrator::Stage::EmptyBefore;
  uint32_t wizSecondsLeft = 0;
  uint32_t wizTotalSeconds = 0;  // length of the current countdown / recording
  const ripple::Calibrator::Result* result = nullptr;  // null if none yet
  const char* calSource = "";  // where the active calibration came from
  bool saved = false;
  uint32_t freeHeap = 0;
  const char* mqttState = "disabled";  // "disabled" | "connecting" | "connected" | "failed"
  const char* net = "";                // WiFi network the board is on
  const char* host = "";               // mDNS name, without ".local"
};

void begin();
bool post(const Command& c);          // queue a command (from MQTT, same path as the web UI)
bool poll(Command& out);              // next queued command, if any
bool publish(const Snapshot& snap);   // call often; true when a new state was built (~5/s)
void loop();                          // housekeeping, call from loop()

}  // namespace web
