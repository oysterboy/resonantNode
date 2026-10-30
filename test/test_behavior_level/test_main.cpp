/*
The level automaton on the host (BEH-008, step 16; decisions/2026-10-29-answers-follow-the-field-level-automaton.md):
one integer level per node (+1 per heard valid train, a chord's merged occurrence counted by its duration; +1 per own
emission; -1 per level_decay_ms of hearing nothing), p = (level + 1) / level_max for answering and for keeping a
pending answer (else the yield), a rest at the top, an answer after the fixed wait plus a random 0..jitter window.
The random draws are scripted, so each rule is pinned exactly. Runs in native-test-behavior.
*/
#include <unity.h>

#include <initializer_list>

#include "../../src/behavior/BehaviorProfile.h"
#include "../../src/behavior/ResonantBehavior.h"
#include "../../src/detection/evaluation/OccurrenceVerdict.h"

#include "../../src/behavior/ResonantBehavior.cpp"

namespace {

using Decision = ResonantBehavior::BehaviorDecision;

constexpr unsigned long kTrainMs = 165;  // the Node's hop train
constexpr unsigned long kVerdictTrainMs = 215;  // one train as a verdict reads it

// The scripted random source: returns the queued values in order (each taken modulo the bound), then 0.
uint32_t gScript[64];
size_t gScriptLen = 0;
size_t gScriptAt = 0;
uint32_t scripted(uint32_t bound) {
    const uint32_t v = gScriptAt < gScriptLen ? gScript[gScriptAt++] : 0U;
    return bound == 0 ? 0U : v % bound;
}
void script(std::initializer_list<uint32_t> values) {
    gScriptLen = 0;
    gScriptAt = 0;
    for (uint32_t v : values) {
        gScript[gScriptLen++] = v;
    }
}

detection::OccurrenceVerdict train(unsigned long heardAtMs, unsigned long durationMs = kVerdictTrainMs) {
    detection::OccurrenceVerdict v;
    v.type = detection::VerdictType::SinglePulse;
    v.accepted = true;
    v.proposalMatched = true;
    v.supportMatched = true;
    v.valid = true;
    v.primaryStartMs = heardAtMs > durationMs ? heardAtMs - durationMs : 0;
    v.primaryHeardAtMs = heardAtMs;
    v.primaryAcceptedMs = heardAtMs;
    v.primaryDurationMs = durationMs;
    v.primaryStrength = 9000.0f;
    return v;
}

BehaviorGateConfig automaton() {
    BehaviorGateConfig cfg;  // shipped: L 6, decay 2 s, jitter 2 s, rest 8 s, the old triggers off
    cfg.idleEnabled = false;
    return cfg;
}

ResonantBehavior make(const BehaviorGateConfig& cfg) {
    ResonantBehavior b;
    b.configure(cfg);
    b.resetState();
    b.setRandomSource(&scripted);
    return b;
}

}  // namespace

void setUp() {
    script({});
}
void tearDown() {}

// A chord's answers that merged into one occurrence count by its length; one train counts 1, at most 3.
void test_trains_counted_by_duration() {
    TEST_ASSERT_EQUAL_UINT32(1, ResonantBehavior::trainsInVerdict(train(1000, 180)));
    TEST_ASSERT_EQUAL_UINT32(1, ResonantBehavior::trainsInVerdict(train(1000, 220)));
    TEST_ASSERT_EQUAL_UINT32(2, ResonantBehavior::trainsInVerdict(train(1000, 430)));  // step 14: +250 ms, merged
    TEST_ASSERT_EQUAL_UINT32(3, ResonantBehavior::trainsInVerdict(train(1000, 2000)));
    TEST_ASSERT_EQUAL_UINT32(1, ResonantBehavior::trainsInVerdict(train(1000, 0)));
}

