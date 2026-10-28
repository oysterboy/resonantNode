/*
ResonantBehavior's snapshot and outputs gate on the host (step 11i N6, review
A9, docs/refactors/node-split.md). status(now) copies exactly what the getters
return (the Node prints the snapshot in place of them), and
setOutputsEnabled(false) does what the Node's gate did around Behavior: a
verdict is only noted as heard (decision None, no HeardPattern, the field
state not taken), update() does nothing, shouldStartChirp() is false; a reset
keeps the gate. Runs in native-test-behavior.
*/
#include <unity.h>


#include "../../src/behavior/BehaviorProfile.h"
#include "../../src/behavior/ResonantBehavior.h"
#include "../../src/detection/evaluation/OccurrenceVerdict.h"

#include "../../src/behavior/ResonantBehavior.cpp"

namespace {

using Decision = ResonantBehavior::BehaviorDecision;

detection::OccurrenceVerdict validPattern(unsigned long heardAtMs) {
    detection::OccurrenceVerdict v;
    v.type = detection::VerdictType::SinglePulse;
    v.accepted = true;
    v.proposalMatched = true;
    v.supportMatched = true;
    v.valid = true;
    v.primaryStartMs = heardAtMs - 165;
    v.primaryHeardAtMs = heardAtMs;
    v.primaryAcceptedMs = heardAtMs;
    v.primaryDurationMs = 165;
    v.primaryStrength = 9000.0f;
    return v;
}

ResonantBehavior configured() {
    ResonantBehavior b;
    BehaviorGateConfig cfg;
    cfg.idleEnabled = false;
    b.configure(cfg);
    b.resetState();
    return b;
}

void assertSnapshotMatchesGetters(const ResonantBehavior& b, unsigned long now) {
    const BehaviorStatus s = b.status(now);
    TEST_ASSERT_EQUAL_UINT32(now, s.nowMs);
    TEST_ASSERT_EQUAL_STRING(b.stateName(), s.stateName);
    TEST_ASSERT_EQUAL(b.state() == ResonantBehavior::State::Idle, s.idle);
    TEST_ASSERT_EQUAL(b.outputsEnabled(), s.outputsEnabled);
    TEST_ASSERT_EQUAL(b.outputBusy(), s.outputBusy);
    TEST_ASSERT_EQUAL(b.behaviorEligible(), s.behaviorEligible);
    TEST_ASSERT_EQUAL(b.config().idleEnabled, s.config.idleEnabled);
    TEST_ASSERT_EQUAL_UINT32(b.config().waitAfterHeardMs, s.config.waitAfterHeardMs);
    TEST_ASSERT_EQUAL_UINT32(b.config().refractoryAfterEmitMs, s.config.refractoryAfterEmitMs);
    TEST_ASSERT_EQUAL_UINT32(b.config().detectionSuppressTailMsOwnEmit, s.config.detectionSuppressTailMsOwnEmit);
    TEST_ASSERT_EQUAL_UINT32(b.config().idleBlockedAfterOwnEmitMs, s.config.idleBlockedAfterOwnEmitMs);
    TEST_ASSERT_EQUAL_STRING(b.lastDecisionName(), s.lastDecisionName);
    TEST_ASSERT_EQUAL_STRING(b.lastBlockReasonName(), s.lastBlockReasonName);
    TEST_ASSERT_EQUAL_STRING(b.lastVerdictTypeName(), s.lastVerdictTypeName);
    TEST_ASSERT_EQUAL_UINT32(b.lastHeardMs(), s.lastHeardMs);
    TEST_ASSERT_EQUAL_UINT32(b.lastEmitMs(), s.lastEmitMs);
    TEST_ASSERT_EQUAL_UINT32(b.waitUntilMs(), s.waitUntilMs);
    TEST_ASSERT_EQUAL_UINT32(b.refractoryUntilMs(), s.refractoryUntilMs);
    TEST_ASSERT_EQUAL_UINT32(b.waitRemainingMs(now), s.waitRemainingMs);
    TEST_ASSERT_EQUAL_UINT32(b.refractoryRemainingMs(now), s.refractoryRemainingMs);
    TEST_ASSERT_EQUAL_UINT32(b.behaviorSuppressRemainingMs(now), s.behaviorSuppressRemainingMs);
    TEST_ASSERT_EQUAL(b.ownEmitSuppressArmed(), s.ownEmitSuppressArmed);
    TEST_ASSERT_EQUAL_UINT32(b.ownEmitSuppressUntilMs(), s.ownEmitSuppressUntilMs);
    TEST_ASSERT_EQUAL_UINT32(b.ownEmitDetectionSuppressUntilMs(), s.ownEmitSuppressUntilMs);
    TEST_ASSERT_EQUAL(b.resting(now), s.resting);
    TEST_ASSERT_EQUAL_UINT32(b.restUntilMs(), s.restUntilMs);
    TEST_ASSERT_EQUAL_UINT32(b.restsEntered(), s.restsEntered);
    TEST_ASSERT_EQUAL_STRING(ResonantBehavior::behaviorDecisionName(b.lastRestReason()), s.lastRestReasonName);
    TEST_ASSERT_EQUAL_UINT32(b.blockedResting(), s.blockedResting);
    TEST_ASSERT_EQUAL_UINT32(b.patternsReceived(), s.patternsReceived);
    TEST_ASSERT_EQUAL_UINT32(b.patternsIgnoredInvalid(), s.patternsIgnoredInvalid);
    TEST_ASSERT_EQUAL_UINT32(b.blockedOutputBusy(), s.blockedOutputBusy);
    TEST_ASSERT_EQUAL_UINT32(b.blockedRefractory(), s.blockedRefractory);
    TEST_ASSERT_EQUAL_UINT32(b.blockedWaiting(), s.blockedWaiting);
    TEST_ASSERT_EQUAL_UINT32(b.blockedSelfSuppressed(), s.blockedSelfSuppressed);
    TEST_ASSERT_EQUAL_UINT32(b.wouldEmitCount(), s.wouldEmitCount);
    TEST_ASSERT_EQUAL_UINT32(b.emittedCount(), s.emittedCount);
}

void test_snapshot_matches_getters_through_an_answer() {
    ResonantBehavior b = configured();
    assertSnapshotMatchesGetters(b, 1000);
    TEST_ASSERT_EQUAL_STRING("Idle", b.status(1000).stateName);

    TEST_ASSERT_EQUAL(Decision::ConsumedPattern, b.handleOccurrenceVerdict(validPattern(1000), 1000));
    b.update(1000);
    assertSnapshotMatchesGetters(b, 1050);  // waiting: wait remaining at 1050
    TEST_ASSERT_EQUAL_UINT32(50, b.status(1050).waitRemainingMs);
    TEST_ASSERT_FALSE(b.status(1050).idle);

    b.update(1100);
    TEST_ASSERT_TRUE(b.shouldStartChirp());
    b.notifyChirpStarted(1100, 165);
    assertSnapshotMatchesGetters(b, 1150);
    TEST_ASSERT_TRUE(b.status(1150).outputBusy);
    TEST_ASSERT_EQUAL_STRING("Chirping", b.status(1150).stateName);

    b.notifyChirpFinished(1265);
    assertSnapshotMatchesGetters(b, 1300);  // refractory
    // A verdict in the refractory: blocked, counted.
    b.handleOccurrenceVerdict(validPattern(1350), 1350);
    assertSnapshotMatchesGetters(b, 1360);
    TEST_ASSERT_EQUAL_UINT32(2, b.status(1360).patternsReceived);
}

void test_outputs_off_notes_heard_and_decides_nothing() {
    ResonantBehavior b = configured();
    b.setOutputsEnabled(false);
    TEST_ASSERT_FALSE(b.status(500).outputsEnabled);
    TEST_ASSERT_EQUAL(Decision::None, b.handleOccurrenceVerdict(validPattern(1000), detection::FieldState{}, 1000));
    TEST_ASSERT_EQUAL(Decision::None, b.handleOccurrenceVerdict(validPattern(1010), 1010));
    // Not received, not in HeardPattern; only the heard time moved (idleBlockedAfterHeardMs).
    TEST_ASSERT_EQUAL_UINT32(0, b.patternsReceived());
    TEST_ASSERT_EQUAL(ResonantBehavior::State::Idle, b.state());
    b.update(1200);
    TEST_ASSERT_FALSE(b.shouldStartChirp());
    // A queued call waits while off: update() does nothing, nothing starts.
    b.requestNetworkCall();
    b.update(1300);
    TEST_ASSERT_FALSE(b.shouldStartChirp());
    TEST_ASSERT_TRUE(b.networkCallPending());

    // A reset keeps the gate; on again, the queued call is taken.
    b.resetState();
    TEST_ASSERT_FALSE(b.outputsEnabled());
    b.setOutputsEnabled(true);
    b.requestNetworkCall();
    b.update(1400);
    TEST_ASSERT_TRUE(b.shouldStartChirp());
}

void test_outputs_on_by_default() {
    ResonantBehavior b = configured();
    TEST_ASSERT_TRUE(b.outputsEnabled());
    TEST_ASSERT_EQUAL(Decision::ConsumedPattern, b.handleOccurrenceVerdict(validPattern(1000), 1000));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_snapshot_matches_getters_through_an_answer);
    RUN_TEST(test_outputs_off_notes_heard_and_decides_nothing);
    RUN_TEST(test_outputs_on_by_default);
    return UNITY_END();
}
