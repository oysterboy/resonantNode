/*
GatedDelayLine on the host (ECHO-002, step 20.3): Whispernet's delay line passes the mic D samples late, only where
the gate bits are set; bits set after the samples came in still pass them while they are unplayed (the gate opens on
a provisional verdict and passes the phrase from its start), the gain ramps at each edge. Runs in
native-test-behavior.
*/
#include <unity.h>

#include "../../src/audio/GatedDelayLine.h"

void setUp() {}
void tearDown() {}

// Shut: nothing comes out. Passed: the input D samples later, unchanged once the ramp is done.
void test_delay_and_gate() {
    GatedDelayLine line;
    TEST_ASSERT_TRUE(line.begin(100, 10, 0));
    for (int i = 0; i < 100; ++i) {
        TEST_ASSERT_EQUAL_INT16(0, line.process(static_cast<int16_t>(i + 1)));  // filling
    }
    line.pass(0, 100);  // all of it, after it came in
    for (int i = 0; i < 100; ++i) {
        TEST_ASSERT_EQUAL_INT16(i + 1, line.process(0));
    }
    for (int i = 0; i < 100; ++i) {
        TEST_ASSERT_EQUAL_INT16(0, line.process(0));  // the zeros came in shut
    }
}

// A block not passed stays silent although its neighbours pass.
void test_only_passed_blocks_play() {
    GatedDelayLine line;
    TEST_ASSERT_TRUE(line.begin(40, 10, 0));
    for (int i = 0; i < 40; ++i) {
        line.process(1000);
    }
    line.pass(0, 10);
    line.pass(20, 40);
    int16_t out[40];
    for (int i = 0; i < 40; ++i) {
        out[i] = line.process(0);
    }
    TEST_ASSERT_EQUAL_INT16(1000, out[0]);
    TEST_ASSERT_EQUAL_INT16(0, out[15]);
    TEST_ASSERT_EQUAL_INT16(1000, out[25]);
}

// setOpen marks what comes in while open; the gain ramps in and out over rampSamples.
void test_open_and_ramp() {
    GatedDelayLine line;
    TEST_ASSERT_TRUE(line.begin(50, 10, 10));
    line.setOpen(true);
    for (int i = 0; i < 20; ++i) {
        line.process(10000);
    }
    line.setOpen(false);
    for (int i = 0; i < 30; ++i) {
        line.process(10000);
    }
    int16_t out[50];
    for (int i = 0; i < 50; ++i) {
        out[i] = line.process(0);
    }
    TEST_ASSERT_EQUAL_INT16(1000, out[0]);    // the ramp's first step
    TEST_ASSERT_EQUAL_INT16(10000, out[15]);  // fully open
    TEST_ASSERT_EQUAL_INT16(9000, out[20]);   // closing
    TEST_ASSERT_EQUAL_INT16(0, out[35]);      // shut
}

// What has been played cannot be passed afterwards; the future is not marked by pass().
void test_pass_outside_the_unplayed_is_ignored() {
    GatedDelayLine line;
    TEST_ASSERT_TRUE(line.begin(20, 10, 0));
    for (int i = 0; i < 40; ++i) {
        line.process(500);  // samples 0..19 played (shut)
    }
    line.pass(0, 1000);  // marks only the unplayed 20..39
    TEST_ASSERT_EQUAL_INT16(500, line.process(0));
    for (int i = 0; i < 19; ++i) {
        line.process(0);
    }
    TEST_ASSERT_EQUAL_INT16(0, line.process(0));  // sample 40 came in after the pass
}

// A delay below the capacity: the input comes out that many samples later (Whispernet's per-node D in a 2 s line).
void test_delay_below_the_capacity() {
    GatedDelayLine line;
    TEST_ASSERT_TRUE(line.begin(100, 10, 0));
    line.setDelay(30);
    TEST_ASSERT_EQUAL_UINT32(30, line.delay());
    line.setOpen(true);
    int16_t out[60];
    for (int i = 0; i < 60; ++i) {
        out[i] = line.process(static_cast<int16_t>(i + 1));
    }
    TEST_ASSERT_EQUAL_INT16(0, out[29]);
    TEST_ASSERT_EQUAL_INT16(1, out[30]);
    TEST_ASSERT_EQUAL_INT16(30, out[59]);
    line.setDelay(1000);  // clamped to the capacity
    TEST_ASSERT_EQUAL_UINT32(100, line.delay());
}

// Whispernet's size: 2 s at 24 kHz, blocks of 128, 10 ms ramps.
void test_whispernet_size() {
    GatedDelayLine line;
    TEST_ASSERT_TRUE(line.begin(48000, 128, 240));
    TEST_ASSERT_EQUAL_UINT32(48000, line.delay());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_delay_and_gate);
    RUN_TEST(test_only_passed_blocks_play);
    RUN_TEST(test_open_and_ramp);
    RUN_TEST(test_pass_outside_the_unplayed_is_ignored);
    RUN_TEST(test_delay_below_the_capacity);
    RUN_TEST(test_whispernet_size);
    return UNITY_END();
}
