#include <unity.h>

#include "ripple_calibrator.h"
#include "ripple_core.h"

using namespace ripple;

namespace {

struct Rng {
  uint32_t s;
  explicit Rng(uint32_t seed) : s(seed) {}
  uint32_t next() {
    s = s * 1664525u + 1013904223u;
    return s >> 8;
  }
  int range(int lo, int hi) { return lo + static_cast<int>(next() % (hi - lo + 1)); }
};

constexpr uint32_t kStepMs = 100;

struct Harness {
  Detector det;
  Calibrator cal;
  Rng rng{777};
  uint32_t now = 1000;

  int movingAmp = 6;
  int stillAmp = 1;  // a motionless person looks like an empty room
  int transitionAmp = 8;

  // One 10 Hz sample with the given jitter amplitude.
  void sample(int amp, bool deliver = true) {
    now += kStepMs;
    det.update(-60 + rng.range(-amp, amp), deliver, now);
    cal.update(now, deliver && det.ready(), det.output().variance);
  }

  int ampForStage() const {
    switch (cal.stage()) {
      case Calibrator::Stage::Moving: return movingAmp;
      case Calibrator::Stage::Still: return stillAmp;
      default: return 1;
    }
  }

  // Drive one full stage: user presses ready, walks into position (noisy
  // countdown), then performs the stage activity until it completes.
  void runStage() {
    TEST_ASSERT_EQUAL(static_cast<int>(Calibrator::State::WaitingUser),
                      static_cast<int>(cal.state()));
    const Calibrator::Stage stage = cal.stage();
    cal.ready(now);
    while (cal.state() == Calibrator::State::Countdown) sample(transitionAmp);
    int guard = 0;
    while (cal.state() == Calibrator::State::Recording && cal.stage() == stage) {
      sample(ampForStage());
      TEST_ASSERT_TRUE(++guard < 2000);
    }
  }

  void runAll() {
    cal.start();
    while (cal.state() == Calibrator::State::WaitingUser) runStage();
  }

  void applyAndFeed(int amp, int n) {
    for (int i = 0; i < n; i++) sample(amp);
  }
};

}  // namespace

void setUp() {}
void tearDown() {}

void test_wizard_walks_through_all_stages() {
  Harness h;
  TEST_ASSERT_EQUAL(static_cast<int>(Calibrator::State::Idle), static_cast<int>(h.cal.state()));
  h.cal.start();

  const Calibrator::Stage order[] = {Calibrator::Stage::EmptyBefore, Calibrator::Stage::Moving,
                                     Calibrator::Stage::Still, Calibrator::Stage::EmptyAfter};
  for (int i = 0; i < 4; i++) {
    TEST_ASSERT_EQUAL(static_cast<int>(order[i]), static_cast<int>(h.cal.stage()));
    h.runStage();
  }
  TEST_ASSERT_EQUAL(static_cast<int>(Calibrator::State::Done), static_cast<int>(h.cal.state()));
  TEST_ASSERT_TRUE(h.cal.result().valid);
}

void test_clear_separation_gives_good_quality() {
  Harness h;
  h.runAll();
  const Calibrator::Result& r = h.cal.result();

  TEST_ASSERT_EQUAL(static_cast<int>(Calibrator::Quality::Good), static_cast<int>(r.quality));
  TEST_ASSERT_TRUE(r.threshold > r.noiseCeiling);
  TEST_ASSERT_TRUE(r.movingCoverage > 0.9f);
  TEST_ASSERT_EQUAL_UINT16(0, r.holdoutFalseSeconds);
  TEST_ASSERT_FALSE(r.stillDetectable);  // still person == empty room here
}

void test_countdown_noise_does_not_pollute_baseline() {
  Harness h;
  h.runAll();
  // Empty room jitter is +-1 dB -> window variance stays small even though the
  // countdown was fed +-8 dB of noise.
  TEST_ASSERT_TRUE(h.cal.result().noiseCeiling < 3.0f);
}

