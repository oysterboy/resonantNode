/*
LongRecorder on the host: the framing (magic, position, count, samples, byte sum), a full ring dropping whole frames
and showing the gap in the position, the stop and the drain. Runs in native-test-behavior.
*/
#include <unity.h>

#include <vector>

#include "../../src/audio/LongRecorder.h"

void setUp() {}
void tearDown() {}

namespace {
std::vector<uint8_t> drain(LongRecorder& r) {
    std::vector<uint8_t> out;
    while (r.available() > 0) {
        size_t n = 0;
        const uint8_t* p = r.peek(n);
        out.insert(out.end(), p, p + n);
        r.consume(n);
    }
    return out;
}
}  // namespace

void test_a_frame_carries_position_count_samples_and_sum() {
    uint8_t store[256];
    LongRecorder r;
    r.begin(store, sizeof(store), 1000);
    const int32_t s[3] = {256, -256, 0x7FFFFF};
    r.push(s, 3);
    const std::vector<uint8_t> f = drain(r);
    TEST_ASSERT_EQUAL(LongRecorder::kFrameOverhead + 6, f.size());
    TEST_ASSERT_EQUAL_HEX8(0xA5, f[0]);
    TEST_ASSERT_EQUAL_HEX8(0x5A, f[1]);
    TEST_ASSERT_EQUAL(0, f[2]);
    TEST_ASSERT_EQUAL(3, f[6]);
    TEST_ASSERT_EQUAL(1, f[8]);  // 256 >> 8 = 1
    TEST_ASSERT_EQUAL(0xFF, f[10]);  // -1, low byte
    TEST_ASSERT_EQUAL(0x7F, f[13]);  // clipped to 32767: high byte
    uint16_t sum = 0;
    for (size_t i = 0; i + 2 < f.size(); ++i) {
        sum = static_cast<uint16_t>(sum + f[i]);
    }
    TEST_ASSERT_EQUAL(sum & 0xFF, f[f.size() - 2]);
    TEST_ASSERT_EQUAL(sum >> 8, f[f.size() - 1]);
    TEST_ASSERT_EQUAL(3, r.position());
}

void test_a_full_ring_drops_a_whole_frame_and_the_position_jumps() {
    uint8_t store[40];  // room for one 10-sample frame (30 bytes) and no second
    LongRecorder r;
    r.begin(store, sizeof(store), 1000);
    int32_t s[10] = {};
    r.push(s, 10);
    r.push(s, 10);  // does not fit
    TEST_ASSERT_EQUAL(10, r.droppedSamples());
    drain(r);
    r.push(s, 10);  // room again: its position is 20
    const std::vector<uint8_t> f = drain(r);
    TEST_ASSERT_EQUAL(20, f[2]);
    TEST_ASSERT_EQUAL(30, r.position());
}

void test_a_long_block_is_cut_into_frames_and_the_total_ends_it() {
    uint8_t store[2048];
    LongRecorder r;
    r.begin(store, sizeof(store), 150);
    int32_t s[400] = {};
    r.push(s, 400);
    TEST_ASSERT_TRUE(r.complete());
    TEST_ASSERT_EQUAL(150, r.position());
    TEST_ASSERT_FALSE(r.drained());
    const std::vector<uint8_t> f = drain(r);
    TEST_ASSERT_EQUAL(2 * LongRecorder::kFrameOverhead + 2 * 150, f.size());  // 120 + 30 samples
    TEST_ASSERT_TRUE(r.drained());
}

void test_the_ring_wraps_and_nothing_is_lost_when_it_is_drained() {
    uint8_t store[64];
    LongRecorder r;
    r.begin(store, sizeof(store), 100000);
    int32_t s[10] = {};
    for (int i = 0; i < 20; ++i) {
        r.push(s, 10);
        const std::vector<uint8_t> f = drain(r);  // 30 bytes a frame in a 64-byte ring: it wraps every other time
        TEST_ASSERT_EQUAL(LongRecorder::kFrameOverhead + 20, f.size());
        TEST_ASSERT_EQUAL_HEX8(0xA5, f[0]);
    }
    TEST_ASSERT_EQUAL(0, r.droppedSamples());
    TEST_ASSERT_EQUAL(200, r.position());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_frame_carries_position_count_samples_and_sum);
    RUN_TEST(test_a_full_ring_drops_a_whole_frame_and_the_position_jumps);
    RUN_TEST(test_a_long_block_is_cut_into_frames_and_the_total_ends_it);
    RUN_TEST(test_the_ring_wraps_and_nothing_is_lost_when_it_is_drained);
    return UNITY_END();
}
