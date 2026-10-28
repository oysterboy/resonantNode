/*
NodeBaseline on the host (step 11i N2, docs/refactors/node-split.md): the
Node's quiet-gated rebase machine, driven with a fake level and explicit
times. Boot warmup and its log cadence, the quiet hold, a noisy room timing
out, Settle -> Active, forced rebase, `RB rebase` restarting the gate, and
SELFTEST's resettle. Runs in native-test-behavior.
*/
#include <unity.h>

#include "../../src/modes/resonant/ResonantNodeBaseline.cpp"

namespace {

using State = NodeBaseline::State;
using Step = NodeBaseline::Step;
using Result = NodeBaseline::RebaseResult;

NodeBaseline::Config testConfig() {
    NodeBaseline::Config c;
    c.quietThreshold = 400;
    c.micWarmupMs = 2000;
    c.quietHoldMs = 1000;
    c.timeoutMs = 8000;
    c.settleMs = 500;
    c.logEveryMs = 1000;
    return c;
}

constexpr float kQuiet = 250.0f;
constexpr float kNoisy = 900.0f;

// Boot at t0 and play the Node's part of WarmupDone (startListening at the given time).
NodeBaseline bootedAndListening(unsigned long t0, unsigned long listenAt) {
    NodeBaseline b(testConfig());
    b.begin(t0);
    const NodeBaseline::Tick tick = b.update(t0 + 2000, kQuiet);
    TEST_ASSERT_EQUAL(static_cast<int>(Step::WarmupDone), static_cast<int>(tick.step));
    b.startListening(listenAt);
    return b;
}

void test_boot_state_and_name() {
    NodeBaseline b(testConfig());
    b.begin(100);
    TEST_ASSERT_EQUAL(static_cast<int>(State::Boot), static_cast<int>(b.state()));
    TEST_ASSERT_EQUAL_STRING("BOOT", b.stateName());
    TEST_ASSERT_FALSE(b.outputsAllowed());
    TEST_ASSERT_EQUAL(static_cast<int>(Result::Pending), static_cast<int>(b.bootResult()));
    TEST_ASSERT_EQUAL(static_cast<int>(Result::Pending), static_cast<int>(b.lastResult()));
}

void test_warmup_logs_once_a_second_then_done() {
    NodeBaseline b(testConfig());
    b.begin(100);
    NodeBaseline::Tick t = b.update(100, kNoisy);  // first tick logs at once
    TEST_ASSERT_EQUAL(static_cast<int>(Step::WarmupWaiting), static_cast<int>(t.step));
    TEST_ASSERT_EQUAL_UINT32(0, t.elapsedMs);
    t = b.update(600, kNoisy);
    TEST_ASSERT_EQUAL(static_cast<int>(Step::None), static_cast<int>(t.step));
    t = b.update(1100, kNoisy);
    TEST_ASSERT_EQUAL(static_cast<int>(Step::WarmupWaiting), static_cast<int>(t.step));
    TEST_ASSERT_EQUAL_UINT32(1000, t.elapsedMs);
    t = b.update(2099, kNoisy);
    TEST_ASSERT_EQUAL(static_cast<int>(Step::None), static_cast<int>(t.step));
    t = b.update(2100, kNoisy);
    TEST_ASSERT_EQUAL(static_cast<int>(Step::WarmupDone), static_cast<int>(t.step));
    TEST_ASSERT_EQUAL_UINT32(2000, t.elapsedMs);
    // A request: the machine stays in Boot until the Node calls startListening().
    TEST_ASSERT_EQUAL(static_cast<int>(State::Boot), static_cast<int>(b.state()));
    b.startListening(2150);
    TEST_ASSERT_EQUAL_STRING("LISTEN_FOR_QUIET", b.stateName());
}

void test_quiet_hold_rebases_then_settles_to_active() {
    NodeBaseline b = bootedAndListening(0, 2000);
    NodeBaseline::Tick t = b.update(2010, kQuiet);  // quiet starts, first log due
    TEST_ASSERT_EQUAL(static_cast<int>(Step::RebaseWaiting), static_cast<int>(t.step));
    TEST_ASSERT_EQUAL_UINT32(0, t.elapsedMs);
    TEST_ASSERT_EQUAL_FLOAT(kQuiet, t.smooth);
    t = b.update(3009, kQuiet);
    TEST_ASSERT_EQUAL(static_cast<int>(Step::None), static_cast<int>(t.step));
    t = b.update(3010, 300.0f);
    TEST_ASSERT_EQUAL(static_cast<int>(Step::QuietHeld), static_cast<int>(t.step));
    TEST_ASSERT_EQUAL_UINT32(1000, t.elapsedMs);
    TEST_ASSERT_EQUAL_STRING("SETTLE", b.stateName());
    TEST_ASSERT_EQUAL_FLOAT(300.0f, b.lastRebaseSmooth());
    TEST_ASSERT_FALSE(b.outputsAllowed());
    b.noteRebaseResult(Result::Done, 3050);
    TEST_ASSERT_EQUAL(static_cast<int>(Result::Done), static_cast<int>(b.bootResult()));
    TEST_ASSERT_EQUAL_UINT32(3050, b.lastRebaseAtMs());
    TEST_ASSERT_EQUAL_UINT32(1, b.quietRebaseCount());
    // Settle runs 500 ms from the tick that saw the hold.
    t = b.update(3509, kNoisy);
    TEST_ASSERT_EQUAL(static_cast<int>(Step::None), static_cast<int>(t.step));
    t = b.update(3510, kNoisy);
    TEST_ASSERT_EQUAL(static_cast<int>(Step::Activated), static_cast<int>(t.step));
    TEST_ASSERT_EQUAL_STRING("ACTIVE", b.stateName());
    TEST_ASSERT_TRUE(b.outputsAllowed());
    t = b.update(9000, kNoisy);
    TEST_ASSERT_EQUAL(static_cast<int>(Step::None), static_cast<int>(t.step));
}

void test_noise_breaks_the_hold() {
    NodeBaseline b = bootedAndListening(0, 2000);
    b.update(2000, kQuiet);
    b.update(2900, kQuiet);
    b.update(2950, kNoisy);  // hold broken
    NodeBaseline::Tick t = b.update(3100, kQuiet);
    TEST_ASSERT_EQUAL(static_cast<int>(Step::RebaseWaiting), static_cast<int>(t.step));
    TEST_ASSERT_EQUAL_UINT32(0, t.elapsedMs);
    t = b.update(4099, kQuiet);
    TEST_ASSERT_NOT_EQUAL(static_cast<int>(Step::QuietHeld), static_cast<int>(t.step));
    t = b.update(4100, kQuiet);
    TEST_ASSERT_EQUAL(static_cast<int>(Step::QuietHeld), static_cast<int>(t.step));
}

void test_noisy_room_times_out_and_runs_on() {
    NodeBaseline b = bootedAndListening(0, 2000);
    NodeBaseline::Tick t = b.update(9999, kNoisy);
    TEST_ASSERT_EQUAL(static_cast<int>(Step::None), static_cast<int>(t.step));
    t = b.update(10000, kNoisy);
    TEST_ASSERT_EQUAL(static_cast<int>(Step::NoQuiet), static_cast<int>(t.step));
    TEST_ASSERT_EQUAL_FLOAT(kNoisy, t.smooth);
    TEST_ASSERT_EQUAL_STRING("FAILED_NO_QUIET", b.stateName());
    TEST_ASSERT_TRUE(b.outputsAllowed());
    b.noteRebaseResult(Result::NoQuiet, 10001);
    TEST_ASSERT_EQUAL(static_cast<int>(Result::NoQuiet), static_cast<int>(b.bootResult()));
    TEST_ASSERT_EQUAL_UINT32(1, b.quietRebaseCount());
    // A later RB rebase that succeeds keeps the boot result, updates the last one.
    b.startListening(20000);
    TEST_ASSERT_FALSE(b.outputsAllowed());
    b.update(20000, kQuiet);
    t = b.update(21000, kQuiet);
    TEST_ASSERT_EQUAL(static_cast<int>(Step::QuietHeld), static_cast<int>(t.step));
    b.noteRebaseResult(Result::Done, 21000);
    TEST_ASSERT_EQUAL(static_cast<int>(Result::NoQuiet), static_cast<int>(b.bootResult()));
    TEST_ASSERT_EQUAL(static_cast<int>(Result::Done), static_cast<int>(b.lastResult()));
    TEST_ASSERT_EQUAL_UINT32(2, b.quietRebaseCount());
}

void test_forced_rebase_settles_and_counts_apart() {
    NodeBaseline b = bootedAndListening(0, 2000);
    b.noteForcedRebase(2500);
    TEST_ASSERT_EQUAL_STRING("SETTLE", b.stateName());
    TEST_ASSERT_EQUAL_UINT32(1, b.forcedRebaseCount());
    TEST_ASSERT_EQUAL_UINT32(0, b.quietRebaseCount());
    TEST_ASSERT_EQUAL(static_cast<int>(Result::Pending), static_cast<int>(b.lastResult()));
    TEST_ASSERT_EQUAL(static_cast<int>(Step::None), static_cast<int>(b.update(2999, kNoisy).step));
    TEST_ASSERT_EQUAL(static_cast<int>(Step::Activated), static_cast<int>(b.update(3000, kNoisy).step));
}

void test_resettle_only_from_active_or_settle() {
    NodeBaseline b = bootedAndListening(0, 2000);
    b.resettle(5000);  // listening: unchanged
    TEST_ASSERT_EQUAL_STRING("LISTEN_FOR_QUIET", b.stateName());
    b.update(10000, kNoisy);  // FailedNoQuiet
    b.resettle(12000);
    TEST_ASSERT_EQUAL_STRING("FAILED_NO_QUIET", b.stateName());

    NodeBaseline a = bootedAndListening(0, 2000);
    a.noteForcedRebase(2000);
    a.update(2500, kNoisy);
    TEST_ASSERT_EQUAL_STRING("ACTIVE", a.stateName());
    a.resettle(4000);
    TEST_ASSERT_EQUAL_STRING("SETTLE", a.stateName());
    TEST_ASSERT_EQUAL(static_cast<int>(Step::None), static_cast<int>(a.update(3999, kNoisy).step));
    TEST_ASSERT_EQUAL(static_cast<int>(Step::Activated), static_cast<int>(a.update(4000, kNoisy).step));
}

void test_begin_resets_results_and_counts() {
    NodeBaseline b = bootedAndListening(0, 2000);
    b.noteForcedRebase(2100);
    b.noteRebaseResult(Result::Done, 2200);
    b.begin(50000);
    TEST_ASSERT_EQUAL_STRING("BOOT", b.stateName());
    TEST_ASSERT_EQUAL_UINT32(0, b.forcedRebaseCount());
    TEST_ASSERT_EQUAL_UINT32(0, b.quietRebaseCount());
    TEST_ASSERT_EQUAL(static_cast<int>(Result::Pending), static_cast<int>(b.bootResult()));
    TEST_ASSERT_EQUAL_UINT32(2000, b.config().micWarmupMs);  // config kept
    TEST_ASSERT_EQUAL(static_cast<int>(Step::WarmupWaiting), static_cast<int>(b.update(50000, kQuiet).step));
}

void test_timing_across_the_32_bit_wrap() {
    const unsigned long t0 = 0xFFFFFF00UL;
    NodeBaseline b(testConfig());
    b.begin(t0);
    b.update(t0, kQuiet);
    TEST_ASSERT_EQUAL(static_cast<int>(Step::WarmupDone), static_cast<int>(b.update(t0 + 2000UL, kQuiet).step));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_boot_state_and_name);
    RUN_TEST(test_warmup_logs_once_a_second_then_done);
    RUN_TEST(test_quiet_hold_rebases_then_settles_to_active);
    RUN_TEST(test_noise_breaks_the_hold);
    RUN_TEST(test_noisy_room_times_out_and_runs_on);
    RUN_TEST(test_forced_rebase_settles_and_counts_apart);
    RUN_TEST(test_resettle_only_from_active_or_settle);
    RUN_TEST(test_begin_resets_results_and_counts);
    RUN_TEST(test_timing_across_the_32_bit_wrap);
    return UNITY_END();
}