// The first train heard raises the level to 1, then draws against p = (1 + 1) / L, as tools/sim/fieldramp.py does
// (count, then draw): a draw of 1 (below 2) answers, a draw of 2 declines; the train is counted either way.
// Each answer also takes the jitter draw and the idle schedule's draw.
void test_low_level_answers_with_two_in_l() {
    ResonantBehavior b = make(automaton());
    script({1, 0, 0});  // answer draw 1, jitter 0, idle schedule
    TEST_ASSERT_EQUAL(Decision::ConsumedPattern, b.handleOccurrenceVerdict(train(1000), 1000));
    TEST_ASSERT_EQUAL_UINT32(1, b.level(1000));

    ResonantBehavior c = make(automaton());
    script({2});
    TEST_ASSERT_EQUAL(Decision::DeclinedByLevel, c.handleOccurrenceVerdict(train(1000), 1000));
    TEST_ASSERT_EQUAL_UINT32(1, c.level(1000));
    TEST_ASSERT_EQUAL_UINT32(1, c.declinedByLevel());
}

// The answer waits the fixed 100 ms plus the jitter draw.
void test_answer_waits_the_jitter_draw() {
    ResonantBehavior b = make(automaton());
    script({0, 700, 0});  // answer, then 700 ms of jitter, then the idle draw
    b.handleOccurrenceVerdict(train(1000), 1000);
    b.update(1000);
    b.update(1000 + 100 + 699);
    TEST_ASSERT_FALSE(b.shouldStartChirp());
    b.update(1000 + 100 + 700);
    TEST_ASSERT_TRUE(b.shouldStartChirp());
}

// L events take the node to the top: it rests, the level is back at 0, and the train is not answered.
void test_top_rests_and_resets_the_level() {
    BehaviorGateConfig cfg = automaton();
    ResonantBehavior b = make(cfg);
    script({5, 5, 5, 5, 5});  // declines: 5 is the largest draw for L 6, below level + 1 only at level 5
    unsigned long t = 1000;
    for (uint32_t i = 0; i + 1 < cfg.levelMax; ++i, t += 300) {
        b.handleOccurrenceVerdict(train(t), t);
    }
    TEST_ASSERT_EQUAL_UINT32(cfg.levelMax - 1, b.level(t));
    TEST_ASSERT_EQUAL(Decision::RestLevel, b.handleOccurrenceVerdict(train(t), t));
    TEST_ASSERT_TRUE(b.resting(t));
    TEST_ASSERT_EQUAL_UINT32(0, b.level(t));
    TEST_ASSERT_EQUAL(Decision::Resting, b.handleOccurrenceVerdict(train(t + 1000), t + 1000));
    TEST_ASSERT_EQUAL_UINT32(0, b.level(t + 1000));  // no counting while resting
    TEST_ASSERT_FALSE(b.resting(t + cfg.restMs + 1));
}

// A merged chord of two counts two levels: one train short of the top plus a chord rests.
void test_chord_counts_two() {
    BehaviorGateConfig cfg = automaton();
    ResonantBehavior b = make(cfg);
    script({5, 5, 5, 5});
    unsigned long t = 1000;
    for (uint32_t i = 0; i + 2 < cfg.levelMax; ++i, t += 300) {
        b.handleOccurrenceVerdict(train(t), t);
    }
    TEST_ASSERT_EQUAL(Decision::RestLevel, b.handleOccurrenceVerdict(train(t, 430), t));
}

// The yield: an answer pending and another train arrives; kept with p, dropped with 1 - p.
void test_yield_drops_a_pending_answer_with_one_minus_p() {
    ResonantBehavior b = make(automaton());
    script({0, 1500, 0, 5});  // answer the first (draw 0), 1.5 s jitter, idle draw; then draw 5 >= level 2 + 1: yield
    b.handleOccurrenceVerdict(train(1000), 1000);
    b.update(1000);
    TEST_ASSERT_EQUAL(Decision::Yielded, b.handleOccurrenceVerdict(train(1400), 1400));
    TEST_ASSERT_EQUAL_UINT32(1, b.yielded());
    b.update(1000 + 100 + 1500 + 10);
    TEST_ASSERT_FALSE(b.shouldStartChirp());

    ResonantBehavior c = make(automaton());
    script({0, 1500, 0, 1});  // the second train's draw 1 < level 2 + 1: the answer is kept
    c.handleOccurrenceVerdict(train(1000), 1000);
    c.update(1000);
    TEST_ASSERT_EQUAL(Decision::AlreadyScheduled, c.handleOccurrenceVerdict(train(1400), 1400));
    c.update(1000 + 100 + 1500);
    TEST_ASSERT_TRUE(c.shouldStartChirp());
}

