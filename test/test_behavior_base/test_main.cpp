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

// A scatter range k moves the base 1..k bins, folded back at the grid's edges (step 19.2's variant).
void test_rest_scatter_within_a_range() {
    BehaviorGateConfig cfg;
    cfg.idleEnabled = false;
    cfg.levelMax = 2;
    cfg.baseScatterBins = 1;
    ResonantBehavior b = make(cfg, 0);
    script({1, 0, 1});  // declined; the scatter: step 1 + (0 % 1), direction draw 1 = down, folded to +1
    b.handleOccurrenceVerdict(train(1000, 3700), 1000);
    TEST_ASSERT_EQUAL(Decision::RestLevel, b.handleOccurrenceVerdict(train(1300, 3700), 1300));
    TEST_ASSERT_EQUAL_UINT32(1, b.baseBin());
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

// A band per node (step 19): the listener's window is 5 bins from its base on the range 2200-4950 (8 bases). A
// sender above shows its lowest bin above the window's bottom; one below shows only its highest bin under the
// window's top; one level with it fills the window.
void test_band_per_node_reads_the_sender_from_lowest_and_highest() {
    BehaviorGateConfig cfg = answerEvery();
    cfg.bandPerNode = true;
    ResonantBehavior b = make(cfg, 0);
    b.setBaseGrid(2200, 250, 8, 5);
    b.seedBaseBin(3);  // window 2950-3950
    b.setOutputsEnabled(false);  // note the heard base only, no answer
    detection::OccurrenceVerdict above = train(1000, 3450);
    above.primaryHighestBinHz = 4450;
    b.handleOccurrenceVerdict(above, 1000);
    TEST_ASSERT_EQUAL(5, b.status(1000).baseHeard);
    detection::OccurrenceVerdict below = train(2000, 2950);  // a sender on base 1: 2700-3700, seen 2950-3450
    below.primaryHighestBinHz = 3450;
    b.handleOccurrenceVerdict(below, 2000);
    TEST_ASSERT_EQUAL(1, b.status(2000).baseHeard);
    detection::OccurrenceVerdict level = train(3000, 2950);
    level.primaryHighestBinHz = 3950;
    b.handleOccurrenceVerdict(level, 3000);
    TEST_ASSERT_EQUAL(3, b.status(3000).baseHeard);
}

// Alone since its last idle call, a node with a band per node calls from a random position; after hearing it stays.
void test_band_per_node_wanders_only_when_alone() {
    BehaviorGateConfig cfg = answerEvery();
    cfg.bandPerNode = true;
    cfg.idleEnabled = true;
    cfg.idleTimeoutMs = 1000;
    cfg.idleTimeVariationMs = 0;
    ResonantBehavior b = make(cfg, 0);
    b.setBaseGrid(2200, 250, 8, 5);
    b.seedBaseBin(0);
    script({5, 5, 5, 5, 5, 5, 5, 5});
    b.update(5000);
    b.update(7000);
    TEST_ASSERT_TRUE(b.shouldStartChirp());  // the idle call
    TEST_ASSERT_EQUAL_UINT32(5, b.baseBin());
    TEST_ASSERT_EQUAL_UINT32(1, b.status(7000).baseWanders);
    b.notifyChirpStarted(7000, 865);
    b.notifyChirpFinished(7865);
    b.update(8500);  // refractory over, back to Idle
    // A sender on base 6 (3700-4700) heard by the window at 5 (3450-4450): answered, gliding to 6.
    detection::OccurrenceVerdict heard = train(9000, 3700);
    heard.primaryHighestBinHz = 4450;
    b.handleOccurrenceVerdict(heard, 9000);
    b.update(9100);
    TEST_ASSERT_TRUE(b.shouldStartChirp());
    TEST_ASSERT_EQUAL_UINT32(6, b.baseBin());
    b.notifyChirpStarted(9100, 165);
    b.notifyChirpFinished(9265);
    // The next idle call: it heard something since the last one, so no wander.
    b.update(29000);  // Refractory -> Idle takes one pass
    b.update(30000);
    TEST_ASSERT_TRUE(b.shouldStartChirp());
    TEST_ASSERT_EQUAL_UINT32(6, b.baseBin());
    TEST_ASSERT_EQUAL_UINT32(1, b.status(30000).baseWanders);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_band_per_node_reads_the_sender_from_lowest_and_highest);
    RUN_TEST(test_band_per_node_wanders_only_when_alone);
    RUN_TEST(test_seed_is_clamped_to_the_grid);
    RUN_TEST(test_glide_one_bin_per_answer);
    RUN_TEST(test_heard_base_clamped_and_no_shape_ignored);
    RUN_TEST(test_no_glide_when_off);
    RUN_TEST(test_rest_scatters_the_base);
    RUN_TEST(test_rest_scatter_within_a_range);
    RUN_TEST(test_outputs_off_hears_and_reset_keeps_the_base);
    return UNITY_END();
}
