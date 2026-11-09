#include <unity.h>

#include "ripple_core.h"

using namespace ripple;

namespace {

// Small deterministic PRNG so tests are reproducible.
struct Rng {
  uint32_t s;
  explicit Rng(uint32_t seed) : s(seed) {}
  uint32_t next() {
    s = s * 1664525u + 1013904223u;
    return s >> 8;
  }
  int range(int lo, int hi) { return lo + static_cast<int>(next() % (hi - lo + 1)); }
};

constexpr uint32_t kStepMs = 100;  // 10 samples per second

struct Harness {
  Detector det;
  Rng rng{12345};
  uint32_t now = 1000;

  Harness() : det(smallConfig()) {}

  static Config smallConfig() {
    Config c;
    c.windowSize = 20;
    c.calibrationSamples = 200;
    c.presenceTimeoutMs = 5000;
    return c;
  }

  // Empty room: RSSI jitters by +-1 dB around -60.
  const Output& quiet() {
    now += kStepMs;
    return det.update(-60 + rng.range(-1, 1), now);
  }
  // Someone moving: RSSI swings by +-6 dB.
  const Output& moving() {
    now += kStepMs;
    return det.update(-60 + rng.range(-6, 6), now);
  }
  void quietFor(int n) { for (int i = 0; i < n; i++) quiet(); }
  void movingFor(int n) { for (int i = 0; i < n; i++) moving(); }

  void calibrate() {
    det.startCalibration();
    // window fill + calibration samples + margin
    quietFor(20 + 200 + 5);
  }
};

}  // namespace

void setUp() {}
void tearDown() {}

void test_starts_uncalibrated_and_ignores_samples() {
  Harness h;
  for (int i = 0; i < 100; i++) h.quiet();
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Uncalibrated), static_cast<int>(h.det.output().phase));
  TEST_ASSERT_FALSE(h.det.output().motion);
}

void test_constant_signal_has_zero_variance() {
  Harness h;
  h.det.startCalibration();
  for (int i = 0; i < 40; i++) h.det.update(-55, h.now += kStepMs);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, h.det.output().variance);
}

void test_calibration_progress_and_completion() {
  Harness h;
  h.det.startCalibration();
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Calibrating), static_cast<int>(h.det.output().phase));

  h.quietFor(20 + 100);  // window fill + half of the calibration samples
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.5f, h.det.output().calibrationProgress);
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Calibrating), static_cast<int>(h.det.output().phase));

  h.quietFor(110);
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Running), static_cast<int>(h.det.output().phase));
  TEST_ASSERT_TRUE(h.det.output().threshold > h.det.output().baselineMean);
}

void test_quiet_room_after_calibration_does_not_trigger() {
  Harness h;
  h.calibrate();
  int motionSamples = 0;
  for (int i = 0; i < 1000; i++) {
    if (h.quiet().motion) motionSamples++;
  }
  TEST_ASSERT_EQUAL(0, motionSamples);
  TEST_ASSERT_FALSE(h.det.output().presence);
}

void test_movement_triggers_motion_and_presence() {
  Harness h;
  h.calibrate();
  h.movingFor(40);
  TEST_ASSERT_TRUE(h.det.output().motion);
  TEST_ASSERT_TRUE(h.det.output().presence);
  TEST_ASSERT_TRUE(h.det.output().score > 1.0f);
}

void test_motion_clears_but_presence_holds_until_timeout() {
  Harness h;
  h.calibrate();
  h.movingFor(40);
  TEST_ASSERT_TRUE(h.det.output().motion);

  h.quietFor(30);  // window fully quiet again, 3 s after last movement
  TEST_ASSERT_FALSE(h.det.output().motion);
  TEST_ASSERT_TRUE(h.det.output().presence);  // still inside the 5 s timeout

  h.quietFor(60);  // well past the timeout
  TEST_ASSERT_FALSE(h.det.output().presence);
}

void test_invalid_samples_are_ignored() {
  Harness h;
  h.calibrate();
  const float before = h.det.output().variance;
  for (int i = 0; i < 50; i++) h.det.update(0, false, h.now += kStepMs);
  TEST_ASSERT_EQUAL_FLOAT(before, h.det.output().variance);
  TEST_ASSERT_FALSE(h.det.output().motion);
}

void test_recalibration_resets_state() {
  Harness h;
  h.calibrate();
  h.movingFor(40);
  TEST_ASSERT_TRUE(h.det.output().presence);

  h.det.startCalibration();
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Calibrating), static_cast<int>(h.det.output().phase));
  TEST_ASSERT_FALSE(h.det.output().motion);
  TEST_ASSERT_FALSE(h.det.output().presence);
}

void test_threshold_can_be_tuned_live() {
  Harness h;
  h.calibrate();
  const float before = h.det.output().threshold;

  Config c = h.det.config();
  c.k = 10.0f;
  c.minMargin = 5.0f;
  h.det.setConfig(c);

  TEST_ASSERT_TRUE(h.det.output().threshold > before);
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Running), static_cast<int>(h.det.output().phase));
}

void test_changing_window_size_requires_recalibration() {
  Harness h;
  h.calibrate();
  Config c = h.det.config();
  c.windowSize = 30;
  h.det.setConfig(c);
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Uncalibrated), static_cast<int>(h.det.output().phase));
}

void test_window_size_is_clamped() {
  Config c;
  c.windowSize = 1000;
  Detector d(c);
  TEST_ASSERT_EQUAL_UINT16(kMaxWindow, d.config().windowSize);
}

void test_threshold_is_never_below_noise_seen_in_calibration() {
  // Mostly quiet, with occasional spikes: window variance is heavy tailed, so
  // mean + k*sigma alone would land below the largest variance calibration saw.
  Harness h;
  h.det.startCalibration();
  float maxSeen = 0;
  for (int i = 0; i < 20 + 200 + 5; i++) {
    h.now += kStepMs;
    const int amp = (i % 40 == 0) ? 5 : 1;
    const Output& o = h.det.update(-60 + h.rng.range(-amp, amp), h.now);
    if (o.phase == Phase::Calibrating && o.variance > maxSeen) maxSeen = o.variance;
  }
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Running), static_cast<int>(h.det.output().phase));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, maxSeen, h.det.output().baselineMax);
  TEST_ASSERT_TRUE(h.det.output().threshold >= maxSeen * h.det.config().ceilingSafety - 0.001f);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_threshold_is_never_below_noise_seen_in_calibration);
  RUN_TEST(test_starts_uncalibrated_and_ignores_samples);
  RUN_TEST(test_constant_signal_has_zero_variance);
  RUN_TEST(test_calibration_progress_and_completion);
  RUN_TEST(test_quiet_room_after_calibration_does_not_trigger);
  RUN_TEST(test_movement_triggers_motion_and_presence);
  RUN_TEST(test_motion_clears_but_presence_holds_until_timeout);
  RUN_TEST(test_invalid_samples_are_ignored);
  RUN_TEST(test_recalibration_resets_state);
  RUN_TEST(test_threshold_can_be_tuned_live);
  RUN_TEST(test_changing_window_size_requires_recalibration);
  RUN_TEST(test_window_size_is_clamped);
  return UNITY_END();
}
