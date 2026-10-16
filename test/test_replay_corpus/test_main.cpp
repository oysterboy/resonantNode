/*
NODE-006: the detection chain replays recorded captures on the host.

Fixtures (test/fixtures, copied from bench/sessions/2026-10-16-corpus-*, E2/E3
facing up, combined build, mode=both: PCM plus the board's amp feature):
  chirp3200_90cm_2026-10-16.{wav,csv}  plain 3,200 Hz chirp, 100 ms, 90 cm; the board's chirp cell at
                                        this placement (single target, no band) was 10/10
  tone3300_90cm_2026-10-16.csv          3,300 Hz, 100 ms, 90 cm: between two bins of the network band
  radio_speech_110cm_2026-10-16.wav     350 ms of phone-radio speech with a 20 ms marker tone
                                        at the start (skip the first 100 ms)
Run with `pio test -e native-test` (needs a host C++ compiler on PATH).

What the expectations encode (2026-10-16 replay, docs/lab/2026-10-16-bench-day.md section 9):
  - single target 3,200 Hz: the chirp capture is accepted once, as on the board;
  - band 2950-3950 (the network band, bins 2950/3200/3450/3700/3950): the on-bin 3,200 Hz chirp is
    accepted, the off-bin 3,300 Hz tone is rejected at the contrast inspector (its energy splits over
    bins 3200 and 3450). Hop steps and any plain chirp a band-mode node should hear must sit on the bins.
*/
#include <unity.h>

#include <string>

#include "../../src/detection/DetectionProfile.h"
#include "../../src/detection/evaluation/OccurrenceVerdict.h"
#include "../../src/host/Replay.h"

