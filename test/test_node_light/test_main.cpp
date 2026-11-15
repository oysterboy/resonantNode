/*
NodeLight on the host: the Node's light effects (decisions/2026-11-15-node-light-one-colour-and-floor.md). One colour
per node from its own base; idle rises from the floor toward the idle call, or breathes when no call can come; heard
lifts quickly and builds to its top by the answer; emitting ramps to 1.0; the afterglow falls back to the floor over
glow_ms, a yield over yield_ms; never below the floor. Also light_curve's HSB: one scale for every colour, white at
the cap, and the dither that spreads a level between two bytes over the pixels.
Runs in native-test-behavior.
*/
#include <unity.h>

#include "../../src/hal/LightCurve.h"
#include "../../src/modes/echo/EchoLight.h"
#include "../../src/modes/resonant/ResonantNodeLight.cpp"

void setUp() {}
void tearDown() {}

namespace {

using Phase = NodeLight::Phase;

NodeLight::Input idleAt(unsigned long now, unsigned long from = 1000, unsigned long due = 31000) {
    NodeLight::Input in;
    in.nowMs = now;
    in.phase = Phase::Idle;
    in.idleCallCanCome = true;
    in.idleFromMs = from;
    in.idleAtMs = due;
    in.baseCount = 4;
    in.ownBase = 0;
    return in;
}

// Steps the light every 20 ms from `from` to `to` (inclusive) with the phase and inputs of `in`.
NodeLight::Hsb run(NodeLight& light, NodeLight::Input in, unsigned long from, unsigned long to) {
    NodeLight::Hsb c;
    for (unsigned long t = from; t <= to; t += 20) {
        in.nowMs = t;
        c = light.update(in);
    }
    return c;
}

// Heard at 0 with the answer at 500, emitting 520..1200; returns the light at 1200.
NodeLight::Hsb toEmitEnd(NodeLight& light, NodeLight::Input& in) {
    in.phase = Phase::Heard;
    in.answerAtMs = 500;
    run(light, in, 0, 500);
    in.phase = Phase::Emitting;
    return run(light, in, 520, 1200);
}

}  // namespace

void test_idle_rises_from_the_floor_to_its_top_at_the_call() {
    NodeLight light;
    NodeLight::Hsb c = light.update(idleAt(1000));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.35f, c.bright);  // starts at the floor, never dark
    c = run(light, idleAt(0), 1020, 16000);
    TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.40f, c.bright);  // half way to the call: half way from 0.35 to 0.45
    c = run(light, idleAt(0), 16020, 31000);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.45f, c.bright);
    c = run(light, idleAt(0), 31020, 40000);  // the call held past its time: stays at the top
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.45f, c.bright);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 20.0f, c.hueDeg);  // hue_low at the lowest base
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.8f, c.sat);
}

void test_idle_breathes_between_floor_and_top_when_no_call_can_come() {
    NodeLight light;
    NodeLight::Input in = idleAt(0);
    in.idleCallCanCome = false;
    float lo = 1.0f, hi = 0.0f;
    for (unsigned long t = 0; t <= 12000; t += 20) {
        in.nowMs = t;
        const float b = light.update(in).bright;
        lo = b < lo ? b : lo;
        hi = b > hi ? b : hi;
    }
    TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.35f, lo);
    TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.45f, hi);
}

void test_heard_lifts_quickly_then_builds_to_its_top_by_the_answer() {
    NodeLight light;
    run(light, idleAt(0), 1000, 7000);  // idle at 0.37 (6 s of 30)
    NodeLight::Input in = idleAt(0);
    in.phase = Phase::Heard;
    in.answerAtMs = 7020 + 1200;
    NodeLight::Hsb c = run(light, in, 7020, 7220);  // the 200 ms lift
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.55f, c.bright);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 20.0f, c.hueDeg);  // the same colour as idle
    c = run(light, in, 7240, 7720);  // half the build (1000 ms from 7220)
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.60f, c.bright);
    c = run(light, in, 7740, 8220);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.65f, c.bright);
}

void test_emitting_ramps_to_full_fast_and_holds() {
    NodeLight light;
    NodeLight::Input in = idleAt(0);
    in.phase = Phase::Heard;
    in.answerAtMs = 500;
    run(light, in, 0, 500);
    in.phase = Phase::Emitting;
    NodeLight::Hsb c = run(light, in, 520, 560);  // 40 ms: half the 80 ms ramp from 0.65
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.825f, c.bright);
    c = run(light, in, 580, 1200);  // the chirp still playing
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, c.bright);
}

void test_the_announcement_holds_and_the_call_jumps() {
    NodeLight light;
    NodeLight::Input in = idleAt(0, 0, 1000);
    run(light, in, 0, 1000);  // at the idle top when the call is due
    in.phase = Phase::Announce;
    NodeLight::Hsb c = run(light, in, 1020, 1720);  // 500 ms tone + 200 ms gap
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.45f, c.bright);
    in.phase = Phase::Emitting;
    c = run(light, in, 1740, 1820);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, c.bright);
}

