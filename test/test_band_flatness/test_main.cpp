/*
BandFlatness on the host (ECHO-002, step 20.3): white noise reads flat (near 1), a tone and a narrow band read near 0,
the reading does not depend on the level, and nothing pushed gives no reading. Runs in native-test-behavior.
*/
#include <math.h>
#include <stdint.h>
#include <unity.h>

#include "../../src/audio/BandFlatness.h"

namespace {

// A small LCG: the same noise every run.
float noise(uint32_t& state) {
    state = state * 1664525U + 1013904223U;
    return static_cast<float>(static_cast<int32_t>(state)) / 2147483648.0f;
}

float flatnessOf(float (*sample)(int, uint32_t&), float scale) {
    BandFlatness f;
    f.begin(24000);
    uint32_t state = 1;
    for (int i = 0; i < 2400; ++i) {  // 100 ms settling
        f.push(scale * sample(i, state));
    }
    float out = -1.0f;
    f.take(out);
    for (int i = 0; i < 4800; ++i) {
        f.push(scale * sample(i, state));
    }
    f.take(out);
    return out;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_white_noise_reads_flat() {
    const float v = flatnessOf([](int, uint32_t& s) { return noise(s); }, 1.0f);
    TEST_ASSERT_TRUE(v > 0.8f);
}

void test_a_tone_reads_near_zero() {
    const float v = flatnessOf([](int i, uint32_t&) { return sinf(6.2831853f * 1000.0f * i / 24000.0f); }, 1.0f);
    TEST_ASSERT_TRUE(v < 0.1f);
}

// A voice's harmonics, energy mostly low: flatter than a tone, far from noise (speech read 0.02-0.05 on the desk).
void test_harmonics_read_low() {
    const float v = flatnessOf(
        [](int i, uint32_t&) {
            float x = 0.0f;
            for (int h = 1; h <= 10; ++h) {
                x += sinf(6.2831853f * 150.0f * h * i / 24000.0f) / h;
            }
            return x;
        },
        1.0f);
    TEST_ASSERT_TRUE(v < 0.2f);
}

void test_level_does_not_matter() {
    const float loud = flatnessOf([](int, uint32_t& s) { return noise(s); }, 1000.0f);
    const float quiet = flatnessOf([](int, uint32_t& s) { return noise(s); }, 0.01f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, loud, quiet);
}

void test_nothing_pushed_no_reading() {
    BandFlatness f;
    f.begin(24000);
    float out = 0.0f;
    TEST_ASSERT_FALSE(f.take(out));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_white_noise_reads_flat);
    RUN_TEST(test_a_tone_reads_near_zero);
    RUN_TEST(test_harmonics_read_low);
    RUN_TEST(test_level_does_not_matter);
    RUN_TEST(test_nothing_pushed_no_reading);
    return UNITY_END();
}
