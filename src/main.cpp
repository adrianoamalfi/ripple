// Ripple - WiFi-only motion & presence detection for the ESP8266.
//
// Samples the RSSI of the link to the router, detects motion from its
// fluctuation, and serves a live page + WebSocket API (see web.h).
//
// Calibration is loaded from flash at boot. With nothing saved, a quick
// automatic calibration runs (assuming an empty room); the guided wizard on
// the web page, or over serial, replaces it and is saved.
//
// Serial commands (115200 baud):
//   w  start the guided calibration wizard     n  "I'm in position, start"
//   x  cancel the wizard                       c  quick calibration (empty room)
//   r  forget the saved calibration
//   p  open the WiFi setup portal           W  (dev) join the build-time default network
//                                              through the portal path

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include "ripple_calibrator.h"
#include "mqtt.h"
#include "ripple_core.h"
#include "storage.h"
#include "web.h"
#include "wifi_setup.h"

#ifndef RIPPLE_SAMPLE_INTERVAL_MS
#define RIPPLE_SAMPLE_INTERVAL_MS 100  // 10 samples per second
#endif

// RSSI only refreshes when frames arrive from the router, so we poke the
// gateway with a tiny UDP packet on the "discard" port every sample. The
// router answers (ICMP port unreachable), which keeps the RSSI updating.
#ifndef RIPPLE_PROBE_PORT
#define RIPPLE_PROBE_PORT 9
#endif

using ripple::Calibrator;

static ripple::Detector detector;
static Calibrator calibrator;
static WiFiUDP udp;

static uint32_t lastSampleMs = 0;
static uint32_t lastPrintMs = 0;
static bool lastMotion = false;
static bool lastPresence = false;

static Calibrator::State lastWizState = Calibrator::State::Idle;
static Calibrator::Stage lastWizStage = Calibrator::Stage::EmptyBefore;
static uint32_t lastCountdownPrint = 0;

// What the page shows as "active calibration".
static Calibrator::Result shownResult;   // valid = a wizard result (new or reloaded)
static char calSource[8] = "none";
static bool calSaved = false;
static bool quickInProgress = false;
static bool quickIsUserRequested = false;  // save the quick calibration only if asked for

static float varPeak = 0;
static int lastRssi = 0;
static bool lastValid = false;

static const char* phaseName(ripple::Phase p) {
  switch (p) {
    case ripple::Phase::Uncalibrated: return "uncalibrated";
    case ripple::Phase::Calibrating: return "calibrating";
    default: return "running";
  }
}

// ---------------------------------------------------------------- persistence

static void saveCurrent(const char* source) {
  const ripple::Output& o = detector.output();
  storage::Stored s;
  strlcpy(s.source, source, sizeof(s.source));
  s.mean = o.baselineMean;
  s.sigma = o.baselineSigma;
  s.max = o.baselineMax;
  s.threshold = o.threshold;
  if (shownResult.valid) {
    s.movingCoverage = shownResult.movingCoverage;
    s.stillCoverage = shownResult.stillCoverage;
    s.holdoutFalseSeconds = shownResult.holdoutFalseSeconds;
    s.quality = shownResult.quality;
    s.stillDetectable = shownResult.stillDetectable;
  }
  calSaved = storage::save(s);
  strlcpy(calSource, source, sizeof(calSource));
  Serial.printf("Calibration %s (%s), threshold %.2f\n",
                calSaved ? "saved" : "NOT saved - flash write failed", source, o.threshold);
}

static bool loadSaved() {
  storage::Stored s;
  if (!storage::load(s)) return false;
  detector.applyCalibration(s.mean, s.sigma, s.threshold, s.max);
  strlcpy(calSource, s.source, sizeof(calSource));
  calSaved = true;
  shownResult = Calibrator::Result();
  if (s.quality != Calibrator::Quality::None) {
    shownResult.valid = true;
    shownResult.baselineMean = s.mean;
    shownResult.baselineSigma = s.sigma;
    shownResult.noiseCeiling = s.max;
    shownResult.threshold = s.threshold;
    shownResult.movingCoverage = s.movingCoverage;
    shownResult.stillCoverage = s.stillCoverage;
    shownResult.holdoutFalseSeconds = s.holdoutFalseSeconds;
    shownResult.quality = s.quality;
    shownResult.stillDetectable = s.stillDetectable;
  }
  Serial.printf("Loaded saved calibration (%s), threshold %.2f\n", s.source, s.threshold);
  return true;
}

// ---------------------------------------------------------------- calibration

