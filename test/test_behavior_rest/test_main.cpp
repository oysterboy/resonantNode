/*
ResonantBehavior on the host: a busy network makes a node rest.

decisions/2026-10-17-two-node-exchange-chain-length.md (owner 2026-10-17): above a
level of network traffic the node pauses hard, no answers and no idle voice, for
restMs. Two triggers: the field (valid network patterns heard in the field
window, FieldState::recentPatternCount) and the node's own budget (emissions
inside ownEmitBudgetWindowMs). Same values on every node. On the bench two nodes
answered each other 518 times in 180 s (bench/sessions/2026-10-17-pairmatrix);
these tests pin down that the rule cuts such a chain.

Run with `pio test -e native-test-behavior`.
*/
#include <unity.h>

#include "../../src/behavior/BehaviorProfile.h"
#include "../../src/behavior/ResonantBehavior.h"
#include "../../src/detection/evaluation/OccurrenceVerdict.h"
#include "../../src/detection/field/FieldState.h"

#include "../../src/behavior/ResonantBehavior.cpp"

namespace {

constexpr unsigned long kTrainMs = 165;  // the Node's hop train, 25 ms x 5, gap 10

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

detection::FieldState fieldWith(unsigned long patterns) {
    detection::FieldState f;
    f.recentPatternCount = patterns;
    return f;
}

BehaviorGateConfig config() {
    BehaviorGateConfig cfg;  // shipped defaults, as compiled
    cfg.idleEnabled = false;
    cfg.refractoryAfterEmitMs = 0;  // isolate the rest rule from the refractory question (step 5d)
    return cfg;
}

// One heard pattern answered: verdict, wait, emission, finish. Returns the end of the emission.
unsigned long answer(ResonantBehavior& b, unsigned long heardAt, unsigned long fieldPatterns) {
    TEST_ASSERT_EQUAL(ResonantBehavior::BehaviorDecision::ConsumedPattern,
                      b.handleOccurrenceVerdict(validPattern(heardAt), fieldWith(fieldPatterns), heardAt));
    b.update(heardAt);
    const unsigned long emitAt = heardAt + b.config().waitAfterHeardMs;
    b.update(emitAt);
    TEST_ASSERT_TRUE(b.shouldStartChirp());
    b.notifyChirpStarted(emitAt, kTrainMs);
    const unsigned long end = emitAt + kTrainMs;
    b.update(end);
    b.notifyChirpFinished(end);
    b.update(end + 1);
    return end;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_shipped_defaults_have_both_triggers_on() {
    const BehaviorGateConfig cfg;
    TEST_ASSERT_TRUE(cfg.restAfterFieldPatterns > 0);
    TEST_ASSERT_TRUE(cfg.ownEmitBudget > 0);
    TEST_ASSERT_TRUE(cfg.restMs > 0);
}

// Below the field threshold the node answers; at it, it rests instead.
void test_dense_field_starts_a_rest_instead_of_an_answer() {
    ResonantBehavior b;
    b.configure(config());
    b.resetState();
    const unsigned long threshold = b.config().restAfterFieldPatterns;
    answer(b, 1000, threshold - 1);
    const unsigned long t = 3000;
    TEST_ASSERT_EQUAL(ResonantBehavior::BehaviorDecision::RestFieldDense,
                      b.handleOccurrenceVerdict(validPattern(t), fieldWith(threshold), t));
    TEST_ASSERT_TRUE(b.resting(t));
    TEST_ASSERT_EQUAL_UINT32(1, b.restsEntered());
    b.update(t + b.config().waitAfterHeardMs);
    TEST_ASSERT_FALSE(b.shouldStartChirp());
}

// While resting every valid verdict is ignored, whatever the field says; after restMs the node answers again.
void test_rest_ignores_verdicts_until_it_ends() {
    ResonantBehavior b;
    b.configure(config());
    b.resetState();
    const unsigned long t0 = 2000;
    b.handleOccurrenceVerdict(validPattern(t0), fieldWith(b.config().restAfterFieldPatterns), t0);
    TEST_ASSERT_TRUE(b.resting(t0));
    const unsigned long mid = t0 + b.config().restMs / 2;
    TEST_ASSERT_EQUAL(ResonantBehavior::BehaviorDecision::Resting,
                      b.handleOccurrenceVerdict(validPattern(mid), fieldWith(0), mid));
    const unsigned long after = t0 + b.config().restMs + 1;
    TEST_ASSERT_FALSE(b.resting(after));
    answer(b, after, 0);
    TEST_ASSERT_EQUAL_UINT32(1, b.blockedResting() >= 1 ? 1 : 0);
}

// A node that hears only a thin field but keeps answering hits its own budget and rests.
void test_own_budget_starts_a_rest() {
    ResonantBehavior b;
    b.configure(config());
    b.resetState();
    const unsigned long budget = b.config().ownEmitBudget;
    unsigned long t = 1000;
    for (unsigned long i = 0; i < budget; ++i) {
        answer(b, t, 0);
        t += 700;  // the bench ping-pong: a turn every ~350 ms per pair, ~700 ms per node
    }
    TEST_ASSERT_TRUE(b.resting(t));
    TEST_ASSERT_EQUAL(ResonantBehavior::BehaviorDecision::RestOwnBudget, b.lastRestReason());
    TEST_ASSERT_EQUAL(ResonantBehavior::BehaviorDecision::Resting,
                      b.handleOccurrenceVerdict(validPattern(t), fieldWith(0), t));
}

// The budget counts only emissions inside its window: slow traffic never trips it.
void test_slow_traffic_stays_under_the_budget() {
    ResonantBehavior b;
    b.configure(config());
    b.resetState();
    unsigned long t = 1000;
    for (int i = 0; i < 20; ++i) {
        answer(b, t, 0);
        t += b.config().ownEmitBudgetWindowMs / (b.config().ownEmitBudget - 1) + 100;
    }
    TEST_ASSERT_EQUAL_UINT32(0, b.restsEntered());
}

// A bench-like ping-pong between two nodes ends: each hears the other's answer, the
// field fills (one pattern per turn in the ~4 s window), and one of them rests.
void test_two_node_ping_pong_is_cut() {
    ResonantBehavior a;
    ResonantBehavior b;
    a.configure(config());
    b.configure(config());
    a.resetState();
    b.resetState();
    unsigned long t = 1000;
    unsigned long heardA = 0;
    unsigned long heardB = 0;
    int turns = 0;
    ResonantBehavior* listener = &b;
    unsigned long* heard = &heardB;
    for (; turns < 100; ++turns) {
        ++*heard;  // the listener's field window gains the pattern it just heard
        listener->update(t);  // the Node's loop runs Behavior every pass (Refractory -> Idle)
        const ResonantBehavior::BehaviorDecision d =
            listener->handleOccurrenceVerdict(validPattern(t), fieldWith(*heard), t);
        if (d != ResonantBehavior::BehaviorDecision::ConsumedPattern) {
            break;
        }
        listener->update(t);
        const unsigned long emitAt = t + listener->config().waitAfterHeardMs;
        listener->update(emitAt);
        TEST_ASSERT_TRUE(listener->shouldStartChirp());
        listener->notifyChirpStarted(emitAt, kTrainMs);
        listener->update(emitAt + kTrainMs);
        listener->notifyChirpFinished(emitAt + kTrainMs);
        t = emitAt + kTrainMs + 30;  // the other node closes the train
        if (listener == &b) {
            listener = &a;
            heard = &heardA;
        } else {
            listener = &b;
            heard = &heardB;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(turns < 12, "the exchange should stop within a few turns");
    TEST_ASSERT_TRUE(a.restsEntered() + b.restsEntered() >= 1);
}

// Idle is silent while resting.
void test_no_idle_while_resting() {
    ResonantBehavior b;
    BehaviorGateConfig cfg = config();
    cfg.idleEnabled = true;
    cfg.idleTimeoutMs = 500;
    cfg.idleTimeVariationMs = 0;
    cfg.idleBlockedAfterHeardMs = 0;
    cfg.idleBlockedAfterOwnEmitMs = 0;
    b.configure(cfg);
    b.resetState();
    b.seedIdleSchedule(0);
    const unsigned long t0 = 100;
    b.handleOccurrenceVerdict(validPattern(t0), fieldWith(b.config().restAfterFieldPatterns), t0);
    TEST_ASSERT_TRUE(b.resting(t0));
    for (unsigned long t = t0; t < t0 + b.config().restMs; t += 50) {
        b.update(t);
        TEST_ASSERT_FALSE_MESSAGE(b.shouldStartChirp(), "idle chirp during a rest");
    }
    bool idled = false;
    for (unsigned long t = t0 + b.config().restMs + 1; t < t0 + b.config().restMs + 3000 && !idled; t += 50) {
        b.update(t);
        idled = b.shouldStartChirp();
    }
    TEST_ASSERT_TRUE_MESSAGE(idled, "idle voice back after the rest");
}

// Step 10e: a live idle_timeout change applies to the wait already running, from when it started.
bool idlesBy(ResonantBehavior& b, unsigned long from, unsigned long until) {
    for (unsigned long t = from; t <= until; t += 10) {
        b.update(t);
        if (b.shouldStartChirp()) {
            return true;
        }
    }
    return false;
}

void test_shorter_idle_timeout_applies_to_the_running_wait() {
    ResonantBehavior b;
    BehaviorGateConfig cfg = config();
    cfg.idleEnabled = true;
    cfg.idleTimeoutMs = 60000;
    cfg.idleTimeVariationMs = 0;
    cfg.idleBlockedAfterHeardMs = 0;
    cfg.idleBlockedAfterOwnEmitMs = 0;
    b.configure(cfg);
    b.resetState();
    b.seedIdleSchedule(0);
    TEST_ASSERT_FALSE(idlesBy(b, 0, 1000));
    cfg.idleTimeoutMs = 2000;  // PARAM SET behavior.idle_timeout_ms 2000 at t = 1 s
    b.configure(cfg);
    TEST_ASSERT_FALSE(idlesBy(b, 1010, 1990));
    TEST_ASSERT_TRUE_MESSAGE(idlesBy(b, 2000, 2100), "the idle call comes 2 s after the wait started, not at 60 s");
}

void test_longer_idle_timeout_extends_the_running_wait_and_same_value_keeps_it() {
    ResonantBehavior b;
    BehaviorGateConfig cfg = config();
    cfg.idleEnabled = true;
    cfg.idleTimeoutMs = 2000;
    cfg.idleTimeVariationMs = 0;
    cfg.idleBlockedAfterHeardMs = 0;
    cfg.idleBlockedAfterOwnEmitMs = 0;
    b.configure(cfg);
    b.resetState();
    b.seedIdleSchedule(0);
    b.configure(cfg);  // an unrelated PARAM SET re-applies the same values: the wait stays
    TEST_ASSERT_FALSE(idlesBy(b, 0, 1000));
    cfg.idleTimeoutMs = 5000;
    b.configure(cfg);
    TEST_ASSERT_FALSE(idlesBy(b, 1010, 4990));
    TEST_ASSERT_TRUE(idlesBy(b, 5000, 5100));
}

// RB CALL (step 5g): the operator call plays the network signal, and is not taken while resting.
void test_network_call_plays_the_network_signal() {
    ResonantBehavior b;
    b.configure(config());
    b.resetState();
    b.requestNetworkCall();
    b.update(1000);
    TEST_ASSERT_TRUE(b.shouldStartChirp());
    TEST_ASSERT_EQUAL(ResonantBehavior::SignalShape::NetworkSignal, b.signalShape());
    TEST_ASSERT_EQUAL_STRING("call", b.chirpRequestSourceName());
}

void test_network_call_waits_while_resting() {
    ResonantBehavior b;
    b.configure(config());
    b.resetState();
    const unsigned long t0 = 1000;
    b.handleOccurrenceVerdict(validPattern(t0), fieldWith(b.config().restAfterFieldPatterns), t0);
    TEST_ASSERT_TRUE(b.resting(t0));
    b.requestNetworkCall();
    b.update(t0 + 100);
    TEST_ASSERT_FALSE(b.shouldStartChirp());
    TEST_ASSERT_TRUE(b.networkCallPending());
    b.update(t0 + b.config().restMs + 1);
    TEST_ASSERT_TRUE(b.shouldStartChirp());
}

// Both triggers off: the old behaviour, no rest.
void test_triggers_off_means_no_rest() {
    ResonantBehavior b;
    BehaviorGateConfig cfg = config();
    cfg.restAfterFieldPatterns = 0;
    cfg.ownEmitBudget = 0;
    b.configure(cfg);
    b.resetState();
    unsigned long t = 1000;
    for (int i = 0; i < 12; ++i) {
        answer(b, t, 50);
        t += 400;
    }
    TEST_ASSERT_EQUAL_UINT32(0, b.restsEntered());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_shipped_defaults_have_both_triggers_on);
    RUN_TEST(test_dense_field_starts_a_rest_instead_of_an_answer);
    RUN_TEST(test_rest_ignores_verdicts_until_it_ends);
    RUN_TEST(test_own_budget_starts_a_rest);
    RUN_TEST(test_slow_traffic_stays_under_the_budget);
    RUN_TEST(test_two_node_ping_pong_is_cut);
    RUN_TEST(test_no_idle_while_resting);
    RUN_TEST(test_shorter_idle_timeout_applies_to_the_running_wait);
    RUN_TEST(test_longer_idle_timeout_extends_the_running_wait_and_same_value_keeps_it);
    RUN_TEST(test_triggers_off_means_no_rest);
    RUN_TEST(test_network_call_plays_the_network_signal);
    RUN_TEST(test_network_call_waits_while_resting);
    return UNITY_END();
}
