/*
SoundOutput on the host (OUT-002/003, step 10f): the request paths the Node
uses (hop train, idle call with its announcement and gap, plain pattern), the
physical-limit checks, and stop() during the idle call's gap. The real
ChirpOutput drives a fake ToneOutput that times a pulse train like the I2S
device does. Time is the host clock (app/TimeBase.h). Runs in
native-test-behavior.
*/
#include <unity.h>

#include "../../src/output/ChirpOutput.cpp"
#include "../../src/audio/ClipShape.h"
#include "../../src/output/ClipEnvelope.h"
#include "../../src/output/SoundOutput.cpp"

namespace {

void setNowMs(unsigned long ms) {
    timebase::host::setNowUs64(static_cast<uint64_t>(ms) * 1000ULL);
}

class FakeTone : public ToneOutput {
public:
    void begin() override {}
    void setToneHz(uint32_t hz) override { toneHz = hz; }
    void toneOn() override {
        on = true;
        ++toneOns;
        lastOnHz = toneHz;
    }
    void toneOff() override {
        on = false;
        trainEndMs = 0;
    }
    bool startPulseTrain(uint32_t count, uint32_t onMs, uint32_t gapMs, uint32_t) override {
        trainEndMs = timebase::nowMs() + count * onMs + (count - 1) * gapMs;
        trainCount = count;
        ++trains;
        return true;
    }
    void setPulseHop(const uint32_t* hz, size_t n) override {
        hopSteps = n;
        for (size_t k = 0; k < n && k < 8; ++k) {
            hop[k] = hz[k];
        }
    }
    bool pulseTrainActive() const override { return trainEndMs != 0 && timebase::nowMs() < trainEndMs; }
    // A clip plays at 48 samples a millisecond (the echo shell's rate).
    bool startClip(const int16_t* samples, size_t count, float gain) override {
        clip = samples;
        clipGain = gain;
        clipEndMs = timebase::nowMs() + count / 48;
        ++clips;
        return true;
    }
    bool clipActive() const override { return clipEndMs != 0 && timebase::nowMs() < clipEndMs; }
    const int16_t* clip = nullptr;
    float clipGain = 0.0f;
    unsigned long clipEndMs = 0;
    int clips = 0;

    uint32_t toneHz = 0;
    uint32_t lastOnHz = 0;
    bool on = false;
    int toneOns = 0;
    int trains = 0;
    uint32_t trainCount = 0;
    unsigned long trainEndMs = 0;
    uint32_t hop[8] = {};
    size_t hopSteps = 0;
};

OutputRequest hopRequest() {
    OutputRequest r;
    r.kind = OutputRequest::Kind::HopTrain;
    const uint32_t hz[5] = {2950, 3200, 3450, 3700, 3950};
    for (size_t k = 0; k < 5; ++k) {
        r.hopHz[k] = hz[k];
    }
    r.hopSteps = 5;
    r.pulseMs = 25;
    r.gapMs = 10;
    return r;
}

OutputRequest idleCall(bool hop) {
    OutputRequest r = hop ? hopRequest() : OutputRequest();
    if (!hop) {
        r.hopSteps = 0;
    }
    r.kind = OutputRequest::Kind::IdleCall;
    r.announceHz = 300;
    r.announceMs = 500;
    r.announceGapMs = 200;
    return r;
}

// Runs update() every ms from `from` to `to`; returns the time of the first event matching `mask`, or 0.
unsigned long runUntil(SoundOutput& out, unsigned long from, unsigned long to, uint8_t mask) {
    for (unsigned long t = from; t <= to; ++t) {
        setNowMs(t);
        if ((out.update() & mask) != 0) {
            return t;
        }
    }
    return 0;
}

}  // namespace

void setUp() {
    setNowMs(1000);
}
void tearDown() {}

void test_hop_train_plays_the_bins_and_finishes_after_its_span() {
    FakeTone tone;
    ChirpOutput chirp(tone, 3200);
    SoundOutput out(chirp);
    TEST_ASSERT_TRUE(out.start(hopRequest()));
    TEST_ASSERT_EQUAL_UINT32(165, out.status().spanMs);
    TEST_ASSERT_EQUAL_UINT32(5, tone.trainCount);
    TEST_ASSERT_EQUAL_UINT32(5, tone.hopSteps);
    TEST_ASSERT_EQUAL_UINT32(3950, tone.hop[4]);
    TEST_ASSERT_TRUE(out.busy());
    TEST_ASSERT_EQUAL_UINT32(1165, runUntil(out, 1001, 2000, SoundOutput::kEventFinished));
    TEST_ASSERT_FALSE(out.busy());
    TEST_ASSERT_EQUAL_UINT32(1165, out.finishedAtMs());
    TEST_ASSERT_EQUAL_UINT32(1, out.status().started);
    TEST_ASSERT_EQUAL_UINT32(1, out.status().done);
}