static void startQuick(bool userRequested) {
  detector.startCalibration();
  quickInProgress = true;
  quickIsUserRequested = userRequested;
  shownResult = Calibrator::Result();
  strlcpy(calSource, "none", sizeof(calSource));
  calSaved = false;
  Serial.println("Quick calibration - keep the room empty (~45 s)...");
}

static void printWizardResult() {
  const Calibrator::Result& r = calibrator.result();
  Serial.println("[wizard] ---- result ----");
  Serial.printf("[wizard] quality        : %s\n", Calibrator::qualityName(r.quality));
  Serial.printf("[wizard] noise (empty)  : mean %.2f, sigma %.2f, ceiling %.2f\n",
                r.baselineMean, r.baselineSigma, r.noiseCeiling);
  Serial.printf("[wizard] threshold      : %.2f\n", r.threshold);
  Serial.printf("[wizard] walking seen   : %.0f%% of seconds\n", r.movingCoverage * 100);
  Serial.printf("[wizard] sitting seen   : %.0f%% of seconds%s\n", r.stillCoverage * 100,
                r.stillDetectable ? "" : " (a still person is not detectable here)");
  Serial.printf("[wizard] hold-out false : %u s above threshold in the 2nd empty stage\n",
                static_cast<unsigned>(r.holdoutFalseSeconds));
  if (r.quality == Calibrator::Quality::Poor) {
    Serial.println("[wizard] Signal too stable to see walking: try moving the board a few "
                   "metres from the router, then run the wizard again.");
  }
}

// Print wizard prompts when its state changes, and apply+save the result.
static void serviceWizard(uint32_t now) {
  const Calibrator::State st = calibrator.state();
  const Calibrator::Stage stage = calibrator.stage();

  if (st != lastWizState || stage != lastWizStage) {
    switch (st) {
      case Calibrator::State::WaitingUser:
        Serial.printf("[wizard] Step %u/4: %s. Get ready, then send 'n'.\n",
                      static_cast<unsigned>(stage) + 1, Calibrator::stageName(stage));
        break;
      case Calibrator::State::Countdown:
        Serial.println("[wizard] Get in position...");
        break;
      case Calibrator::State::Recording:
        Serial.printf("[wizard] RECORDING: %s\n", Calibrator::stageName(stage));
        break;
      case Calibrator::State::Done: {
        const Calibrator::Result& r = calibrator.result();
        detector.applyCalibration(r.baselineMean, r.baselineSigma, r.threshold, r.noiseCeiling);
        quickInProgress = false;  // the wizard replaces a quick calibration still running
        shownResult = r;
        printWizardResult();
        saveCurrent("wizard");
        break;
      }
      case Calibrator::State::Failed:
        Serial.println("[wizard] Failed: not enough samples (WiFi dropped?). Send 'w' to retry.");
        break;
      default:
        break;
    }
    lastWizState = st;
    lastWizStage = stage;
  }

  if ((st == Calibrator::State::Countdown || st == Calibrator::State::Recording) &&
      now - lastCountdownPrint >= 5000) {
    lastCountdownPrint = now;
    Serial.printf("[wizard] %s: %u s left\n", Calibrator::stateName(st),
                  static_cast<unsigned>(calibrator.secondsLeft(now)));
  }
}

static void startWizard() {
  calibrator.start();
  Serial.println("[wizard] Starting guided calibration.");
}

static void handleCommand(const web::Command& c, uint32_t now) {
  switch (c.cmd) {
    case web::Cmd::WizardStart: startWizard(); break;
    case web::Cmd::WizardReady: calibrator.ready(now); break;
    case web::Cmd::WizardCancel:
      calibrator.cancel();
      Serial.println("[wizard] Cancelled.");
      break;
    case web::Cmd::QuickCalibrate: startQuick(true); break;
    case web::Cmd::SetThreshold:
      if (detector.output().phase == ripple::Phase::Running) {
        detector.setThreshold(c.value);
        saveCurrent("manual");
      }
      break;
    case web::Cmd::ResetCalibration:
      storage::clear();
      startQuick(false);
      break;
    default: break;
  }
}

// ---------------------------------------------------------------------- setup

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\nRipple %s\n", RIPPLE_VERSION);
  Serial.printf("Reset reason: %s\n", ESP.getResetReason().c_str());

  if (!storage::begin()) Serial.println("Warning: flash filesystem unavailable, calibration will not be saved.");
  wifi_setup::begin();  // joins the saved network, or opens the setup portal
  web::begin();
  mqtt::begin(RIPPLE_VERSION);

  if (!loadSaved()) {
    Serial.println("No saved calibration: running a standard quick calibration.");
    startQuick(false);
  }
  Serial.println("Send 'w' for the guided calibration wizard (or use the web page).");
}

