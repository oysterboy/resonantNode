/*
BandFlatness on the host (ECHO-002, step 20.3): white noise reads flat (near 1), a tone and a narrow band read near 0,
the reading does not depend on the level, and nothing pushed gives no reading. Runs in native-test-behavior.
*/
#include <math.h>
#include <stdint.h>
#include <unity.h>

#include "../../src/audio/BandBalance.h"
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

// BandBalance: a 400 Hz tone sits in the low band and is mostly gone from the whisper band; a 2.6 kHz tone the reverse.
void test_band_balance_splits_low_and_whisper_band() {
    auto run = [](float hz, float& lowRms, float& midRms) {
        BandBalance b;
        b.begin(24000);
        double mid = 0.0;
        int n = 0;
        for (int i = 0; i < 24000; ++i) {
            const float y = b.push(1000.0f * sinf(6.2831853f * hz * static_cast<float>(i) / 24000.0f));
            if (i >= 12000) {
                mid += static_cast<double>(y) * y;
                ++n;
            }
            if (i == 11999) {
                b.takeLowRms(lowRms);  // drop the settling half
            }
        }
        b.takeLowRms(lowRms);
        midRms = static_cast<float>(sqrt(mid / n));
    };
    float lo = 0.0f, mi = 0.0f;
    run(400.0f, lo, mi);
    TEST_ASSERT_TRUE(lo > 600.0f);  // ~707 (the tone's RMS) near the band's centre
    TEST_ASSERT_TRUE(mi < lo / 4.0f);
    run(2600.0f, lo, mi);
    TEST_ASSERT_TRUE(mi > 600.0f);
    TEST_ASSERT_TRUE(lo < mi / 4.0f);
    BandBalance none;
    none.begin(24000);
    float r = 1.0f;
    TEST_ASSERT_FALSE(none.takeLowRms(r));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_white_noise_reads_flat);
    RUN_TEST(test_a_tone_reads_near_zero);
    RUN_TEST(test_harmonics_read_low);
    RUN_TEST(test_level_does_not_matter);
    RUN_TEST(test_nothing_pushed_no_reading);
    RUN_TEST(test_band_balance_splits_low_and_whisper_band);
    return UNITY_END();
}