void test_idle_call_announces_waits_then_plays_the_train() {
    FakeTone tone;
    ChirpOutput chirp(tone, 3200);
    SoundOutput out(chirp);
    TEST_ASSERT_TRUE(out.start(idleCall(true)));
    TEST_ASSERT_EQUAL_UINT32(300, tone.lastOnHz);
    TEST_ASSERT_EQUAL_UINT32(500 + 200 + 165, out.status().spanMs);
    TEST_ASSERT_EQUAL(OutputPhase::Announce, out.status().phase);
    // Announcement ends at 1500, the gap at 1700: the call starts then, at the chirp tone's grid.
    const unsigned long callAt = runUntil(out, 1001, 3000, SoundOutput::kEventCallStarted);
    TEST_ASSERT_EQUAL_UINT32(1700, callAt);
    TEST_ASSERT_TRUE(out.callIsHop());
    TEST_ASSERT_EQUAL_UINT32(1, tone.trains);
    TEST_ASSERT_EQUAL_UINT32(3200, tone.toneHz);  // tone restored after the announcement
    TEST_ASSERT_EQUAL_UINT32(1865, runUntil(out, callAt + 1, 3000, SoundOutput::kEventFinished));
    TEST_ASSERT_EQUAL_UINT32(1, out.status().done);
}

void test_idle_call_without_hop_ends_with_the_plain_chirp() {
    FakeTone tone;
    ChirpOutput chirp(tone, 3200);
    SoundOutput out(chirp);
    TEST_ASSERT_TRUE(out.start(idleCall(false)));
    TEST_ASSERT_EQUAL_UINT32(500 + 200 + 100, out.status().spanMs);
    TEST_ASSERT_EQUAL_UINT32(1700, runUntil(out, 1001, 3000, SoundOutput::kEventCallStarted));
    TEST_ASSERT_FALSE(out.callIsHop());
    TEST_ASSERT_EQUAL_UINT32(3200, tone.lastOnHz);
    TEST_ASSERT_EQUAL_UINT32(1800, runUntil(out, 1701, 3000, SoundOutput::kEventFinished));
}

void test_stop_in_the_gap_cancels_the_call() {
    FakeTone tone;
    ChirpOutput chirp(tone, 3200);
    SoundOutput out(chirp);
    TEST_ASSERT_TRUE(out.start(idleCall(true)));
    runUntil(out, 1001, 1600, 0);
    TEST_ASSERT_EQUAL(OutputPhase::AnnounceGap, out.status().phase);
    out.stop();
    TEST_ASSERT_FALSE(out.busy());
    TEST_ASSERT_EQUAL_UINT32(0,
                             runUntil(out, 1601, 3000, SoundOutput::kEventCallStarted | SoundOutput::kEventFinished));
    TEST_ASSERT_EQUAL_INT(0, tone.trains);
}

void test_limits_reject_and_nothing_sounds() {
    FakeTone tone;
    ChirpOutput chirp(tone, 3200);
    SoundOutput out(chirp);
    OutputRequest r = hopRequest();
    r.hopHz[2] = 9000;
    TEST_ASSERT_FALSE(out.start(r));
    TEST_ASSERT_EQUAL(OutputReject::FrequencyOutOfRange, out.status().lastReject);
    r = hopRequest();
    r.hopSteps = 0;
    TEST_ASSERT_FALSE(out.start(r));
    TEST_ASSERT_EQUAL(OutputReject::NoSteps, out.status().lastReject);
    r = hopRequest();
    r.pulseMs = 1001;
    TEST_ASSERT_FALSE(out.start(r));
    TEST_ASSERT_EQUAL(OutputReject::PulseOutOfRange, out.status().lastReject);
    r = idleCall(true);
    r.announceMs = 2000;
    r.announceGapMs = 2000;
    r.gapMs = 1000;  // 2000 + 2000 + 5x25 + 4x1000 = 8125 ms
    TEST_ASSERT_FALSE(out.start(r));
    TEST_ASSERT_EQUAL(OutputReject::SpanTooLong, out.status().lastReject);
    TEST_ASSERT_EQUAL_UINT32(4, out.status().rejected);
    TEST_ASSERT_EQUAL_INT(0, tone.toneOns);
    TEST_ASSERT_EQUAL_INT(0, tone.trains);
    TEST_ASSERT_EQUAL_UINT32(0, out.status().started);
}