// ----------------------------------------------------------------------- loop

void loop() {
  while (Serial.available()) {
    switch (Serial.read()) {
      case 'c': startQuick(true); break;
      case 'w': startWizard(); break;
      case 'n': calibrator.ready(millis()); break;
      case 'x':
        calibrator.cancel();
        Serial.println("[wizard] Cancelled.");
        break;
      case 'r':
        storage::clear();
        calSaved = false;
        Serial.println("Saved calibration removed.");
        break;
      case 'p': wifi_setup::openPortal(); break;
      case 'W': wifi_setup::provisionWithBuildDefaults(); break;
    }
  }

  web::loop();

  const uint32_t now = millis();
  wifi_setup::loop(now);
  web::Command cmd;
  while (web::poll(cmd)) handleCommand(cmd, now);

  if (now - lastSampleMs < RIPPLE_SAMPLE_INTERVAL_MS) return;
  lastSampleMs = now;

  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected) {
    udp.beginPacket(WiFi.gatewayIP(), RIPPLE_PROBE_PORT);
    udp.write('r');
    udp.endPacket();
  }

  const int rssi = WiFi.RSSI();
  // The SDK reports 31 when there is no valid link.
  const bool valid = connected && rssi < 0;
  const ripple::Output& o = detector.update(rssi, valid, now);
  lastRssi = rssi;
  lastValid = valid;
  if (detector.ready() && o.variance > varPeak) varPeak = o.variance;

  // A quick calibration that just finished: keep it only if the user asked.
  if (quickInProgress && o.phase == ripple::Phase::Running) {
    quickInProgress = false;
    Serial.printf("Quick calibration done, threshold %.2f\n", o.threshold);
    if (quickIsUserRequested) saveCurrent("quick");
    else strlcpy(calSource, "quick", sizeof(calSource));
  }

  calibrator.update(now, valid && detector.ready(), o.variance);
  serviceWizard(now);

  mqtt::Inputs mi;
  mi.running = o.phase == ripple::Phase::Running;
  mi.motion = mi.running && o.motion;
  mi.presence = mi.running && o.presence;
  mi.linkOk = valid;
  mi.rssi = rssi;
  mi.variance = detector.ready() ? o.variance : 0;
  mi.threshold = o.threshold;
  mqtt::loop(mi, now);

  web::Snapshot snap;
  snap.ms = now;
  snap.rssi = rssi;
  snap.linkOk = valid;
  snap.varPeak = varPeak;
  snap.out = &o;
  snap.wizState = calibrator.state();
  snap.wizStage = calibrator.stage();
  snap.wizSecondsLeft = calibrator.secondsLeft(now);
  snap.wizTotalSeconds = calibrator.state() == Calibrator::State::Countdown
                             ? calibrator.countdownSeconds()
                             : calibrator.recordSeconds();
  snap.result = shownResult.valid ? &shownResult : nullptr;
  snap.calSource = calSource;
  snap.saved = calSaved;
  snap.freeHeap = ESP.getFreeHeap();
  snap.mqttState = mqtt::state();
  snap.net = wifi_setup::networkName();
  snap.host = wifi_setup::hostName();
  if (web::publish(snap)) varPeak = 0;  // peak-hold restarts after each push

#ifdef RIPPLE_STREAM
  // Raw capture for offline tuning: one CSV line per sample.
  Serial.printf("S,%lu,%d,%d,%.3f\n", static_cast<unsigned long>(now), rssi, valid,
                o.variance);
  return;
#endif

  if (o.motion != lastMotion || o.presence != lastPresence) {
    Serial.printf("[%lu] motion=%d presence=%d\n", static_cast<unsigned long>(now),
                  o.motion, o.presence);
    lastMotion = o.motion;
    lastPresence = o.presence;
  }

  if (now - lastPrintMs >= 500) {
    lastPrintMs = now;
    Serial.printf("rssi=%d var=%.2f thr=%.2f score=%.2f %s", rssi, o.variance,
                  o.threshold, o.score, phaseName(o.phase));
    if (o.phase == ripple::Phase::Calibrating) {
      Serial.printf(" %.0f%%", o.calibrationProgress * 100);
    }
    Serial.println();
  }
}