void test_the_afterglow_falls_fast_then_a_long_tail_to_the_floor() {
    NodeLight light;
    NodeLight::Input in = idleAt(0, 0, 100000);
    toEmitEnd(light, in);
    in.phase = Phase::Afterglow;
    NodeLight::Hsb c = run(light, in, 1220, 1220 + 400);  // a fifth of glow_ms: (0.8)^2 of the way left
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.35f + 0.65f * 0.64f, c.bright);
    // Behavior's afterglow ends long before the light's: idle takes over and the fall goes on underneath.
    NodeLight::Input idle = idleAt(0, 1900, 100000);
    c = run(light, idle, 1640, 1220 + 1000);  // half of glow_ms: a quarter of the way left
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.35f + 0.65f * 0.25f, c.bright);
    c = run(light, idle, 2240, 1220 + 2000);
    TEST_ASSERT_TRUE(c.bright < 0.37f);  // back at the floor, the slow idle rise just above it
    TEST_ASSERT_TRUE(c.bright >= 0.35f);
}

void test_emit_straight_to_idle_glows_too() {
    NodeLight light;
    NodeLight::Input in = idleAt(0, 0, 100000);
    toEmitEnd(light, in);
    NodeLight::Input idle = idleAt(0, 1220, 100000);
    const NodeLight::Hsb c = run(light, idle, 1220, 1220 + 1000);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.35f + 0.65f * 0.25f, c.bright);
}

void test_a_yield_falls_back_over_yield_ms() {
    NodeLight light;
    NodeLight::Input in = idleAt(0, 0, 100000);
    in.phase = Phase::Heard;
    in.answerAtMs = 1000;
    run(light, in, 0, 1000);                          // at heard_top, 0.65
    NodeLight::Input back = idleAt(0, 1000, 31000);  // Behavior scheduled the idle wait at the heard
    NodeLight::Hsb c = run(light, back, 1020, 1020 + 750);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.35f + 0.30f * 0.25f, c.bright);  // half of 1500 ms
    c = run(light, back, 1790, 1020 + 1500);
    TEST_ASSERT_TRUE(c.bright < 0.37f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 20.0f, c.hueDeg);
}

void test_the_own_base_sets_the_one_hue() {
    NodeLight light;
    NodeLight::Input in = idleAt(0);
    in.ownBase = 3;  // the highest of four
    NodeLight::Hsb c = light.update(in);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 220.0f, c.hueDeg);
    in.phase = Phase::Heard;
    in.answerAtMs = 10000;
    c = run(light, in, 20, 300);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 220.0f, c.hueDeg);  // heard does not change the colour
    in.ownBase = 1;
    c = light.update(in);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 326.7f, c.hueDeg);  // a third of the way, 20 -> 220 through magenta
}

void test_mix_hue_goes_the_short_way() {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, NodeLight::mixHue(350.0f, 10.0f, 0.5f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 355.0f, NodeLight::mixHue(10.0f, 340.0f, 0.5f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 120.0f, NodeLight::mixHue(100.0f, 140.0f, 0.5f));
}

void test_disabled_is_dark() {
    NodeLight light;
    light.values().enabled = false;
    NodeLight::Input in = idleAt(0);
    in.phase = Phase::Emitting;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, run(light, in, 0, 1000).bright);
}

void test_hsb_white_at_the_cap_and_one_scale_for_every_hue() {
    uint8_t white[3], red[3], yellow[3];
    light_curve::rgbAtHsb(0.0f, 0.0f, 1.0f, 28, 500, white);
    TEST_ASSERT_TRUE(light_curve::estimateMa(white, 3, 28) <= 500.0f);
    TEST_ASSERT_TRUE(light_curve::estimateMa(white, 3, 28) > 490.0f);
    TEST_ASSERT_EQUAL_UINT8(white[0], white[1]);
    light_curve::rgbAtHsb(0.0f, 1.0f, 1.0f, 28, 500, red);
    TEST_ASSERT_EQUAL_UINT8(white[0], red[0]);  // red at B = 1 is white's red channel, not three times it
    TEST_ASSERT_EQUAL_UINT8(0, red[1]);
    TEST_ASSERT_EQUAL_UINT8(0, red[2]);
    light_curve::rgbAtHsb(60.0f, 1.0f, 1.0f, 28, 500, yellow);
    TEST_ASSERT_EQUAL_UINT8(white[0], yellow[0]);
    TEST_ASSERT_EQUAL_UINT8(white[0], yellow[1]);
    uint8_t dark[3] = {9, 9, 9};
    light_curve::rgbAtHsb(200.0f, 1.0f, 0.0f, 28, 500, dark);
    TEST_ASSERT_EQUAL_UINT8(0, dark[0] + dark[1] + dark[2]);
}

