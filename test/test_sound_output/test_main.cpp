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

// Review A12 (step 11f): an idle call puts back the chirp setup it found, not the build defaults; a caller
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
    return UNITY_END();
}