void test_a_second_request_while_busy_is_rejected() {
    FakeTone tone;
    ChirpOutput chirp(tone, 3200);
    SoundOutput out(chirp);
    TEST_ASSERT_TRUE(out.start(hopRequest()));
    OutputRequest p;
    TEST_ASSERT_FALSE(out.start(p));
    TEST_ASSERT_EQUAL(OutputReject::Busy, out.status().lastReject);
    TEST_ASSERT_EQUAL_INT(1, tone.trains);
}

void test_pattern_is_timed_by_chirp_output() {
    FakeTone tone;
    ChirpOutput chirp(tone, 3200);
    SoundOutput out(chirp);
    OutputRequest p;  // Pattern, Single: 100 ms at the chirp tone
    TEST_ASSERT_TRUE(out.start(p));
    TEST_ASSERT_EQUAL_UINT32(0, out.status().spanMs);
    TEST_ASSERT_EQUAL_UINT32(3200, tone.lastOnHz);
    TEST_ASSERT_EQUAL_UINT32(1100, runUntil(out, 1001, 2000, SoundOutput::kEventFinished));
}

// An idle call puts back the chirp setup it found, not the build defaults; a caller
// that configured the chirp (tone, beep and pause length) keeps its setup after the announcement.
void test_idle_call_restores_the_callers_chirp_setup() {
    FakeTone tone;
    ChirpOutput chirp(tone, 3200);
    chirp.setToneHz(3450);
    chirp.setTiming(60, 40);
    SoundOutput out(chirp);
    TEST_ASSERT_TRUE(out.start(idleCall(false)));
    TEST_ASSERT_EQUAL_UINT32(300, tone.lastOnHz);
    runUntil(out, 1001, 1600, 0);  // the announcement is over, the gap runs
    TEST_ASSERT_EQUAL(OutputPhase::AnnounceGap, out.status().phase);
    TEST_ASSERT_EQUAL_UINT32(3450, chirp.toneHz());
    TEST_ASSERT_EQUAL_UINT32(60, chirp.chirpOnMs());
    TEST_ASSERT_EQUAL_UINT32(40, chirp.chirpPauseMs());
    // stop() during the announcement restores it as well
    FakeTone tone2;
    ChirpOutput chirp2(tone2, 3200);
    chirp2.setTiming(70, 30);
    SoundOutput out2(chirp2);
    TEST_ASSERT_TRUE(out2.start(idleCall(true)));
    out2.stop();
    TEST_ASSERT_EQUAL_UINT32(3200, chirp2.toneHz());
    TEST_ASSERT_EQUAL_UINT32(70, chirp2.chirpOnMs());
    TEST_ASSERT_EQUAL_UINT32(30, chirp2.chirpPauseMs());
}

// ECHO-001: a clip request goes to the device as it is and ends when the device says so.
void test_clip_plays_and_finishes() {
    setNowMs(1000);
    FakeTone tone;
    ChirpOutput chirp(tone, 3200);
    SoundOutput out(chirp);
    static int16_t samples[4800] = {};
    OutputRequest r;
    r.kind = OutputRequest::Kind::Clip;
    r.clipSamples = samples;
    r.clipCount = 4800;
    r.clipMs = 100;
    r.clipGain = 0.5f;
    TEST_ASSERT_TRUE(out.start(r));
    TEST_ASSERT_EQUAL(1, tone.clips);
    TEST_ASSERT_EQUAL_PTR(samples, tone.clip);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, tone.clipGain);
    TEST_ASSERT_TRUE(out.busy());
    setNowMs(1050);
    out.update();
    TEST_ASSERT_TRUE(out.busy());
    setNowMs(1101);
    TEST_ASSERT_EQUAL_UINT8(SoundOutput::kEventFinished, out.update());
    TEST_ASSERT_FALSE(out.busy());
    TEST_ASSERT_EQUAL_UINT32(100, out.status().spanMs);
}

