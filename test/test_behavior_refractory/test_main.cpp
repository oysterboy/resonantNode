/*
ResonantBehavior on the host: can a node answer the reply to its own signal?

Step 5d of roadmap-0-steps.md (docs/research/unproven-assumptions-audit.md
section 1.1). Two nodes A and B on the shipped BehaviorGateConfig: A answers
a heard pattern with its hop train, B answers that train, and B's reply
closes on A's detector about 300-370 ms after A's own train ended:

    A's train ends                      t0
    B closes it                         + 10..50 ms   (release debounce, room tail)
    B waits                             + 100 ms      (waitAfterHeardMs)
    B's train plays                     + 165 ms      (25 ms x 5, gap 10; ResonantNodeApp hop defaults)
    A closes B's train                  + 10..50 ms
    A's verdict on the reply            t0 + 285..365 ms

A's refractory after its own emission is refractoryAfterEmitMs = 400, and a
verdict that arrives in Refractory is dropped (RefractoryAfterEmit), not
queued. These tests pin that arithmetic down so the bench run of step 5d
(nodepair.py with refractory 0 vs 400) has a stated expectation, and so a
later change to the defaults, the train shape or the state machine shows
up here before a board sees it.

The host clock does not matter: Behavior takes `now` as an argument.
Random idle scheduling is switched off (idleEnabled = false) so only the
heard-pattern path runs. Run with `pio test -e native-test-behavior`.
*/
#include <unity.h>

#include <initializer_list>

#include "../../src/behavior/BehaviorProfile.h"
#include "../../src/behavior/ResonantBehavior.h"
#include "../../src/detection/evaluation/OccurrenceVerdict.h"

// The native env does not compile src/behavior/ (the Node's loop is not a
// host target); the test pulls the one translation unit it needs.
#include "../../src/behavior/ResonantBehavior.cpp"

namespace {

// The network signal as the Node plays it (ResonantNodeApp.h hop defaults):
// 5 pulses of 25 ms with 10 ms gaps, 165 ms on air.
constexpr unsigned long kHopSteps = 5;
constexpr unsigned long kHopPulseMs = 25;
constexpr unsigned long kHopGapMs = 10;
constexpr unsigned long kTrainMs = kHopSteps * kHopPulseMs + (kHopSteps - 1) * kHopGapMs;
// How long after the sound stops the listener's scalar detector closes the
// occurrence and the verdict reaches Behavior: release debounce 10 ms plus
// the room tail (lab notes 2026-10-15/16: 10-50 ms). The worst case for the
// refractory question is the fastest close.
constexpr unsigned long kCloseFastMs = 10;
constexpr unsigned long kCloseSlowMs = 50;

detection::OccurrenceVerdict validPattern(unsigned long heardAtMs) {
    detection::OccurrenceVerdict v;
    v.type = detection::VerdictType::SinglePulse;
    v.accepted = true;
    v.proposalMatched = true;
    v.supportMatched = true;
    v.valid = true;
    v.primaryStartMs = heardAtMs > kTrainMs ? heardAtMs - kTrainMs : 0;
    v.primaryHeardAtMs = heardAtMs;
    v.primaryAcceptedMs = heardAtMs;
    v.primaryDurationMs = kTrainMs;
    v.primaryStrength = 9000.0f;
    return v;
}

BehaviorGateConfig shippedDefaults() {
    BehaviorGateConfig cfg;  // BehaviorProfile.h as compiled
    cfg.idleEnabled = false;  // keep the idle voice out of the timeline
    cfg.levelMax = 0;  // the refractory alone: no level draws, no jitter (BEH-008's own suite is test_behavior_level)
    return cfg;
}

// Drives node A through one answer: a heard pattern at t = heardAt, the
// wait, its own train, and the chirp-finished edge. Returns the time A's
// train ended.
unsigned long answerOnce(ResonantBehavior& a, unsigned long heardAt) {
    const ResonantBehavior::BehaviorDecision d = a.handleOccurrenceVerdict(validPattern(heardAt), heardAt);
    TEST_ASSERT_EQUAL(ResonantBehavior::BehaviorDecision::ConsumedPattern, d);
    a.update(heardAt);
    TEST_ASSERT_FALSE(a.shouldStartChirp());
    const unsigned long emitAt = heardAt + a.config().waitAfterHeardMs;
    a.update(emitAt);
    TEST_ASSERT_TRUE(a.shouldStartChirp());
    TEST_ASSERT_EQUAL(ResonantBehavior::SignalShape::NetworkSignal, a.signalShape());
    a.notifyChirpStarted(emitAt, kTrainMs);
    const unsigned long trainEnd = emitAt + kTrainMs;
    a.update(trainEnd);
    a.notifyChirpFinished(trainEnd);
    return trainEnd;
}

// When B's reply lands on A, measured from the end of A's train.
unsigned long replyVerdictDelayMs(const BehaviorGateConfig& cfgB, unsigned long closeMs) {
    return closeMs + cfgB.waitAfterHeardMs + kTrainMs + closeMs;
}

}  // namespace

void setUp() {}
void tearDown() {}