namespace {

const char* kChirpWav = "test/fixtures/chirp3200_90cm_2026-10-16.wav";
const char* kChirpCsv = "test/fixtures/chirp3200_90cm_2026-10-16.csv";
const char* kTone3300Csv = "test/fixtures/tone3300_90cm_2026-10-16.csv";
const char* kSpeechWav = "test/fixtures/radio_speech_110cm_2026-10-16.wav";

hostreplay::Options singleTarget3200() {
    hostreplay::Options opt;
    opt.bandLow = 0;
    opt.bandHigh = 0;
    opt.targetHz = 3200;
    return opt;
}

hostreplay::Options networkBand() {
    hostreplay::Options opt;
    opt.bandLow = 2950;
    opt.bandHigh = 3950;
    return opt;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_wav_loads_the_capture() {
    hostreplay::Capture cap;
    TEST_ASSERT_TRUE(hostreplay::loadCapture(kChirpWav, cap));
    TEST_ASSERT_EQUAL_UINT32(16000, cap.sampleRateHz);
    TEST_ASSERT_TRUE(cap.pcm.size() > 4000 && cap.pcm.size() < 5000);  // 50 ms pre + 250 ms post
}

void test_csv_and_wav_hold_the_same_samples() {
    hostreplay::Capture wav;
    hostreplay::Capture csv;
    TEST_ASSERT_TRUE(hostreplay::loadCapture(kChirpWav, wav));
    TEST_ASSERT_TRUE(hostreplay::loadCapture(kChirpCsv, csv));
    TEST_ASSERT_EQUAL_UINT32(csv.pcm.size(), wav.pcm.size());
    TEST_ASSERT_EQUAL_STRING("both", csv.mode.c_str());
    TEST_ASSERT_EQUAL_UINT32(csv.pcm.size(), csv.boardAmp.size());
    for (size_t i = 0; i < csv.pcm.size(); i += 97) {
        TEST_ASSERT_EQUAL_INT32(csv.pcm[i], wav.pcm[i]);
    }
}

// Single target at 3,200 Hz (the board's chirp cell): accepted once.
void test_chirp_is_accepted_once_on_the_single_target() {
    hostreplay::Capture cap;
    TEST_ASSERT_TRUE(hostreplay::loadCapture(kChirpCsv, cap));
    const detection::DetectionProfile profile = detection::makeTonalPulseScalarProfile();
    const hostreplay::Result r = hostreplay::replay(cap, singleTarget3200(), profile);
    TEST_ASSERT_EQUAL_UINT32(1, r.accepted);
    TEST_ASSERT_EQUAL_UINT32(0, r.rejected);
    TEST_ASSERT_TRUE(r.verdicts.front().primaryDurationMs >= 85 && r.verdicts.front().primaryDurationMs <= 300);
}

// Band mode: the on-bin 3,200 Hz chirp is accepted.
void test_on_bin_tone_is_accepted_in_band_mode() {
    hostreplay::Capture cap;
    TEST_ASSERT_TRUE(hostreplay::loadCapture(kChirpCsv, cap));
    const detection::DetectionProfile profile = detection::makeTonalPulseScalarProfile();
    const hostreplay::Result r = hostreplay::replay(cap, networkBand(), profile);
    TEST_ASSERT_EQUAL_UINT32(1, r.accepted);
    TEST_ASSERT_EQUAL_UINT32(0, r.rejected);
}

// Band mode: the off-bin 3,300 Hz tone fails the contrast gate (support_too_low on contrast).
void test_off_bin_tone_fails_the_contrast_gate_in_band_mode() {
    hostreplay::Capture cap;
    TEST_ASSERT_TRUE(hostreplay::loadCapture(kTone3300Csv, cap));
    const detection::DetectionProfile profile = detection::makeTonalPulseScalarProfile();
    const hostreplay::Result r = hostreplay::replay(cap, networkBand(), profile);
    TEST_ASSERT_EQUAL_UINT32(0, r.accepted);
    TEST_ASSERT_EQUAL_UINT32(1, r.rejected);
    TEST_ASSERT_EQUAL(detection::InspectionTarget::Contrast, r.verdicts.front().firstFailedRequirementTarget);
}

// The host amp feature equals the board's recorded amp column on the same
// samples (mode=both) once the board's baseline (RAW_SUMMARY) is seeded:
// the two chains centre and scale the signal identically (2026-10-16: max
// difference 1 Strength16 over all 48 corpus captures, rounding).
void test_host_amp_feature_matches_the_board() {
    hostreplay::Capture cap;
    TEST_ASSERT_TRUE(hostreplay::loadCapture(kChirpCsv, cap));
    TEST_ASSERT_TRUE(cap.hasBoardBaseline);
    hostreplay::Options opt = singleTarget3200();
    opt.compare = true;
    const detection::DetectionProfile profile = detection::makeTonalPulseScalarProfile();
    const hostreplay::Result r = hostreplay::replay(cap, opt, profile);
    TEST_ASSERT_TRUE(r.ampCompared > 4000);
    TEST_ASSERT_TRUE_MESSAGE(r.ampMaxAbsDiff <= 1.0, "max |host amp - board amp| (Strength16)");
}

// A contrast limit above what any chirp reaches rejects the same capture at the inspector.
void test_profile_override_reaches_the_inspector() {
    hostreplay::Capture cap;
    TEST_ASSERT_TRUE(hostreplay::loadCapture(kChirpCsv, cap));
    detection::DetectionProfile profile = detection::makeTonalPulseScalarProfile();
    TEST_ASSERT_TRUE(hostreplay::applyProfileSet(profile, "contrast_medium", 40000.0));
    const hostreplay::Result r = hostreplay::replay(cap, singleTarget3200(), profile);
    TEST_ASSERT_EQUAL_UINT32(0, r.accepted);
    TEST_ASSERT_EQUAL_UINT32(1, r.rejected);
}

// Radio speech (after the marker tone) produces no accepted occurrence with the shipped profile.
void test_radio_speech_is_not_accepted() {
    hostreplay::Capture cap;
    TEST_ASSERT_TRUE(hostreplay::loadCapture(kSpeechWav, cap));
    hostreplay::Options opt = networkBand();
    opt.skipMs = 100;
    const detection::DetectionProfile profile = detection::makeTonalPulseScalarProfile();
    const hostreplay::Result r = hostreplay::replay(cap, opt, profile);
    TEST_ASSERT_EQUAL_UINT32(0, r.accepted);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_wav_loads_the_capture);
    RUN_TEST(test_csv_and_wav_hold_the_same_samples);
    RUN_TEST(test_chirp_is_accepted_once_on_the_single_target);
    RUN_TEST(test_on_bin_tone_is_accepted_in_band_mode);
    RUN_TEST(test_off_bin_tone_fails_the_contrast_gate_in_band_mode);
    RUN_TEST(test_host_amp_feature_matches_the_board);
    RUN_TEST(test_profile_override_reaches_the_inspector);
    RUN_TEST(test_radio_speech_is_not_accepted);
    return UNITY_END();
}
