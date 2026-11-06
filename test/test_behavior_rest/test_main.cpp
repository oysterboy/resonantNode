/*
ResonantBehavior on the host: a busy network makes a node rest.

decisions/2026-10-17-two-node-exchange-chain-length.md: above a level of network traffic the node pauses hard, no
answers and no idle voice, for restMs. Since 2026-11-06 the one trigger is the level automaton (levelMax; the field
and own-budget triggers were retired, decisions/2026-11-06-retire-the-old-rest-triggers.md). On the bench two nodes
answered each other 518 times in 180 s (bench/sessions/2026-10-17-pairmatrix); these tests pin down that the rule
cuts such a chain and what a rest does (drops a scheduled answer, silences idle, holds a call).

Run with `pio test -e native-test-behavior`.
*/
#include <unity.h>

#include "../../src/behavior/BehaviorProfile.h"
#include "../../src/behavior/ResonantBehavior.h"
#include "../../src/detection/evaluation/OccurrenceVerdict.h"
#include "../../src/detection/field/FieldState.h"

#include "../../src/behavior/ResonantBehavior.cpp"

namespace {

constexpr unsigned long kTrainMs = 165;  // one hop train

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

detection::FieldState quietField() {
    return detection::FieldState();
}

// Every valid train answers (a start chance of 100 %), no spread; the rest comes at the top of the level.
BehaviorGateConfig config() {
    BehaviorGateConfig cfg;
    cfg.idleEnabled = false;
    cfg.levelMax = 4;
    cfg.startChancePct = 100;
    cfg.levelDecayMs = 600000;  // the level only counts events here
    cfg.answerJitterMs = 0;
    cfg.restMs = 5000;
    cfg.refractoryAfterEmitMs = 0;  // isolate the rest rule from the refractory question
    return cfg;
}

// Valid trains at t until the level reaches the top and the node rests (levelMax events at most).
void restNow(ResonantBehavior& b, unsigned long t) {
    for (unsigned long i = 0; i < b.config().levelMax + 1 && !b.resting(t); ++i) {
        b.handleOccurrenceVerdict(validPattern(t), quietField(), t);
    }
    TEST_ASSERT_TRUE(b.resting(t));
}

// One heard pattern answered: verdict, wait, emission, finish. Returns the end of the emission.
unsigned long answer(ResonantBehavior& b, unsigned long heardAt) {
    TEST_ASSERT_EQUAL(ResonantBehavior::BehaviorDecision::ConsumedPattern,
                      b.handleOccurrenceVerdict(validPattern(heardAt), quietField(), heardAt));
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

// The shipped defaults rest by the level automaton.
void test_shipped_defaults_rest_by_the_level() {
    const BehaviorGateConfig cfg;
    TEST_ASSERT_TRUE(cfg.levelMax > 0);
    TEST_ASSERT_TRUE(cfg.restMs > 0);
}

// The events that take the level to the top start a rest instead of an answer.
void test_the_top_of_the_level_starts_a_rest_instead_of_an_answer() {
    ResonantBehavior b;
    b.configure(config());
    b.resetState();
    unsigned long t = 1000;
    for (unsigned long i = 0; i + 1 < b.config().levelMax && !b.resting(t); ++i) {
        t = answer(b, t) +
            2000;  // each heard train and each own emission raises the level by 1; clear of the self-suppress
    }
    restNow(b, t);
    TEST_ASSERT_EQUAL_UINT32(1, b.restsEntered());
    TEST_ASSERT_EQUAL(ResonantBehavior::BehaviorDecision::RestLevel, b.lastRestReason());
    b.update(t + b.config().waitAfterHeardMs);
    TEST_ASSERT_FALSE(b.shouldStartChirp());
}

// A rest that starts while an answer is scheduled drops it (code-review-2026-10-30 B-M1).
void test_rest_drops_a_scheduled_answer() {
    ResonantBehavior b;
    BehaviorGateConfig cfg = config();
    cfg.waitAfterHeardMs = 300;
    b.configure(cfg);
    b.resetState();
    const unsigned long t = 1000;
    TEST_ASSERT_EQUAL(ResonantBehavior::BehaviorDecision::ConsumedPattern,
                      b.handleOccurrenceVerdict(validPattern(t), quietField(), t));
    b.update(t);
    restNow(b, t + 60);  // trains inside the wait take the level to the top
    b.update(t + cfg.waitAfterHeardMs + 5);
    TEST_ASSERT_FALSE(b.shouldStartChirp());
}

// A valid verdict blocked by a rest still counts as heard: an idle call due when the rest ends waits
// idleBlockedAfterHeardMs after it instead of firing at once (code-review-2026-10-30 B-M3).
void test_blocked_verdict_holds_the_idle_call() {
    ResonantBehavior b;
    BehaviorGateConfig cfg = config();
    cfg.idleEnabled = true;
    cfg.idleTimeoutMs = 500;  // due long before the rest ends
    cfg.idleTimeVariationMs = 0;
    b.configure(cfg);
    b.resetState();
    const unsigned long t0 = 2000;
    b.update(t0);  // schedules the idle call
    restNow(b, t0);
    const unsigned long restEnd = t0 + cfg.restMs;
    const unsigned long late = restEnd - 200;  // heard 200 ms before the rest ends, blocked as Resting
    TEST_ASSERT_EQUAL(ResonantBehavior::BehaviorDecision::Resting,
                      b.handleOccurrenceVerdict(validPattern(late), quietField(), late));
    b.update(restEnd + 1);
    TEST_ASSERT_FALSE(b.shouldStartChirp());
    b.update(late + cfg.idleBlockedAfterHeardMs + 1);
    TEST_ASSERT_TRUE(b.shouldStartChirp());
}

// While resting every valid verdict is ignored; after restMs the node answers again.
void test_rest_ignores_verdicts_until_it_ends() {
    ResonantBehavior b;
    b.configure(config());
    b.resetState();
    const unsigned long t0 = 2000;
    restNow(b, t0);
    const unsigned long mid = t0 + b.config().restMs / 2;
    TEST_ASSERT_EQUAL(ResonantBehavior::BehaviorDecision::Resting,
                      b.handleOccurrenceVerdict(validPattern(mid), quietField(), mid));
    const unsigned long after = t0 + b.config().restMs + 1;
    TEST_ASSERT_FALSE(b.resting(after));
    answer(b, after);
    TEST_ASSERT_TRUE(b.blockedResting() >= 1);
}

// A bench-like ping-pong between two nodes ends: each hears the other's answer, the level rises on both, and one of
// them rests.
void test_two_node_ping_pong_is_cut() {
    ResonantBehavior a;
    ResonantBehavior b;
    a.configure(config());
    b.configure(config());
    a.resetState();
    b.resetState();
    unsigned long t = 1000;
    int turns = 0;
    ResonantBehavior* listener = &b;
    for (; turns < 100; ++turns) {
        listener->update(t);  // the Node's loop runs Behavior every pass (Refractory -> Idle)
        const ResonantBehavior::BehaviorDecision d =
            listener->handleOccurrenceVerdict(validPattern(t), quietField(), t);
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
        listener = (listener == &b) ? &a : &b;
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
    restNow(b, t0);
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

// A live idle_timeout change applies to the wait already running, from when it started.
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
    cfg.idleTimeoutMs = 2000;  // PARAM SET behavior.call_after_silence_ms 2000 at t = 1 s
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

// RB CALL: the operator call plays the network signal, and is not taken while resting.
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
    restNow(b, t0);
    b.requestNetworkCall();
    b.update(t0 + 100);
    TEST_ASSERT_FALSE(b.shouldStartChirp());
    TEST_ASSERT_TRUE(b.networkCallPending());
    b.update(t0 + b.config().restMs + 1);
    TEST_ASSERT_TRUE(b.shouldStartChirp());
}

// Automaton off (levelMax 0): no rest, whatever is heard.
void test_automaton_off_means_no_rest() {
    ResonantBehavior b;
    BehaviorGateConfig cfg = config();
    cfg.levelMax = 0;
    b.configure(cfg);
    b.resetState();
    unsigned long t = 1000;
    for (int i = 0; i < 12; ++i) {
        answer(b, t);
        t += 400;
    }
    TEST_ASSERT_EQUAL_UINT32(0, b.restsEntered());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_shipped_defaults_rest_by_the_level);
    RUN_TEST(test_the_top_of_the_level_starts_a_rest_instead_of_an_answer);
    RUN_TEST(test_rest_ignores_verdicts_until_it_ends);
    RUN_TEST(test_rest_drops_a_scheduled_answer);
    RUN_TEST(test_blocked_verdict_holds_the_idle_call);
    RUN_TEST(test_two_node_ping_pong_is_cut);
    RUN_TEST(test_no_idle_while_resting);
    RUN_TEST(test_shorter_idle_timeout_applies_to_the_running_wait);
    RUN_TEST(test_longer_idle_timeout_extends_the_running_wait_and_same_value_keeps_it);
    RUN_TEST(test_automaton_off_means_no_rest);
    RUN_TEST(test_network_call_plays_the_network_signal);
    RUN_TEST(test_network_call_waits_while_resting);
    return UNITY_END();
}
