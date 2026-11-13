/*
light_curve on the host: the light's level 0..1 as the driver turns it into bytes
(decisions/2026-11-13-light-level-scalar-cap-in-hal.md). What the effects above rely on: 0 is dark and 1 is the
brightest the cap allows, the curve rises with the level and follows lightness (0.5 is about 18 % of the light), and
no level, colour or pixel count gives a frame whose estimate is over the cap. Runs in native-test-behavior.
*/
#include <math.h>
#include <unity.h>

#include "../../src/hal/LightCurve.h"

void setUp() {}
void tearDown() {}

namespace {
const uint8_t kWarmWhite[3] = {255, 170, 90};  // the stripboard's LIGHT_WHITE_* at the time of writing
}

void test_curve_ends_and_clamps() {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, light_curve::perceivedToLinear(0.0f));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, light_curve::perceivedToLinear(1.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, light_curve::perceivedToLinear(-0.5f));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, light_curve::perceivedToLinear(3.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, light_curve::perceivedToLinear(NAN));  // NaN reads as dark
}

void test_curve_rises_and_follows_lightness() {
    float last = -1.0f;
    for (int i = 0; i <= 1000; ++i) {
        const float y = light_curve::perceivedToLinear(i / 1000.0f);
        TEST_ASSERT_TRUE(y > last);
        last = y;
    }
    TEST_ASSERT_FLOAT_WITHIN(0.002f, 0.1842f, light_curve::perceivedToLinear(0.5f));
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, 0.0114f, light_curve::perceivedToLinear(0.1f));
    // the two pieces meet at L* = 8 without a step: 0.0002 apart differ by the slope alone (~0.11 x 0.0002)
    TEST_ASSERT_FLOAT_WITHIN(0.00003f, light_curve::perceivedToLinear(0.0799f),
                             light_curve::perceivedToLinear(0.0801f));
}

void test_full_level_is_the_cap() {
    uint8_t out[3];
    light_curve::colourAtLevel(1.0f, kWarmWhite, 3, 30, 500, out);
    const float ma = light_curve::estimateMa(out, 3, 30);
    TEST_ASSERT_TRUE(ma <= 500.0f);
    TEST_ASSERT_TRUE(ma > 490.0f);  // within the rounding of three bytes, not the 41 % ceiling of a raw percentage
    TEST_ASSERT_EQUAL_UINT8(105, out[0]);
    TEST_ASSERT_EQUAL_UINT8(70, out[1]);
    TEST_ASSERT_EQUAL_UINT8(37, out[2]);
}

void test_a_colour_that_fits_is_not_raised() {
    uint8_t out[3];
    light_curve::colourAtLevel(1.0f, kWarmWhite, 3, 1, 500, out);  // one pixel: ~40 mA, under the cap
    TEST_ASSERT_EQUAL_UINT8(255, out[0]);
    TEST_ASSERT_EQUAL_UINT8(170, out[1]);
    TEST_ASSERT_EQUAL_UINT8(90, out[2]);
}

void test_dark_is_dark() {
    uint8_t out[4] = {1, 1, 1, 1};
    const uint8_t white4[4] = {255, 255, 255, 255};
    light_curve::colourAtLevel(0.0f, white4, 4, 30, 500, out);
    for (int i = 0; i < 4; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, out[i]);
    }
}

void test_never_over_the_cap() {
    const uint8_t colours[4][4] = {{255, 170, 90, 0}, {255, 255, 255, 255}, {255, 0, 0, 0}, {0, 0, 0, 255}};
    const uint16_t pixelCounts[4] = {1, 12, 30, 60};
    const uint16_t caps[3] = {100, 250, 500};
    for (const auto& colour : colours) {
        for (uint16_t pixels : pixelCounts) {
            for (uint16_t cap : caps) {
                for (int step = 0; step <= 100; ++step) {
                    uint8_t out[4] = {0, 0, 0, 0};
                    light_curve::colourAtLevel(step / 100.0f, colour, 4, pixels, cap, out);
                    TEST_ASSERT_TRUE(light_curve::estimateMa(out, 4, pixels) <= cap);
                }
            }
        }
    }
}

void test_cap_scale() {
    TEST_ASSERT_EQUAL_FLOAT(1.0f, light_curve::capScale(400.0f, 500));
    TEST_ASSERT_EQUAL_FLOAT(0.5f, light_curve::capScale(1000.0f, 500));
    // 255 + 170 + 90 on 30 pixels at 20 mA per full channel
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 1211.8f, light_curve::estimateMa(kWarmWhite, 3, 30));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_curve_ends_and_clamps);
    RUN_TEST(test_curve_rises_and_follows_lightness);
    RUN_TEST(test_full_level_is_the_cap);
    RUN_TEST(test_a_colour_that_fits_is_not_raised);
    RUN_TEST(test_dark_is_dark);
    RUN_TEST(test_never_over_the_cap);
    RUN_TEST(test_cap_scale);
    return UNITY_END();
}