// The reply timeline from the shipped values lands inside the refractory
// for both the fastest and the slowest close.
void test_reply_arrives_inside_the_shipped_refractory() {
    const BehaviorGateConfig cfg = shippedDefaults();
    const unsigned long fast = replyVerdictDelayMs(cfg, kCloseFastMs);
    const unsigned long slow = replyVerdictDelayMs(cfg, kCloseSlowMs);
    TEST_ASSERT_TRUE_MESSAGE(fast < cfg.refractoryAfterEmitMs,
                             "fastest reply is after the refractory: chain can continue");
    TEST_ASSERT_TRUE_MESSAGE(slow < cfg.refractoryAfterEmitMs,
                             "slowest reply is after the refractory: chain can continue");
}

// Node A, shipped defaults: B's reply verdict is dropped as RefractoryAfterEmit.
void test_reply_inside_refractory_is_dropped_on_shipped_defaults() {
    ResonantBehavior a;
    a.resetState();
    a.configure(shippedDefaults());
    const unsigned long trainEnd = answerOnce(a, 1000);
    for (unsigned long closeMs : {kCloseFastMs, kCloseSlowMs}) {
        const unsigned long replyAt = trainEnd + replyVerdictDelayMs(shippedDefaults(), closeMs);
        a.update(replyAt);
        const ResonantBehavior::BehaviorDecision d = a.handleOccurrenceVerdict(validPattern(replyAt), replyAt);
        TEST_ASSERT_EQUAL(ResonantBehavior::BehaviorDecision::RefractoryAfterEmit, d);
        TEST_ASSERT_FALSE(a.behaviorEligible());
    }
    TEST_ASSERT_EQUAL_UINT32(2, a.blockedRefractory());
    // And the reply is gone: once the refractory ends nothing is pending.
    a.update(trainEnd + shippedDefaults().refractoryAfterEmitMs + 1);
    TEST_ASSERT_FALSE(a.shouldStartChirp());
}

// The same reply one millisecond after the refractory ends is consumed:
// the boundary is the constant, not the state machine.
void test_reply_after_refractory_is_consumed() {
    ResonantBehavior a;
    a.resetState();
    a.configure(shippedDefaults());
    const unsigned long trainEnd = answerOnce(a, 1000);
    const unsigned long replyAt = trainEnd + shippedDefaults().refractoryAfterEmitMs + 1;
    a.update(replyAt);
    const ResonantBehavior::BehaviorDecision d = a.handleOccurrenceVerdict(validPattern(replyAt), replyAt);
    TEST_ASSERT_EQUAL(ResonantBehavior::BehaviorDecision::ConsumedPattern, d);
}

// With refractory 0 (the 2026-05-26 slow-circle value) the same reply is
// consumed and A answers again: the chain can continue.
void test_reply_is_consumed_with_refractory_zero() {
    BehaviorGateConfig cfg = shippedDefaults();
    cfg.refractoryAfterEmitMs = 0;
    ResonantBehavior a;
    a.resetState();
    a.configure(cfg);
    const unsigned long trainEnd = answerOnce(a, 1000);
    const unsigned long replyAt = trainEnd + replyVerdictDelayMs(cfg, kCloseFastMs);
    a.update(replyAt);
    const ResonantBehavior::BehaviorDecision d = a.handleOccurrenceVerdict(validPattern(replyAt), replyAt);
    TEST_ASSERT_EQUAL(ResonantBehavior::BehaviorDecision::ConsumedPattern, d);
    a.update(replyAt + cfg.waitAfterHeardMs);
    TEST_ASSERT_TRUE(a.shouldStartChirp());
    TEST_ASSERT_EQUAL_UINT32(2, a.wouldEmitCount());
}

// A verdict while A's own train is still sounding is OutputBusy, and the
// own-emit suppression covers the whole train plus the tail.
void test_own_train_keeps_detection_suppressed_until_the_tail_ends() {
    ResonantBehavior a;
    a.resetState();
    a.configure(shippedDefaults());
    const unsigned long heardAt = 1000;
    const unsigned long emitAt = heardAt + shippedDefaults().waitAfterHeardMs;
    a.handleOccurrenceVerdict(validPattern(heardAt), heardAt);
    a.update(heardAt);
    a.update(emitAt);
    a.notifyChirpStarted(emitAt, kTrainMs);
    TEST_ASSERT_TRUE(a.ownEmitDetectionSuppressed(emitAt + kTrainMs - 1));
    const ResonantBehavior::BehaviorDecision d = a.handleOccurrenceVerdict(validPattern(emitAt + 50), emitAt + 50);
    TEST_ASSERT_EQUAL(ResonantBehavior::BehaviorDecision::OutputBusy, d);
    const unsigned long trainEnd = emitAt + kTrainMs;
    a.notifyChirpFinished(trainEnd);
    TEST_ASSERT_TRUE(a.ownEmitDetectionSuppressed(trainEnd + shippedDefaults().detectionSuppressTailMsOwnEmit - 1));
    TEST_ASSERT_FALSE(a.ownEmitDetectionSuppressed(trainEnd + shippedDefaults().detectionSuppressTailMsOwnEmit + 1));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_reply_arrives_inside_the_shipped_refractory);
    RUN_TEST(test_reply_inside_refractory_is_dropped_on_shipped_defaults);
    RUN_TEST(test_reply_after_refractory_is_consumed);
    RUN_TEST(test_reply_is_consumed_with_refractory_zero);
    RUN_TEST(test_own_train_keeps_detection_suppressed_until_the_tail_ends);
    return UNITY_END();
}
