/*
TrillShape on the host (24.4.1): every setting at 0 changes nothing, the fade is a smooth rise and fall held to half
the pulse, the pitch fall is centred on the pulse's own frequency, and the swell grows from half to full level.
Runs in native-test-behavior.
*/
#include <unity.h>

#include "../../src/output/TrillShape.h"

void setUp() {}
void tearDown() {}

void test_off_is_the_plain_trill() {
    TEST_ASSERT_FALSE(trill::Shape{}.any());
    for (uint32_t pos = 0; pos < 300; pos += 7) {
        TEST_ASSERT_EQUAL_FLOAT(1.0f, trill::fadeGain(pos, 240, 0));
        TEST_ASSERT_EQUAL_FLOAT(0.0f, trill::droopOffsetHz(pos, 240, 0));
    }
    for (uint32_t k = 0; k < 12; ++k) {
        TEST_ASSERT_EQUAL_FLOAT(1.0f, trill::swellGain(k, 0));
    }
}

void test_the_fade_rises_holds_and_falls() {
    const uint32_t pulse = 240;  // 15 ms at 16 kHz
    const uint32_t fade = 64;    // 4 ms
    TEST_ASSERT_EQUAL_FLOAT(0.0f, trill::fadeGain(0, pulse, fade));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, trill::fadeGain(fade / 2, pulse, fade));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, trill::fadeGain(fade, pulse, fade));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, trill::fadeGain(pulse / 2, pulse, fade));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, trill::fadeGain(pulse - fade / 2, pulse, fade));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, trill::fadeGain(pulse, pulse, fade));
    float last = -1.0f;
    for (uint32_t pos = 0; pos <= fade; ++pos) {  // the rise never steps back
        const float g = trill::fadeGain(pos, pulse, fade);
        TEST_ASSERT_TRUE(g >= last);
        last = g;
    }
}

void test_a_fade_longer_than_half_the_pulse_is_held_to_half() {
    const uint32_t pulse = 240;
    TEST_ASSERT_EQUAL_FLOAT(trill::fadeGain(60, pulse, pulse / 2), trill::fadeGain(60, pulse, 1000));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, trill::fadeGain(pulse / 2, pulse, 1000));
}

void test_the_fall_is_centred_on_the_pulse() {
    const uint32_t pulse = 240;
    TEST_ASSERT_EQUAL_FLOAT(75.0f, trill::droopOffsetHz(0, pulse, 150));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, trill::droopOffsetHz(pulse / 2, pulse, 150));
    TEST_ASSERT_EQUAL_FLOAT(-75.0f, trill::droopOffsetHz(pulse, pulse, 150));
    double sum = 0.0;
    for (uint32_t pos = 0; pos <= pulse; ++pos) {
        sum += trill::droopOffsetHz(pos, pulse, 150);
    }
    TEST_ASSERT_FLOAT_WITHIN(1.0, 0.0, sum / (pulse + 1));  // the pulse's mean stays on its bin
    TEST_ASSERT_EQUAL_FLOAT(trill::kMaxDroopHz / 2.0f, trill::droopOffsetHz(0, pulse, 5000));
}

void test_the_swell_grows_from_half_to_full() {
    TEST_ASSERT_EQUAL_FLOAT(0.5f, trill::swellGain(0, 3));
    TEST_ASSERT_TRUE(trill::swellGain(1, 3) > trill::swellGain(0, 3));
    TEST_ASSERT_TRUE(trill::swellGain(2, 3) > trill::swellGain(1, 3));
    TEST_ASSERT_TRUE(trill::swellGain(2, 3) < 1.0f);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, trill::swellGain(3, 3));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, trill::swellGain(9, 3));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_off_is_the_plain_trill);
    RUN_TEST(test_the_fade_rises_holds_and_falls);
    RUN_TEST(test_a_fade_longer_than_half_the_pulse_is_held_to_half);
    RUN_TEST(test_the_fall_is_centred_on_the_pulse);
    RUN_TEST(test_the_swell_grows_from_half_to_full);
    return UNITY_END();
}
