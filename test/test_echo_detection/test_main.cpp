/*
The echo room's detection facts on the host (ECHO-001, step 20.2): AttackSlope's rise time per block and
makeClapVerdict's verdict on one occurrence. Runs in native-test-behavior.
*/
#include <unity.h>

#include "../../src/detection/echo/ClapVerdict.h"
#include "../../src/detection/features/AttackSlope.h"

using detection::AttackSlope;

void setUp() {}
void tearDown() {}

// Blocks every 2 ms (128 frames at 48 kHz is 2.7 ms; the gate only needs ms resolution).
void test_a_clap_rises_in_a_block_or_two() {
    AttackSlope a;
    a.configure(6000.0f, 18000.0f);
    TEST_ASSERT_EQUAL(-1, a.update(5000.0f, 100));
    TEST_ASSERT_EQUAL(-1, a.update(5200.0f, 102));
    TEST_ASSERT_EQUAL(2, a.update(30000.0f, 104));  // last quiet at 102
    TEST_ASSERT_EQUAL(-1, a.update(31000.0f, 106)); // still above: no second crossing
}

void test_a_swell_rises_slowly() {
    AttackSlope a;
    a.configure(6000.0f, 18000.0f);
    a.update(5000.0f, 0);
    long attack = -1;
    for (uint32_t t = 2; t <= 80 && attack < 0; t += 2) {
        attack = a.update(5000.0f + 200.0f * t, t);  // reaches 18000 at 65 ms
    }
    TEST_ASSERT_TRUE(attack >= 60);
}

void test_no_attack_without_a_quiet_level_first() {
    AttackSlope a;
    a.configure(6000.0f, 18000.0f);
    TEST_ASSERT_EQUAL(-1, a.update(20000.0f, 10));  // loud from the start
    a.update(12000.0f, 12);                        // falls, but not to the floor
    TEST_ASSERT_EQUAL(-1, a.update(20000.0f, 14));
    a.update(4000.0f, 20);
    TEST_ASSERT_EQUAL(4, a.update(20000.0f, 24));
    a.reset();
    TEST_ASSERT_EQUAL(-1, a.update(4000.0f, 30) < 0 ? -1 : 0);
}

detection::Occurrence accepted() {
    detection::Occurrence o;
    o.present = true;
    o.valid = true;
    o.occurrenceId = 7;
    o.startMs = 1000;
    o.peakMs = 1003;
    o.releaseMs = 1040;
    o.durationMs = 40;
    o.strength = 25000.0f;
    return o;
}

void test_clap_verdict_valid_with_a_fast_attack() {
    const detection::OccurrenceVerdict v = detection::makeClapVerdict(accepted(), 2, 10);
    TEST_ASSERT_TRUE(v.valid);
    TEST_ASSERT_EQUAL_UINT32(1040, v.primaryHeardAtMs);
    TEST_ASSERT_EQUAL_UINT32(40, v.primaryDurationMs);
    TEST_ASSERT_EQUAL_FLOAT(25000.0f, v.primaryStrength);
    TEST_ASSERT_EQUAL_UINT16(0, v.primaryLowestBinHz);
}

void test_clap_verdict_rejects_a_slow_or_unmeasured_attack() {
    detection::OccurrenceVerdict v = detection::makeClapVerdict(accepted(), 30, 10);
    TEST_ASSERT_FALSE(v.valid);
    TEST_ASSERT_TRUE(v.proposalMatched);
    TEST_ASSERT_EQUAL(static_cast<int>(detection::VerdictRejectReason::SupportTooLow),
                      static_cast<int>(v.rejectReason));
    v = detection::makeClapVerdict(accepted(), -1, 10);
    TEST_ASSERT_FALSE(v.valid);
    detection::Occurrence notAccepted = accepted();
    notAccepted.valid = false;
    TEST_ASSERT_FALSE(detection::makeClapVerdict(notAccepted, 2, 10).valid);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_clap_rises_in_a_block_or_two);
    RUN_TEST(test_a_swell_rises_slowly);
    RUN_TEST(test_no_attack_without_a_quiet_level_first);
    RUN_TEST(test_clap_verdict_valid_with_a_fast_attack);
    RUN_TEST(test_clap_verdict_rejects_a_slow_or_unmeasured_attack);
    return UNITY_END();
}
