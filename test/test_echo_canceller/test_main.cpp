/*
EchoCanceller on the host (ECHO-002): the node's own output (white noise here) through a room-like path (a decaying
random response, 60 taps after a 5-sample gap) is learned and taken out of the mic by > 20 dB; a loud other voice
in the middle does not throw the filter (the cancellation holds after it); with the reference silent the mic passes
untouched; a voice from the room passes while the filter cancels. Runs in native-test-behavior.
*/
#include <math.h>
#include <stdint.h>
#include <unity.h>

#include "../../src/audio/EchoCanceller.h"

namespace {

struct Rng {
    uint32_t s = 12345;
    float next() {
        s = s * 1664525U + 1013904223U;
        return static_cast<float>(static_cast<int32_t>(s)) / 2147483648.0f;
    }
};

// The speaker -> mic path: 5 samples of flight, then 60 taps decaying.
struct Path {
    float h[65] = {};
    float hist[65] = {};
    size_t pos = 0;
    Path() {
        Rng r;
        r.s = 777;
        for (int i = 5; i < 65; ++i) {
            h[i] = 0.6f * r.next() * expf(-(i - 5) / 15.0f);
        }
    }
    float push(float x) {
        pos = (pos + 64) % 65;
        hist[pos] = x;
        float y = 0.0f;
        for (int i = 0; i < 65; ++i) {
            y += h[i] * hist[(pos + i) % 65];
        }
        return y;
    }
};

float db(double num, double den) {
    return 10.0f * static_cast<float>(log10((num + 1e-18) / (den + 1e-18)));
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_learns_the_path_and_cancels() {
    EchoCanceller ec;
    TEST_ASSERT_TRUE(ec.begin(128));
    Path path;
    Rng ref, noise;
    noise.s = 99;
    double mic2 = 0, err2 = 0;
    for (int n = 0; n < 72000; ++n) {
        const float r = ref.next();
        const float mic = path.push(r) + 0.001f * noise.next();
        const float e = ec.process(r, mic);
        if (n >= 60000) {
            mic2 += mic * mic;
            err2 += e * e;
        }
    }
    TEST_ASSERT_TRUE(db(mic2, err2) > 20.0f);
    TEST_ASSERT_TRUE(ec.erleDb() > 10.0f);
}

void test_a_loud_other_voice_does_not_throw_it() {
    EchoCanceller ec;
    TEST_ASSERT_TRUE(ec.begin(128));
    Path path;
    Rng ref, near;
    near.s = 4242;
    double mic2 = 0, err2 = 0;
    for (int n = 0; n < 96000; ++n) {
        const float r = ref.next();
        float mic = path.push(r);
        if (n >= 48000 && n < 60000) {
            mic += 1.0f * near.next();  // a voice twice the echo's level for half a second
        }
        const float e = ec.process(r, mic);
        if (n >= 84000) {
            mic2 += mic * mic;
            err2 += e * e;
        }
    }
    TEST_ASSERT_TRUE(db(mic2, err2) > 15.0f);
}

void test_silent_reference_passes_the_mic() {
    EchoCanceller ec;
    TEST_ASSERT_TRUE(ec.begin(64));
    Rng near;
    for (int n = 0; n < 2400; ++n) {
        const float m = near.next();
        TEST_ASSERT_EQUAL_FLOAT(m, ec.process(0.0f, m));
    }
}

void test_a_room_voice_passes_while_cancelling() {
    EchoCanceller ec;
    TEST_ASSERT_TRUE(ec.begin(128));
    Path path;
    Rng ref, voice;
    voice.s = 31337;
    for (int n = 0; n < 72000; ++n) {  // learn first
        const float r = ref.next();
        ec.process(r, path.push(r));
    }
    double v2 = 0, diff2 = 0;
    for (int n = 0; n < 12000; ++n) {
        const float r = ref.next();
        const float v = 0.3f * voice.next();
        const float e = ec.process(r, path.push(r) + v);
        v2 += v * v;
        diff2 += (e - v) * (e - v);
    }
    TEST_ASSERT_TRUE(db(v2, diff2) > 10.0f);  // what comes out is mostly the voice
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_learns_the_path_and_cancels);
    RUN_TEST(test_a_loud_other_voice_does_not_throw_it);
    RUN_TEST(test_silent_reference_passes_the_mic);
    RUN_TEST(test_a_room_voice_passes_while_cancelling);
    return UNITY_END();
}
