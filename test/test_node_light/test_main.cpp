/*
NodeLight on the host: the Node's light effects (decisions/2026-11-13-light-colour-model-and-states.md). Idle rises
in colour A toward the idle call, or breathes when no call can come; heard fades to colour B and builds to its top
by the answer; emitting ramps to 1.0; afterglow and a yield fall at one rate; the bases set the hues, colour B from
the base heard when hue_source is 1. Also light_curve's HSB: one scale for every colour, white at the cap.
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

}  // namespace

void test_idle_rises_to_its_top_at_the_call() {
    NodeLight light;
    NodeLight::Hsb c = light.update(idleAt(1000));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, c.bright);
    c = run(light, idleAt(0), 1020, 16000);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.25f, c.bright);  // half way to the call, half of 0.5
    c = run(light, idleAt(0), 16020, 31000);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, c.bright);
    c = run(light, idleAt(0), 31020, 40000);  // the call held past its time: stays at the top
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, c.bright);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 190.0f, c.hueDeg);  // colour A at the lowest base
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.8f, c.sat);
}

void test_idle_breathes_when_no_call_can_come() {
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
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, lo);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, hi);
    in.nowMs = 3000 + 12000;  // half a period after two whole ones: the breath's top
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, light.update(in).bright);
}

void test_heard_fades_to_b_and_builds_to_its_top_by_the_answer() {
    NodeLight light;
    run(light, idleAt(0), 1000, 7000);  // idle at 0.1 (6 s of 30)
    NodeLight::Input in = idleAt(0);
    in.phase = Phase::Heard;
    in.answerAtMs = 7020 + 1000;
    NodeLight::Hsb c = run(light, in, 7020, 7120);  // 100 ms: the fade is through
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 20.0f, c.hueDeg);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, c.sat);
    TEST_ASSERT_TRUE(c.bright > 0.1f && c.bright < 0.2f);
    c = run(light, in, 7140, 8020);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, c.bright);
}

void test_emitting_ramps_to_full_then_the_afterglow_falls() {
    NodeLight light;
    NodeLight::Input in = idleAt(0);
    in.phase = Phase::Heard;
    in.answerAtMs = 500;
    run(light, in, 0, 500);
    in.phase = Phase::Emitting;
    NodeLight::Hsb c = run(light, in, 520, 680);  // 160 ms, past the 150 ms ramp
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, c.bright);
    c = run(light, in, 690, 1200);  // the chirp still playing
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, c.bright);
    in.phase = Phase::Afterglow;
    c = run(light, in, 1220, 1220 + 750);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.5f, c.bright);  // half of fall_ms: half way down
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 20.0f, c.hueDeg);  // still colour B
    c = run(light, in, 1990, 2800);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, c.bright);
}

void test_a_yield_ramps_down_in_colour_a() {
    NodeLight light;
    NodeLight::Input in = idleAt(0, 0, 100000);
    in.phase = Phase::Heard;
    in.answerAtMs = 1000;
    run(light, in, 0, 1000);  // at heard_top, 0.5
    NodeLight::Input back = idleAt(0, 1000, 31000);  // Behavior scheduled the idle wait at the heard
    NodeLight::Hsb c = run(light, back, 1020, 1120);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 190.0f, c.hueDeg);
    c = run(light, back, 1140, 1020 + 750);
    TEST_ASSERT_TRUE(c.bright < 0.3f);  // falling at the afterglow's rate (0.5 in 750 ms), the rise below it
    TEST_ASSERT_TRUE(c.bright > 0.0f);
}

void test_the_bases_set_the_hues() {
    NodeLight light;
    NodeLight::Input in = idleAt(0);
    in.ownBase = 3;  // the highest of four
    NodeLight::Hsb c = light.update(in);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 250.0f, c.hueDeg);
    in.phase = Phase::Heard;
    in.answerAtMs = 10000;
    in.heardBase = 0;
    c = run(light, in, 20, 300);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 50.0f, c.hueDeg);  // hue_source 0: B from the own base
    light.values().hueSource = 1;
    c = run(light, in, 320, 400);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 20.0f, c.hueDeg);  // B from the base heard
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
    RUN_TEST(test_idle_rises_to_its_top_at_the_call);
    RUN_TEST(test_idle_breathes_when_no_call_can_come);
    RUN_TEST(test_heard_fades_to_b_and_builds_to_its_top_by_the_answer);
    RUN_TEST(test_emitting_ramps_to_full_then_the_afterglow_falls);
    RUN_TEST(test_a_yield_ramps_down_in_colour_a);
    RUN_TEST(test_the_bases_set_the_hues);
    RUN_TEST(test_mix_hue_goes_the_short_way);
    RUN_TEST(test_disabled_is_dark);
    RUN_TEST(test_hsb_white_at_the_cap_and_one_scale_for_every_hue);
    RUN_TEST(test_hs_to_rgb_primaries_and_wrap);
    RUN_TEST(test_echo_light_glows_and_follows_the_replay);
    return UNITY_END();
}
