/*
BroadbandBurst on the host (step 24.1): level 0 leaves the tone untouched, tone and burst never pass full scale, a
burst is a short high-passed click, and almost none of it lies in the detection band. Runs in native-test-behavior.
*/
#include <initializer_list>
#include <math.h>
#include <unity.h>

#include "../../src/output/BroadbandBurst.h"

void setUp() {}
void tearDown() {}

namespace {
constexpr uint32_t kRate = 16000;

// Power of x at f Hz (one DFT bin).
double powerAt(const float* x, size_t n, double f) {
    double re = 0.0;
    double im = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double a = 2.0 * M_PI * f * static_cast<double>(i) / kRate;
        re += x[i] * cos(a);
        im += x[i] * sin(a);
    }
    return re * re + im * im;
}
}  // namespace

void test_level_zero_is_the_plain_tone() {
    TEST_ASSERT_EQUAL_FLOAT(1.0f, broadband::toneScale(0));
    for (float tone : {-1.0f, -0.3f, 0.0f, 0.7f, 1.0f}) {
        TEST_ASSERT_EQUAL_FLOAT(tone, broadband::mix(tone, 0.9f, 0));
    }
}

void test_the_mix_never_passes_full_scale() {
    for (uint32_t pct = 0; pct <= 150; pct += 5) {  // past 100 is held at 100
        for (float tone : {-1.0f, 0.0f, 1.0f}) {
            for (float burst : {-1.0f, 0.0f, 1.0f}) {
                TEST_ASSERT_TRUE(fabsf(broadband::mix(tone, burst, pct)) <= 1.0001f);
            }
        }
    }
}

void test_the_tone_gives_up_the_headroom_the_burst_takes() {
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f / 1.5f, broadband::toneScale(50));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f, broadband::toneScale(100));
    // 20 log10(1.5) = 3.5 dB at 50 %.
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -3.52f, 20.0f * log10f(broadband::toneScale(50)));
}

void test_a_burst_is_a_short_click() {
    broadband::BroadbandBurst b;
    b.configure(kRate, 6500);
    TEST_ASSERT_FALSE(b.active());
    TEST_ASSERT_EQUAL_FLOAT(0.0f, b.next());
    b.trigger();
    uint32_t n = 0;
    float peak = 0.0f;
    while (b.active()) {
        peak = fmaxf(peak, fabsf(b.next()));
        ++n;
    }
    TEST_ASSERT_EQUAL_UINT32(kRate * broadband::kBurstMs / 1000, n);  // 64 samples
    TEST_ASSERT_TRUE(peak <= 1.0f);
    TEST_ASSERT_TRUE(peak > 0.3f);  // it is audible, not a whisper
    TEST_ASSERT_EQUAL_FLOAT(0.0f, b.next());
}

void test_almost_none_of_it_is_in_the_detection_band() {
    // 200 bursts summed in power: the band's 3500 and 4700 Hz bins against the burst's own 7000 Hz.
    float x[64];
    double p3500 = 0.0;
    double p4700 = 0.0;
    double p7000 = 0.0;
    broadband::BroadbandBurst b;
    b.configure(kRate, 6500);
    for (int k = 0; k < 200; ++k) {
        b.trigger();
        for (float& v : x) {
            v = b.next();
        }
        p3500 += powerAt(x, 64, 3500.0);
        p4700 += powerAt(x, 64, 4700.0);
        p7000 += powerAt(x, 64, 7000.0);
    }
    const double db3500 = 10.0 * log10(p3500 / p7000);
    const double db4700 = 10.0 * log10(p4700 / p7000);
    // Measured 2026-11-17: -23 dB and -20 dB. The floor is the sharp onset (the click is meant to start at once), not
    // the filter: a burst with a soft start would be 20 dB lower still.
    TEST_ASSERT_TRUE(db3500 < -15.0);
    TEST_ASSERT_TRUE(db4700 < -12.0);
}

void test_a_lower_corner_lets_more_through() {
    float x[64];
    broadband::BroadbandBurst low;
    broadband::BroadbandBurst high;
    low.configure(kRate, 4000);
    high.configure(kRate, 6500);
    double pLow = 0.0;
    double pHigh = 0.0;
    for (int k = 0; k < 100; ++k) {
        low.trigger();
        for (float& v : x) {
            v = low.next();
        }
        pLow += powerAt(x, 64, 3500.0);
        high.trigger();
        for (float& v : x) {
            v = high.next();
        }
        pHigh += powerAt(x, 64, 3500.0);
    }
    TEST_ASSERT_TRUE(pLow > pHigh * 10.0);  // 4 kHz corner: about 14 dB more at 3500 Hz
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_level_zero_is_the_plain_tone);
    RUN_TEST(test_the_mix_never_passes_full_scale);
    RUN_TEST(test_the_tone_gives_up_the_headroom_the_burst_takes);
    RUN_TEST(test_a_burst_is_a_short_click);
    RUN_TEST(test_almost_none_of_it_is_in_the_detection_band);
    RUN_TEST(test_a_lower_corner_lets_more_through);
    return UNITY_END();
}
