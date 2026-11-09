// Guided calibration: the user walks through four labelled stages
//
//   1. EmptyBefore - leave the room            (learn the noise floor)
//   2. Moving      - walk around               (learn what motion looks like)
//   3. Still       - sit still                 (is a motionless person visible?)
//   4. EmptyAfter  - leave again               (hold-out check of the noise floor)
//
// and the threshold is derived from the data instead of a fixed formula. Each
// stage has a "waiting for user" step, a countdown (time to get in position)
// and a fixed recording time. Framework independent and allocation free.
#pragma once

#include <stdint.h>

namespace ripple {

class Calibrator {
 public:
  static constexpr uint8_t kStages = 4;
  static constexpr uint8_t kMaxBuckets = 64;  // 1 s buckets -> max 64 s per stage

  enum class Stage : uint8_t { EmptyBefore, Moving, Still, EmptyAfter };

  enum class State : uint8_t {
    Idle,         // not running
    WaitingUser,  // waiting for the user to press "I'm in position"
    Countdown,    // time to get in position
    Recording,
    Done,
    Failed,
  };

  enum class Quality : uint8_t { None, Poor, Fair, Good };

  struct Params {
    uint32_t countdownMs = 10000;
    uint32_t recordMs = 30000;  // clamped to kMaxBuckets seconds
    uint32_t settleMs = 2500;   // ignored at the start of each recording: the sliding
                                // window still holds samples from the countdown
    float safety = 1.15f;       // threshold = noise ceiling * safety
    float minMargin = 0.5f;     // threshold is at least baseline mean + this (dB^2)
  };

  struct Result {
    bool valid = false;
    float baselineMean = 0;
    float baselineSigma = 0;
    float noiseCeiling = 0;       // highest window variance seen in an empty room
    float threshold = 0;
    float movingCoverage = 0;     // share of 1 s slots of the Moving stage above threshold
    float stillCoverage = 0;      // same for the Still stage
    uint16_t holdoutFalseSeconds = 0;  // EmptyAfter seconds above the EmptyBefore threshold
    Quality quality = Quality::None;
    bool stillDetectable = false;
  };

  Calibrator();
  explicit Calibrator(const Params& p);

  void setParams(const Params& p);

  void start();                  // -> WaitingUser at EmptyBefore
  void ready(uint32_t nowMs);    // user is in position -> Countdown
  void cancel();                 // -> Idle

  // Feed the detector's window variance whenever it is valid. Call it on every
  // sample (hasVariance = false when none is available) so timers advance.
  void update(uint32_t nowMs, bool hasVariance, float variance);

  State state() const { return state_; }
  Stage stage() const { return stage_; }
  const Result& result() const { return result_; }
  uint32_t secondsLeft(uint32_t nowMs) const;  // countdown / recording time left
  uint32_t countdownSeconds() const { return p_.countdownMs / 1000; }
  uint32_t recordSeconds() const { return p_.recordMs / 1000; }

  static const char* stageName(Stage s);
  static const char* stateName(State s);
  static const char* qualityName(Quality q);

 private:
  struct StageStats {
    uint32_t windows = 0;
    double mean = 0;
    double m2 = 0;
    float max = 0;
    float bucketMax[kMaxBuckets];  // -1 = no data in that second
    uint8_t buckets = 0;
  };

  void beginRecording(uint32_t nowMs);
  void finishStage();
  void computeResult();
  static float coverage(const StageStats& s, float threshold);

  Params p_;
  State state_ = State::Idle;
  Stage stage_ = Stage::EmptyBefore;
  uint32_t phaseEndMs_ = 0;
  uint32_t recStartMs_ = 0;
  StageStats stats_[kStages];
  Result result_;
};

}  // namespace ripple