void test_hsb_totals_fit_the_cap_and_match_the_bytes() {
    uint32_t totals[3];
    light_curve::rgbTotalsAtHsb(0.0f, 0.0f, 1.0f, 28, 500, totals);
    const float ma = (totals[0] + totals[1] + totals[2]) / 255.0f * light_curve::kChannelFullMa;
    TEST_ASSERT_TRUE(ma <= 500.0f);
    TEST_ASSERT_TRUE(ma > 499.0f);  // the rounding of three totals, not of three bytes
    uint8_t bytes[3];
    light_curve::rgbAtHsb(0.0f, 0.0f, 1.0f, 28, 500, bytes);
    TEST_ASSERT_EQUAL_UINT32(bytes[0], totals[0] / 28);
}

void test_dither_spreads_a_total_over_the_pixels() {
    const uint16_t stride = light_curve::ditherStride(28);
    TEST_ASSERT_EQUAL_UINT16(11, stride);
    for (uint32_t total = 0; total < 28U * 8U; ++total) {
        uint32_t sum = 0;
        for (uint16_t p = 0; p < 28; ++p) {
            const uint8_t b = light_curve::ditherByte(total, 28, stride, p);
            TEST_ASSERT_TRUE(b == total / 28 || b == total / 28 + 1);
            sum += b;
        }
        TEST_ASSERT_EQUAL_UINT32(total, sum);  // the level is kept exactly
    }
    // One more lights exactly one more pixel: the rest of the strip does not shimmer while a fade crawls.
    for (uint32_t total = 0; total < 28U * 3U; ++total) {
        int changed = 0;
        for (uint16_t p = 0; p < 28; ++p) {
            changed +=
                light_curve::ditherByte(total, 28, stride, p) != light_curve::ditherByte(total + 1, 28, stride, p);
        }
        TEST_ASSERT_EQUAL_INT(1, changed);
    }
    // The first extra bytes land apart, not side by side.
    TEST_ASSERT_EQUAL_UINT8(1, light_curve::ditherByte(2, 28, stride, 0));
    TEST_ASSERT_EQUAL_UINT8(0, light_curve::ditherByte(2, 28, stride, 1));
    TEST_ASSERT_EQUAL_UINT16(1, light_curve::ditherStride(2));
    TEST_ASSERT_EQUAL_UINT8(255, light_curve::ditherByte(255U * 28U, 28, stride, 27));
}

void test_hs_to_rgb_primaries_and_wrap() {
    float rgb[3];
    light_curve::hsToRgb(120.0f, 1.0f, rgb);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, rgb[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, rgb[1]);
    light_curve::hsToRgb(240.0f + 360.0f, 1.0f, rgb);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, rgb[2]);
    light_curve::hsToRgb(-60.0f, 1.0f, rgb);  // magenta
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, rgb[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, rgb[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, rgb[2]);
}

void test_echo_light_glows_and_follows_the_replay() {
    EchoLight light;  // glow 30 %
    EchoLight::Hsb c = light.update(0, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.3f, c.bright);  // nothing playing: the glow
    c = light.update(20, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, c.bright);  // full scale: full
    c = light.update(170, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.65f, c.bright);  // half the release gone
    c = light.update(320, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.3f, c.bright);  // back at the glow after release_ms
    c = light.update(340, 0.0039f);                     // -48 dBFS: still the glow
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.3f, c.bright);
    c = light.update(360, 0.0631f);  // -24 dBFS: half the range above the glow
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.65f, c.bright);
    light.values().enabled = false;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, light.update(380, 1.0f).bright);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_idle_rises_from_the_floor_to_its_top_at_the_call);
    RUN_TEST(test_idle_breathes_between_floor_and_top_when_no_call_can_come);
    RUN_TEST(test_heard_lifts_quickly_then_builds_to_its_top_by_the_answer);
    RUN_TEST(test_emitting_ramps_to_full_fast_and_holds);
    RUN_TEST(test_the_announcement_holds_and_the_call_jumps);
    RUN_TEST(test_the_afterglow_falls_fast_then_a_long_tail_to_the_floor);
    RUN_TEST(test_emit_straight_to_idle_glows_too);
    RUN_TEST(test_a_yield_falls_back_over_yield_ms);
    RUN_TEST(test_the_own_base_sets_the_one_hue);
    RUN_TEST(test_mix_hue_goes_the_short_way);
    RUN_TEST(test_disabled_is_dark);
    RUN_TEST(test_hsb_white_at_the_cap_and_one_scale_for_every_hue);
    RUN_TEST(test_hsb_totals_fit_the_cap_and_match_the_bytes);
    RUN_TEST(test_dither_spreads_a_total_over_the_pixels);
    RUN_TEST(test_hs_to_rgb_primaries_and_wrap);
    RUN_TEST(test_echo_light_glows_and_follows_the_replay);
    return UNITY_END();
}
