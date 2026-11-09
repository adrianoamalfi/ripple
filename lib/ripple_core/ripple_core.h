// Ripple core: RSSI-fluctuation motion / presence detector.
//
// Framework independent (no Arduino headers, no dynamic allocation) so it can
// run on the ESP8266, in host-side unit tests, and later inside an ESPHome
// external component.
//
// Idea: a moving body disturbs the 2.4 GHz multipath, so the RSSI of the link
// fluctuates more. We measure the variance of RSSI over a short sliding
// window, learn what that variance looks like in an empty room, and flag
// motion when it rises clearly above that baseline.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ripple {

constexpr size_t kMaxWindow = 64;

struct Config {
  uint16_t windowSize = 20;           // samples in the sliding window (<= kMaxWindow)
  uint32_t calibrationSamples = 400;  // window variances used to learn the baseline
  float k = 3.0f;                     // threshold = mean + k * sigma of baseline
  float minMargin = 0.5f;             // minimum threshold margin over the mean (dB^2),
                                      // needed because RSSI is an integer and a quiet
                                      // room can have sigma == 0
  float ceilingSafety = 1.15f;        // threshold is also at least this * the highest window
                                      // variance seen while calibrating. Window variance has
                                      // a heavy tail, so mean + k*sigma alone can fall
                                      // below noise the calibration itself saw.
  float hysteresis = 0.5f;            // motion ends below mean + hysteresis * margin
  uint32_t presenceTimeoutMs = 30000; // presence stays on this long after last motion
};

enum class Phase : uint8_t {
  Uncalibrated,  // waiting for startCalibration()
  Calibrating,   // learning the empty-room baseline
  Running,       // detecting
};

struct Output {
  Phase phase = Phase::Uncalibrated;
  float variance = 0;       // variance of the current window (dB^2)
  float baselineMean = 0;   // learned mean window variance
  float baselineSigma = 0;  // learned std deviation of window variance
  float baselineMax = 0;    // highest window variance seen in the empty room
  float threshold = 0;      // variance above which motion starts
  float score = 0;          // 0 = at baseline, 1 = at threshold, >1 = motion
  float calibrationProgress = 0;  // 0..1 while calibrating
  bool motion = false;
  bool presence = false;
};

class Detector {
 public:
  explicit Detector(const Config& cfg = Config());

  void setConfig(const Config& cfg);
  const Config& config() const { return cfg_; }

  // Forget the baseline and learn a new one. The room should be empty.
  void startCalibration();

  // Enter Running with a baseline / threshold learned elsewhere (the guided
  // Calibrator). The threshold stays fixed until changed with setThreshold().
  void applyCalibration(float baselineMean, float baselineSigma, float threshold,
                        float baselineMax = 0);
  void setThreshold(float threshold);

  // True once the window is full, i.e. output().variance is meaningful. This
  // holds in every phase, including Uncalibrated.
  bool ready() const { return filled_ >= cfg_.windowSize; }

  // Feed one RSSI sample (dBm). `valid` = false for failed reads (e.g. link
  // down); those samples are ignored. `nowMs` is a monotonic millisecond clock.
  const Output& update(int rssi, bool valid, uint32_t nowMs);
  const Output& update(int rssi, uint32_t nowMs) { return update(rssi, true, nowMs); }

  const Output& output() const { return out_; }

 private:
  float windowVariance() const;
  void finishCalibration();
  float autoThreshold() const;

  Config cfg_;
  Output out_;

  int8_t window_[kMaxWindow];
  uint16_t head_ = 0;
  uint16_t filled_ = 0;

  // Welford accumulators over window variances during calibration.
  uint32_t calibCount_ = 0;
  double calibMean_ = 0;
  double calibM2_ = 0;
  double calibMax_ = 0;

  uint32_t lastMotionMs_ = 0;
  bool everMoved_ = false;
  bool autoThreshold_ = true;  // threshold derived from mean + k*sigma (vs. set externally)
};

}  // namespace ripple
