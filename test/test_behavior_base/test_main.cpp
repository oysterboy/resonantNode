/*
The base bin on the host (BEH-009 part 2, step 17.2; decisions/2026-10-29-wider-band-base-bin-per-node.md): the Node
hands Behavior the grid and a seed, the verdict carries the heard train's lowest bin, each own answer moves the base
one bin toward the base last heard (the glide), the automaton's rest scatters it. Runs in native-test-behavior.
*/
#include <unity.h>

#include <initializer_list>

#include "../../src/behavior/BehaviorProfile.h"
#include "../../src/behavior/ResonantBehavior.h"
#include "../../src/detection/evaluation/OccurrenceVerdict.h"

#include "../../src/behavior/ResonantBehavior.cpp"

namespace {

using Decision = ResonantBehavior::BehaviorDecision;

// The 8-bin band 2950-4700 with a 5-step train: bases 0..3 (2950, 3200, 3450, 3700).
constexpr uint32_t kLowHz = 2950;
constexpr uint32_t kSpacingHz = 250;
constexpr uint32_t kBases = 4;

uint32_t gScript[16];
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

detection::OccurrenceVerdict train(unsigned long heardAtMs, uint16_t lowestBinHz) {
    detection::OccurrenceVerdict v;
    v.type = detection::VerdictType::SinglePulse;
    v.accepted = true;
    v.proposalMatched = true;
    v.supportMatched = true;
    v.valid = true;
    v.primaryStartMs = heardAtMs - 215;
    v.primaryHeardAtMs = heardAtMs;
    v.primaryAcceptedMs = heardAtMs;
    v.primaryDurationMs = 215;
    v.primaryStrength = 9000.0f;
    v.primaryLowestBinHz = lowestBinHz;
    return v;
}

// Every train answered after the fixed wait (the automaton off), so each answer is one glide step.
BehaviorGateConfig answerEvery() {
    BehaviorGateConfig cfg;
    cfg.idleEnabled = false;
    cfg.levelMax = 0;
    return cfg;
}

ResonantBehavior make(const BehaviorGateConfig& cfg, uint32_t seed) {
    ResonantBehavior b;
    b.configure(cfg);
    b.resetState();
    b.setRandomSource(&scripted);
    b.setBaseGrid(kLowHz, kSpacingHz, kBases);
    b.seedBaseBin(seed);
    return b;
}

// Hear a train at `now`, let the wait pass, play the answer (165 ms) and its refractory; the base after it.
uint32_t answerTo(ResonantBehavior& b, unsigned long now, uint16_t lowestBinHz) {
    b.handleOccurrenceVerdict(train(now, lowestBinHz), now);
    b.update(now + 100);
    TEST_ASSERT_TRUE(b.shouldStartChirp());
    b.notifyChirpStarted(now + 100, 165);
    b.notifyChirpFinished(now + 265);
    b.update(now + 265 + 1000);  // past refractory and the suppression tail
    return b.baseBin();
}

}  // namespace

void setUp() {
    script({});
}
void tearDown() {}

// The seed lands on the grid: above the highest base it is the highest; a smaller grid pulls the base down with it.
void test_seed_is_clamped_to_the_grid() {
    ResonantBehavior b = make(answerEvery(), 9);
    TEST_ASSERT_EQUAL_UINT32(3, b.baseBin());
    b.setBaseGrid(kLowHz, kSpacingHz, 2);
    TEST_ASSERT_EQUAL_UINT32(1, b.baseBin());
    b.setBaseGrid(0, 0, 0);  // single-target mode: no base
    TEST_ASSERT_EQUAL_UINT32(0, b.baseBin());
}

// The answer glides one bin per answer toward the base heard and stays once there.
void test_glide_one_bin_per_answer() {
    ResonantBehavior b = make(answerEvery(), 0);
    TEST_ASSERT_EQUAL_UINT32(1, answerTo(b, 1000, 3700));  // heard base 3
    TEST_ASSERT_EQUAL_UINT32(2, answerTo(b, 4000, 3700));
    TEST_ASSERT_EQUAL_UINT32(3, answerTo(b, 7000, 3700));
    TEST_ASSERT_EQUAL_UINT32(3, answerTo(b, 10000, 3700));
    TEST_ASSERT_EQUAL_UINT32(2, answerTo(b, 13000, 2950));  // and back down toward a lower one
    TEST_ASSERT_EQUAL_UINT32(4, b.status(13000).baseGlides);
}

// A train that started above the highest base (more bases elsewhere, or a cut start) counts as the highest; one
// without band bins (lowest 0, below the band) leaves the base heard unset, so nothing glides.
void test_heard_base_clamped_and_no_shape_ignored() {
    ResonantBehavior b = make(answerEvery(), 1);
    TEST_ASSERT_EQUAL_UINT32(1, answerTo(b, 1000, 0));
    TEST_ASSERT_EQUAL(-1, b.status(1000).baseHeard);
    TEST_ASSERT_EQUAL_UINT32(2, answerTo(b, 4000, 4700));  // bin 7 -> base 3
    TEST_ASSERT_EQUAL(3, b.status(4000).baseHeard);
    // A bin between two grid steps rounds to the nearer one.
    b.handleOccurrenceVerdict(train(8000, 3330), 8000);
    TEST_ASSERT_EQUAL(2, b.status(8000).baseHeard);
}

// Glide off (config): the base stays where it was seeded.
void test_no_glide_when_off() {
    BehaviorGateConfig cfg = answerEvery();
    cfg.baseGlide = false;
    ResonantBehavior b = make(cfg, 0);
    TEST_ASSERT_EQUAL_UINT32(0, answerTo(b, 1000, 3700));
    TEST_ASSERT_EQUAL_UINT32(0, b.status(1000).baseGlides);
}

// The automaton's rest scatters the base to a random one and forgets the base heard.
void test_rest_scatters_the_base() {
    BehaviorGateConfig cfg;
    cfg.idleEnabled = false;
    cfg.levelMax = 2;
    ResonantBehavior b = make(cfg, 0);
    script({1, 2});  // the first train declined (p = 1 / 2, draw 1), the scatter's draw 2
    TEST_ASSERT_EQUAL(Decision::DeclinedByLevel, b.handleOccurrenceVerdict(train(1000, 3700), 1000));
    TEST_ASSERT_EQUAL(3, b.status(1000).baseHeard);
    TEST_ASSERT_EQUAL(Decision::RestLevel, b.handleOccurrenceVerdict(train(1300, 3700), 1300));
    TEST_ASSERT_EQUAL_UINT32(2, b.baseBin());
    TEST_ASSERT_EQUAL(-1, b.status(1300).baseHeard);
    TEST_ASSERT_EQUAL_UINT32(1, b.status(1300).baseScatters);
}

// Hearing while the outputs are off still notes the base heard; resetState keeps the base.
void test_outputs_off_hears_and_reset_keeps_the_base() {
    ResonantBehavior b = make(answerEvery(), 2);
    b.setOutputsEnabled(false);
    b.handleOccurrenceVerdict(train(1000, 3200), 1000);
    TEST_ASSERT_EQUAL(1, b.status(1000).baseHeard);
    b.resetState();
    TEST_ASSERT_EQUAL_UINT32(2, b.baseBin());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_seed_is_clamped_to_the_grid);
    RUN_TEST(test_glide_one_bin_per_answer);
    RUN_TEST(test_heard_base_clamped_and_no_shape_ignored);
    RUN_TEST(test_no_glide_when_off);
    RUN_TEST(test_rest_scatters_the_base);
    RUN_TEST(test_outputs_off_hears_and_reset_keeps_the_base);
    return UNITY_END();
}