void test_calibration_makes_detector_work() {
  Harness h;
  h.runAll();
  const Calibrator::Result r = h.cal.result();
  h.det.applyCalibration(r.baselineMean, r.baselineSigma, r.threshold);
  TEST_ASSERT_EQUAL(static_cast<int>(Phase::Running), static_cast<int>(h.det.output().phase));

  int quietMotion = 0;
  for (int i = 0; i < 600; i++) {
    h.now += kStepMs;
    if (h.det.update(-60 + h.rng.range(-1, 1), h.now).motion) quietMotion++;
  }
  TEST_ASSERT_EQUAL(0, quietMotion);

  bool sawMotion = false;
  for (int i = 0; i < 100; i++) {
    h.now += kStepMs;
    if (h.det.update(-60 + h.rng.range(-6, 6), h.now).motion) sawMotion = true;
  }
  TEST_ASSERT_TRUE(sawMotion);
}

void test_external_threshold_survives_config_changes() {
  Harness h;
  h.runAll();
  const Calibrator::Result r = h.cal.result();
  h.det.applyCalibration(r.baselineMean, r.baselineSigma, r.threshold);

  Config c = h.det.config();
  c.k = 1.0f;  // would lower an auto threshold; must not touch an external one
  h.det.setConfig(c);
  TEST_ASSERT_EQUAL_FLOAT(r.threshold, h.det.output().threshold);
}

void test_no_separation_gives_poor_quality() {
  Harness h;
  h.movingAmp = 1;  // walking is invisible on this link
  h.runAll();
  TEST_ASSERT_EQUAL(static_cast<int>(Calibrator::Quality::Poor),
                    static_cast<int>(h.cal.result().quality));
}

void test_visible_still_person_is_reported() {
  Harness h;
  h.stillAmp = 6;
  h.runAll();
  TEST_ASSERT_TRUE(h.cal.result().stillDetectable);
}

void test_cancel_returns_to_idle() {
  Harness h;
  h.cal.start();
  h.cal.ready(h.now);
  h.cal.cancel();
  TEST_ASSERT_EQUAL(static_cast<int>(Calibrator::State::Idle), static_cast<int>(h.cal.state()));
  TEST_ASSERT_FALSE(h.cal.result().valid);
}

void test_ready_is_ignored_unless_waiting() {
  Harness h;
  h.cal.ready(h.now);  // idle: no effect
  TEST_ASSERT_EQUAL(static_cast<int>(Calibrator::State::Idle), static_cast<int>(h.cal.state()));
}

void test_countdown_reports_seconds_left() {
  Harness h;
  h.cal.start();
  h.cal.ready(h.now);
  TEST_ASSERT_EQUAL_UINT32(10, h.cal.secondsLeft(h.now));
  TEST_ASSERT_EQUAL_UINT32(5, h.cal.secondsLeft(h.now + 5000));
}

void test_stage_fails_without_samples() {
  Harness h;
  h.cal.start();
  h.cal.ready(h.now);
  while (h.cal.state() == Calibrator::State::Countdown) h.sample(1);
  // WiFi dropped for the whole recording: nothing delivered.
  int guard = 0;
  while (h.cal.state() == Calibrator::State::Recording) {
    h.sample(1, false);
    TEST_ASSERT_TRUE(++guard < 2000);
  }
  TEST_ASSERT_EQUAL(static_cast<int>(Calibrator::State::Failed), static_cast<int>(h.cal.state()));
}

void test_survives_millis_wraparound() {
  Harness h;
  h.now = 0xFFFFFFFFu - 15000;  // clock wraps during the run
  h.runAll();
  TEST_ASSERT_EQUAL(static_cast<int>(Calibrator::Quality::Good),
                    static_cast<int>(h.cal.result().quality));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_wizard_walks_through_all_stages);
  RUN_TEST(test_clear_separation_gives_good_quality);
  RUN_TEST(test_countdown_noise_does_not_pollute_baseline);
  RUN_TEST(test_calibration_makes_detector_work);
  RUN_TEST(test_external_threshold_survives_config_changes);
  RUN_TEST(test_no_separation_gives_poor_quality);
  RUN_TEST(test_visible_still_person_is_reported);
  RUN_TEST(test_cancel_returns_to_idle);
  RUN_TEST(test_ready_is_ignored_unless_waiting);
  RUN_TEST(test_countdown_reports_seconds_left);
  RUN_TEST(test_stage_fails_without_samples);
  RUN_TEST(test_survives_millis_wraparound);
  return UNITY_END();
}
