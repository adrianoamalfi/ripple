#include "ripple_calibrator.h"

#include <math.h>

namespace ripple {

namespace {

constexpr float kGoodCoverage = 0.6f;
constexpr float kFairCoverage = 0.3f;

// Wrap-safe "now has reached deadline".
bool reached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

}  // namespace

Calibrator::Calibrator() : Calibrator(Params()) {}

Calibrator::Calibrator(const Params& p) : p_(p) {
  setParams(p);
}

void Calibrator::setParams(const Params& p) {
  p_ = p;
  const uint32_t maxMs = static_cast<uint32_t>(kMaxBuckets) * 1000;
  if (p_.recordMs > maxMs) p_.recordMs = maxMs;
  if (p_.recordMs < 1000) p_.recordMs = 1000;
}

void Calibrator::start() {
  stage_ = Stage::EmptyBefore;
  state_ = State::WaitingUser;
  result_ = Result();
}

void Calibrator::ready(uint32_t nowMs) {
  if (state_ != State::WaitingUser) return;
  state_ = State::Countdown;
  phaseEndMs_ = nowMs + p_.countdownMs;
}

void Calibrator::cancel() {
  state_ = State::Idle;
  result_ = Result();
}

uint32_t Calibrator::secondsLeft(uint32_t nowMs) const {
  if (state_ != State::Countdown && state_ != State::Recording) return 0;
  if (reached(nowMs, phaseEndMs_)) return 0;
  return (phaseEndMs_ - nowMs + 999) / 1000;
}

void Calibrator::beginRecording(uint32_t nowMs) {
  StageStats& s = stats_[static_cast<uint8_t>(stage_)];
  s = StageStats();
  for (uint8_t i = 0; i < kMaxBuckets; i++) s.bucketMax[i] = -1.0f;
  s.buckets = static_cast<uint8_t>((p_.recordMs + 999) / 1000);
  recStartMs_ = nowMs;
  phaseEndMs_ = nowMs + p_.recordMs;
  state_ = State::Recording;
}

void Calibrator::update(uint32_t nowMs, bool hasVariance, float variance) {
  if (state_ == State::Countdown) {
    if (reached(nowMs, phaseEndMs_)) beginRecording(nowMs);
    return;
  }
  if (state_ != State::Recording) return;

  StageStats& s = stats_[static_cast<uint8_t>(stage_)];
  if (hasVariance && (nowMs - recStartMs_) >= p_.settleMs) {
    s.windows++;
    const double delta = variance - s.mean;
    s.mean += delta / s.windows;
    s.m2 += delta * (variance - s.mean);
    if (variance > s.max) s.max = variance;

    uint32_t b = (nowMs - recStartMs_) / 1000;
    if (b >= s.buckets) b = s.buckets - 1;
    if (variance > s.bucketMax[b]) s.bucketMax[b] = variance;
  }

  if (reached(nowMs, phaseEndMs_)) finishStage();
}

void Calibrator::finishStage() {
  const StageStats& s = stats_[static_cast<uint8_t>(stage_)];
  // Need at least ~1 window per second, otherwise the link was not delivering
  // samples (WiFi down?) and the stage is useless.
  if (s.windows < s.buckets) {
    state_ = State::Failed;
    result_ = Result();
    return;
  }
  if (stage_ == Stage::EmptyAfter) {
    computeResult();
    state_ = State::Done;
    return;
  }
  stage_ = static_cast<Stage>(static_cast<uint8_t>(stage_) + 1);
  state_ = State::WaitingUser;
}

float Calibrator::coverage(const StageStats& s, float threshold) {
  uint16_t withData = 0, above = 0;
  for (uint8_t i = 0; i < s.buckets; i++) {
    if (s.bucketMax[i] < 0) continue;
    withData++;
    if (s.bucketMax[i] > threshold) above++;
  }
  return withData ? static_cast<float>(above) / withData : 0.0f;
}

void Calibrator::computeResult() {
  const StageStats& e1 = stats_[static_cast<uint8_t>(Stage::EmptyBefore)];
  const StageStats& mv = stats_[static_cast<uint8_t>(Stage::Moving)];
  const StageStats& st = stats_[static_cast<uint8_t>(Stage::Still)];
  const StageStats& e2 = stats_[static_cast<uint8_t>(Stage::EmptyAfter)];

  Result r;

  // Merge the two empty-room stages (parallel Welford).
  const double n1 = e1.windows, n2 = e2.windows, n = n1 + n2;
  const double delta = e2.mean - e1.mean;
  const double mean = (n1 * e1.mean + n2 * e2.mean) / n;
  const double m2 = e1.m2 + e2.m2 + delta * delta * n1 * n2 / n;
  r.baselineMean = static_cast<float>(mean);
  r.baselineSigma = static_cast<float>(sqrt(m2 / n));
  r.noiseCeiling = e1.max > e2.max ? e1.max : e2.max;

  // Hold-out: how noisy was the second empty stage relative to the first?
  const float thr1 = e1.max * p_.safety;
  for (uint8_t i = 0; i < e2.buckets; i++) {
    if (e2.bucketMax[i] > thr1) r.holdoutFalseSeconds++;
  }

  float thr = r.noiseCeiling * p_.safety;
  const float floorThr = r.baselineMean + p_.minMargin;
  if (thr < floorThr) thr = floorThr;
  r.threshold = thr;

  r.movingCoverage = coverage(mv, thr);
  r.stillCoverage = coverage(st, thr);
  r.stillDetectable = r.stillCoverage >= kFairCoverage;

  if (r.movingCoverage >= kGoodCoverage && r.holdoutFalseSeconds == 0) {
    r.quality = Quality::Good;
  } else if (r.movingCoverage >= kFairCoverage) {
    r.quality = Quality::Fair;
  } else {
    r.quality = Quality::Poor;
  }

  r.valid = true;
  result_ = r;
}

const char* Calibrator::stageName(Stage s) {
  switch (s) {
    case Stage::EmptyBefore: return "Leave the room (empty)";
    case Stage::Moving: return "Walk around the room";
    case Stage::Still: return "Sit still in the room";
    default: return "Leave the room again (empty)";
  }
}

const char* Calibrator::stateName(State s) {
  switch (s) {
    case State::Idle: return "idle";
    case State::WaitingUser: return "waiting";
    case State::Countdown: return "countdown";
    case State::Recording: return "recording";
    case State::Done: return "done";
    default: return "failed";
  }
}

const char* Calibrator::qualityName(Quality q) {
  switch (q) {
    case Quality::Good: return "good";
    case Quality::Fair: return "fair";
    case Quality::Poor: return "poor";
    default: return "none";
  }
}

}  // namespace ripple