// One step down per level_decay_ms of hearing nothing.
void test_level_decays_one_step_per_decay() {
    BehaviorGateConfig cfg = automaton();
    ResonantBehavior b = make(cfg);
    script({5, 5, 5});
    b.handleOccurrenceVerdict(train(1000), 1000);
    b.handleOccurrenceVerdict(train(1100), 1100);
    b.handleOccurrenceVerdict(train(1200), 1200);
    TEST_ASSERT_EQUAL_UINT32(3, b.level(1200));
    TEST_ASSERT_EQUAL_UINT32(3, b.level(1200 + cfg.levelDecayMs - 1));
    TEST_ASSERT_EQUAL_UINT32(2, b.level(1200 + cfg.levelDecayMs));
    TEST_ASSERT_EQUAL_UINT32(0, b.level(1200 + 3 * cfg.levelDecayMs));
    // A train after a long silence counts from the decayed level, not from 3.
    script({5});
    b.handleOccurrenceVerdict(train(1200 + 10 * cfg.levelDecayMs), 1200 + 10 * cfg.levelDecayMs);
    TEST_ASSERT_EQUAL_UINT32(1, b.level(1200 + 10 * cfg.levelDecayMs));
}

// An own emission is one event; at the top it rests from the end of the emission.
void test_own_emission_raises_the_level_and_can_rest() {
    BehaviorGateConfig cfg = automaton();
    ResonantBehavior b = make(cfg);
    script({5, 5, 5, 5, 0, 0, 0});  // four declines, then answer the fifth: draw 0, no jitter, idle draw
    unsigned long t = 1000;
    for (int i = 0; i < 4; ++i, t += 300) {
        b.handleOccurrenceVerdict(train(t), t);
    }
    TEST_ASSERT_EQUAL(Decision::ConsumedPattern, b.handleOccurrenceVerdict(train(t), t));  // level 5
    b.update(t);
    const unsigned long emitAt = t + cfg.waitAfterHeardMs;
    b.update(emitAt);
    TEST_ASSERT_TRUE(b.shouldStartChirp());
    b.notifyChirpStarted(emitAt, kTrainMs);  // level 6 = L: rest from the end of this train
    TEST_ASSERT_EQUAL_UINT32(0, b.level(emitAt));
    TEST_ASSERT_TRUE(b.resting(emitAt + kTrainMs + cfg.restMs - 1));
    TEST_ASSERT_FALSE(b.resting(emitAt + kTrainMs + cfg.restMs + 1));
    TEST_ASSERT_EQUAL(Decision::RestLevel, b.lastRestReason());
}

// level_max 0 switches the automaton off: every train is answered after exactly the fixed wait (the timing before
// step 16), whatever the draws.
void test_off_reproduces_the_fixed_answer() {
    BehaviorGateConfig cfg = automaton();
    cfg.levelMax = 0;
    ResonantBehavior b = make(cfg);
    script({5, 5, 5});
    TEST_ASSERT_EQUAL(Decision::ConsumedPattern, b.handleOccurrenceVerdict(train(1000), 1000));
    b.update(1000);
    b.update(1000 + cfg.waitAfterHeardMs);
    TEST_ASSERT_TRUE(b.shouldStartChirp());
    TEST_ASSERT_EQUAL_UINT32(0, b.level(1000));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_trains_counted_by_duration);
    RUN_TEST(test_low_level_answers_with_two_in_l);
    RUN_TEST(test_answer_waits_the_jitter_draw);
    RUN_TEST(test_top_rests_and_resets_the_level);
    RUN_TEST(test_chord_counts_two);
    RUN_TEST(test_yield_drops_a_pending_answer_with_one_minus_p);
    RUN_TEST(test_level_decays_one_step_per_decay);
    RUN_TEST(test_own_emission_raises_the_level_and_can_rest);
    RUN_TEST(test_off_reproduces_the_fixed_answer);
    return UNITY_END();
}
