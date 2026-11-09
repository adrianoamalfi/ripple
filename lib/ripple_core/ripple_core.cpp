#include "ripple_core.h"

#include <math.h>

namespace ripple {

namespace {

constexpr float kMinMargin = 1e-3f;  // guards the score division

uint16_t clampWindow(uint16_t n) {
  if (n < 2) return 2;
  if (n > kMaxWindow) return kMaxWindow;
  return n;
}

}  // namespace

Detector::Detector(const Config& cfg) : cfg_(cfg) {
  cfg_.windowSize = clampWindow(cfg_.windowSize);
  for (size_t i = 0; i < kMaxWindow; i++) window_[i] = 0;
}

void Detector::setConfig(const Config& cfg) {
  const bool windowChanged = clampWindow(cfg.windowSize) != cfg_.windowSize;
  cfg_ = cfg;
  cfg_.windowSize = clampWindow(cfg_.windowSize);

  if (windowChanged) {
    // Window variances are not comparable across window sizes: the old
    // baseline is meaningless, so the user must calibrate again.
    head_ = 0;
    filled_ = 0;
    out_ = Output();
    return;
  }

  if (out_.phase == Phase::Running && autoThreshold_) {
    // k / minMargin / ceilingSafety can be tuned live from the stored baseline.
    out_.threshold = autoThreshold();
  }
}

float Detector::autoThreshold() const {
  float margin = cfg_.k * out_.baselineSigma;
  if (margin < cfg_.minMargin) margin = cfg_.minMargin;
  if (margin < kMinMargin) margin = kMinMargin;
  float thr = out_.baselineMean + margin;
  const float ceiling = out_.baselineMax * cfg_.ceilingSafety;
  return thr > ceiling ? thr : ceiling;
}

void Detector::startCalibration() {
  head_ = 0;
  filled_ = 0;
  calibCount_ = 0;
  calibMean_ = 0;
  calibM2_ = 0;
  calibMax_ = 0;
  everMoved_ = false;
  lastMotionMs_ = 0;
  out_ = Output();
  out_.phase = Phase::Calibrating;
}

void Detector::applyCalibration(float baselineMean, float baselineSigma, float threshold,
                                float baselineMax) {
  out_.baselineMean = baselineMean;
  out_.baselineSigma = baselineSigma;
  out_.baselineMax = baselineMax;
  out_.calibrationProgress = 1.0f;
  out_.phase = Phase::Running;
  out_.motion = false;
  out_.presence = false;
  everMoved_ = false;
  lastMotionMs_ = 0;
  autoThreshold_ = false;
  setThreshold(threshold);
}

void Detector::setThreshold(float threshold) {
  const float floor = out_.baselineMean + kMinMargin;
  out_.threshold = threshold > floor ? threshold : floor;
  autoThreshold_ = false;
}

float Detector::windowVariance() const {
  const uint16_t n = cfg_.windowSize;
  float sum = 0;
  for (uint16_t i = 0; i < n; i++) sum += window_[i];
  const float mean = sum / n;
  float acc = 0;
  for (uint16_t i = 0; i < n; i++) {
    const float d = window_[i] - mean;
    acc += d * d;
  }
  return acc / n;
}

void Detector::finishCalibration() {
  const double variance = calibCount_ > 1 ? calibM2_ / calibCount_ : 0.0;
  out_.baselineMean = static_cast<float>(calibMean_);
  out_.baselineSigma = static_cast<float>(sqrt(variance));
  out_.baselineMax = static_cast<float>(calibMax_);
  out_.threshold = autoThreshold();
  out_.calibrationProgress = 1.0f;
  out_.phase = Phase::Running;
  out_.motion = false;
  out_.presence = false;
  autoThreshold_ = true;
}

const Output& Detector::update(int rssi, bool valid, uint32_t nowMs) {
  if (!valid) return out_;

  if (rssi < -127) rssi = -127;
  if (rssi > 127) rssi = 127;

  window_[head_] = static_cast<int8_t>(rssi);
  head_ = (head_ + 1) % cfg_.windowSize;
  if (filled_ < cfg_.windowSize) {
    filled_++;
    if (filled_ < cfg_.windowSize) return out_;  // window not full yet
  }

  out_.variance = windowVariance();

  // Uncalibrated: keep the variance live (the guided Calibrator reads it) but
  // do not detect anything.
  if (out_.phase == Phase::Uncalibrated) return out_;

  if (out_.phase == Phase::Calibrating) {
    // Welford's online mean / variance of the window variance.
    calibCount_++;
    const double delta = out_.variance - calibMean_;
    calibMean_ += delta / calibCount_;
    calibM2_ += delta * (out_.variance - calibMean_);
    if (out_.variance > calibMax_) calibMax_ = out_.variance;

    const uint32_t goal = cfg_.calibrationSamples ? cfg_.calibrationSamples : 1;
    out_.calibrationProgress = static_cast<float>(calibCount_) / goal;
    if (calibCount_ >= goal) finishCalibration();
    return out_;
  }

  // Running
  const float margin = out_.threshold - out_.baselineMean;
  const float offThreshold = out_.baselineMean + cfg_.hysteresis * margin;

  if (!out_.motion && out_.variance > out_.threshold) {
    out_.motion = true;
  } else if (out_.motion && out_.variance < offThreshold) {
    out_.motion = false;
  }

  if (out_.motion) {
    everMoved_ = true;
    lastMotionMs_ = nowMs;
  }

  out_.presence = out_.motion ||
                  (everMoved_ && (nowMs - lastMotionMs_) < cfg_.presenceTimeoutMs);

  const float score = (out_.variance - out_.baselineMean) / margin;
  out_.score = score > 0 ? score : 0;
  return out_;
}

}  // namespace ripple