void test_clip_without_samples_is_rejected() {
    OutputRequest r;
    r.kind = OutputRequest::Kind::Clip;
    TEST_ASSERT_EQUAL(static_cast<int>(OutputReject::NoClip),
                      static_cast<int>(SoundOutput::validate(r, OutputLimits())));
    static int16_t one[1] = {};
    r.clipSamples = one;
    r.clipCount = 1;
    r.clipMs = 7000;  // longer than OutputLimits.maxSpanMs
    TEST_ASSERT_EQUAL(static_cast<int>(OutputReject::SpanTooLong),
                      static_cast<int>(SoundOutput::validate(r, OutputLimits())));
}

void test_clip_envelope_ramps_in_and_out() {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f / 11.0f, clipEnvelope(0, 1000, 10));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, clipEnvelope(500, 1000, 10));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f / 11.0f, clipEnvelope(999, 1000, 10));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, clipEnvelope(1000, 1000, 10));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, clipEnvelope(0, 1, 0));
    // Shorter than two ramps: each ramp is half the clip.
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f / 3.0f, clipEnvelope(0, 4, 10));
}

// ECHO-004: damping 0 and fade 0 leave the clip as recorded; the fade ends at 0 and rises linearly before it.
void test_clip_shape_fade_and_bypass() {
    int16_t c[100];
    for (int i = 0; i < 100; ++i) {
        c[i] = 1000;
    }
    clipshape::shape(c, 100, 24000, 0, 0);
    TEST_ASSERT_EQUAL_INT16(1000, c[0]);
    TEST_ASSERT_EQUAL_INT16(1000, c[99]);
    clipshape::shape(c, 100, 24000, 0, 1);  // 1 ms = 24 samples
    TEST_ASSERT_EQUAL_INT16(0, c[99]);
    TEST_ASSERT_EQUAL_INT16(500, c[99 - 12]);
    TEST_ASSERT_EQUAL_INT16(1000, c[99 - 24]);
    TEST_ASSERT_EQUAL_INT16(1000, c[0]);
}

// The damping's corner: the clip's top at 0 %, 1 kHz at 100 %, log-spaced; a 6 kHz tone loses more the more damped.
void test_clip_shape_damping_dulls_highs() {
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, clipshape::cornerHz(0, 24000));
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 1000.0f, clipshape::cornerHz(100, 24000));
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 3464.0f, clipshape::cornerHz(50, 24000));
    auto tonePeak = [](uint32_t damping) {
        static int16_t c[2400];
        for (int i = 0; i < 2400; ++i) {
            c[i] = static_cast<int16_t>(10000.0f * sinf(2.0f * 3.14159265f * 6000.0f * i / 24000.0f));
        }
        clipshape::shape(c, 2400, 24000, damping, 0);
        int peak = 0;
        for (int i = 1200; i < 2400; ++i) {
            peak = c[i] > peak ? c[i] : peak;
        }
        return peak;
    };
    const int none = tonePeak(0);
    const int some = tonePeak(30);
    const int much = tonePeak(80);
    TEST_ASSERT_EQUAL_INT(10000, none);
    TEST_ASSERT_TRUE(some < none);
    TEST_ASSERT_TRUE(much < some / 2);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_hop_train_plays_the_bins_and_finishes_after_its_span);
    RUN_TEST(test_idle_call_announces_waits_then_plays_the_train);
    RUN_TEST(test_idle_call_without_hop_ends_with_the_plain_chirp);
    RUN_TEST(test_stop_in_the_gap_cancels_the_call);
    RUN_TEST(test_limits_reject_and_nothing_sounds);
    RUN_TEST(test_a_second_request_while_busy_is_rejected);
    RUN_TEST(test_pattern_is_timed_by_chirp_output);
    RUN_TEST(test_idle_call_restores_the_callers_chirp_setup);
    RUN_TEST(test_clip_plays_and_finishes);
    RUN_TEST(test_clip_without_samples_is_rejected);
    RUN_TEST(test_clip_envelope_ramps_in_and_out);
    RUN_TEST(test_clip_shape_fade_and_bypass);
    RUN_TEST(test_clip_shape_damping_dulls_highs);
    return UNITY_END();
}
