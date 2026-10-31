/*
CaptureRing on the host (ECHO-001, step 20.2): samples addressed by their absolute index, a clip copied only while it
is whole in the ring. Runs in native-test-behavior.
*/
#include <unity.h>

#include "../../src/audio/CaptureRing.h"

void setUp() {}
void tearDown() {}

void test_copy_inside_the_ring() {
    CaptureRing ring;
    TEST_ASSERT_TRUE(ring.begin(8));
    for (int i = 0; i < 5; ++i) {
        ring.push(static_cast<int16_t>(100 + i));
    }
    int16_t out[3] = {};
    TEST_ASSERT_TRUE(ring.copy(1, 3, out));
    TEST_ASSERT_EQUAL_INT16(101, out[0]);
    TEST_ASSERT_EQUAL_INT16(103, out[2]);
    TEST_ASSERT_EQUAL_UINT64(5, ring.written());
    TEST_ASSERT_EQUAL_UINT64(0, ring.oldest());
}

void test_copy_across_the_wrap() {
    CaptureRing ring;
    ring.begin(4);
    for (int i = 0; i < 10; ++i) {
        ring.push(static_cast<int16_t>(i));
    }
    TEST_ASSERT_EQUAL_UINT64(6, ring.oldest());
    int16_t out[4] = {};
    TEST_ASSERT_TRUE(ring.copy(6, 4, out));
    TEST_ASSERT_EQUAL_INT16(6, out[0]);
    TEST_ASSERT_EQUAL_INT16(9, out[3]);
}

void test_refuses_overwritten_or_future_samples() {
    CaptureRing ring;
    ring.begin(4);
    for (int i = 0; i < 10; ++i) {
        ring.push(static_cast<int16_t>(i));
    }
    int16_t out[4] = {7, 7, 7, 7};
    TEST_ASSERT_FALSE(ring.copy(5, 2, out));   // 5 is overwritten
    TEST_ASSERT_FALSE(ring.copy(8, 3, out));   // 10 is not written yet
    TEST_ASSERT_EQUAL_INT16(7, out[0]);        // untouched
}

void test_no_buffer_no_copy() {
    CaptureRing ring;
    ring.push(1);  // before begin: ignored
    int16_t out[1] = {};
    TEST_ASSERT_FALSE(ring.copy(0, 1, out));
    TEST_ASSERT_EQUAL_UINT64(0, ring.written());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_copy_inside_the_ring);
    RUN_TEST(test_copy_across_the_wrap);
    RUN_TEST(test_refuses_overwritten_or_future_samples);
    RUN_TEST(test_no_buffer_no_copy);
    return UNITY_END();
}
