/*
NODE-006: the detection chain replays recorded captures on the host.

Fixtures (test/fixtures, copied from bench/sessions/2026-10-16-corpus-*, E2/E3
facing up, combined build, mode=both: PCM plus the board's amp feature):
  chirp3200_90cm_2026-10-16.{wav,csv}  plain 3,200 Hz chirp, 100 ms, 90 cm; the board's chirp cell at
                                        this placement (single target, no band) was 10/10
  tone3300_90cm_2026-10-16.csv          3,300 Hz, 100 ms, 90 cm: between two bins of the network band
  radio_speech_110cm_2026-10-16.wav     350 ms of phone-radio speech with a 20 ms marker tone
                                        at the start (skip the first 100 ms)
  hop5_25x5g10_70cm_2026-10-16.csv      the Node's network signal as shipped: 25 ms x 5, gap 10, on the
                                        bins 2950/3200/3450/3700/3950, 70 cm, E3 0.5 FS, shipped build
                                        5ba805d (bench corpus-70cm hop5_p25g10_pos1_r0; the board and the
                                        replay both accepted all three of that session's hop captures)
  hop5_25x5g10_70cm_2026-10-16_{dry,wet}_gN.csv
                                        variants of the hop capture from tools/bench/hopvariants.py (step 5h,
                                        assumption 1): dry_gN keeps the five step windows and replaces the gaps
                                        and the tail after the train with the capture's pre-roll noise, gaps of
                                        N = 10/15/20/30 ms; wet_gN keeps each recorded 10 ms gap (with the room
                                        tail) and adds N - 10 ms of noise, N = 15/20/30. Step windows from the
                                        send plan, onset 57.2 ms (first 1 ms RMS at 20 % of the peak)
Fixture provenance: all four were captured on the DC-blocker build (PCM_PREPROCESS_MODE_VALUE 2); RAW_BEGIN
lines from builds after 2026-10-16 carry `preprocess=` and Capture::preprocess reads it.
Run with `pio test -e native-test` (needs a host C++ compiler on PATH).

What the expectations encode (2026-10-16 replay, docs/lab/2026-10-16-bench-day.md section 9):
  - single target 3,200 Hz: the chirp capture is accepted once, as on the board;
  - band 2950-3950 (the network band, bins 2950/3200/3450/3700/3950): the on-bin 3,200 Hz chirp is
    accepted, the off-bin 3,300 Hz tone is rejected at the contrast inspector (its energy splits over
    bins 3200 and 3450). Hop steps and any plain chirp a band-mode node should hear must sit on the bins.
*/
#include <unity.h>

#include <string>

#include <math.h>

#include "../../src/app/RuntimeDefaults.h"
#include "../../src/detection/DetectionProfile.h"
#include "../../src/detection/HopGapContract.h"
#include "../../src/detection/evaluation/OccurrenceVerdict.h"
#include "../../src/detection/features/FreqBandStream.h"
#include "../../src/host/Replay.h"

namespace {

const char* kChirpWav = "test/fixtures/chirp3200_90cm_2026-10-16.wav";
const char* kChirpCsv = "test/fixtures/chirp3200_90cm_2026-10-16.csv";
const char* kTone3300Csv = "test/fixtures/tone3300_90cm_2026-10-16.csv";
const char* kSpeechWav = "test/fixtures/radio_speech_110cm_2026-10-16.wav";
const char* kHopCsv = "test/fixtures/hop5_25x5g10_70cm_2026-10-16.csv";

std::string hopVariant(const char* kind, int gapMs) {
    return std::string("test/fixtures/hop5_25x5g10_70cm_2026-10-16_") + kind + "_g" + std::to_string(gapMs) + ".csv";
}

// Peaking-EQ notch (RBJ biquad) at f0, amplitude gain `gain` at f0, bandwidth bwHz:
// the same filter as tools/bench/notchsweep.py (step 5h item 3), a stand-in for a comb null.
hostreplay::Capture notched(const hostreplay::Capture& in, double f0, double gain, double bwHz) {
    hostreplay::Capture out = in;
    const double a = sqrt(gain);
    const double w0 = 2.0 * PI * f0 / static_cast<double>(in.sampleRateHz);
    const double alpha = sin(w0) / (2.0 * (f0 / bwHz));
    const double a0 = 1.0 + alpha / a;
    const double b0 = (1.0 + alpha * a) / a0, b1 = -2.0 * cos(w0) / a0, b2 = (1.0 - alpha * a) / a0;
    const double a1 = -2.0 * cos(w0) / a0, a2 = (1.0 - alpha / a) / a0;
    double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
    for (size_t i = 0; i < out.pcm.size(); ++i) {
        const double x = static_cast<double>(in.pcm[i]);
        const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        out.pcm[i] = static_cast<int32_t>(lround(y));
    }
    return out;
}

// The train's first step starts at ~59 ms in the capture's axis (accepted start of the recorded train).
constexpr unsigned long kHopTrainStartMaxMs = 65;

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

// The network signal (the Node's hop train on the band bins) is read as one
// occurrence in band mode: the 5 x 25 ms train with 10 ms gaps is bridged by
// the scalar detector's gap/debounce logic (DET-011) and passes both
// inspectors. This is the first host regression for the signal the nodes
// actually exchange; the duration bound is the profile's 85..300 ms window.
void test_hop_train_is_accepted_once_in_band_mode() {
    hostreplay::Capture cap;
    TEST_ASSERT_TRUE(hostreplay::loadCapture(kHopCsv, cap));
    TEST_ASSERT_EQUAL_STRING("f=3200 dur=25 pulses=5 gap=10 hop=2950,3200,3450,3700,3950", cap.shape.c_str());
    const detection::DetectionProfile profile = detection::makeTonalPulseScalarProfile();
    const hostreplay::Result r = hostreplay::replay(cap, networkBand(), profile);
    TEST_ASSERT_EQUAL_UINT32(1, r.accepted);
    TEST_ASSERT_EQUAL_UINT32(0, r.rejected);
    const unsigned long duration = r.verdicts.front().primaryDurationMs;
    TEST_ASSERT_TRUE_MESSAGE(duration >= 150 && duration <= 300, "hop train duration outside 150..300 ms");
}

// Step 5h, assumption 1: the room tail, not the detector, bridges the 10 ms gaps.
// Shipped scalar values: release debounce 10, max gap 20, cooldown 50, min duration 85.
// Without the tail a 10 ms gap reads as a dip of about the debounce: the candidate
// closes after the first step (25 ms, too short) and the 50 ms onset cooldown keeps the
// next step from starting a new one. What survives (if anything) starts inside the
// train, never at its start. The onset of the step windows was varied 54..59 ms when
// this was written (2026-10-20): dry g10 kept a 115 ms rest from step 2 at 57-58 ms
// and nothing at the others; this assertion holds at all six.
void test_dry_hop_train_loses_its_first_step() {
    hostreplay::Capture cap;
    TEST_ASSERT_TRUE(hostreplay::loadCapture(hopVariant("dry", 10), cap));
    TEST_ASSERT_EQUAL_STRING("f=3200 dur=25 pulses=5 gap=10 hop=2950,3200,3450,3700,3950", cap.shape.c_str());
    const detection::DetectionProfile profile = detection::makeTonalPulseScalarProfile();
    const hostreplay::Result r = hostreplay::replay(cap, networkBand(), profile);
    for (const detection::OccurrenceVerdict& v : r.verdicts) {
        if (v.accepted) {
            TEST_ASSERT_TRUE_MESSAGE(v.primaryStartMs > kHopTrainStartMaxMs, "a dry train was read from its first step");
        }
    }
}

// Dry gaps of 15 ms and more: no occurrence at all (every step is a short reject).
void test_dry_gaps_of_15_ms_and_more_are_not_heard() {
    const detection::DetectionProfile profile = detection::makeTonalPulseScalarProfile();
    const int gaps[] = {15, 20, 30};
    for (int g : gaps) {
        hostreplay::Capture cap;
        TEST_ASSERT_TRUE(hostreplay::loadCapture(hopVariant("dry", g), cap));
        const hostreplay::Result r = hostreplay::replay(cap, networkBand(), profile);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, r.accepted, hopVariant("dry", g).c_str());
    }
}

// With this room's tail a 15 ms gap is still one train from its first step; 30 ms is not.
// (wet g20 is accepted at the default onset but not at every onset: not asserted.)
void test_room_tail_bridges_15_ms_but_not_30_ms() {
    const detection::DetectionProfile profile = detection::makeTonalPulseScalarProfile();
    hostreplay::Capture g15;
    TEST_ASSERT_TRUE(hostreplay::loadCapture(hopVariant("wet", 15), g15));
    const hostreplay::Result r15 = hostreplay::replay(g15, networkBand(), profile);
    TEST_ASSERT_EQUAL_UINT32(1, r15.accepted);
    TEST_ASSERT_TRUE(r15.verdicts.front().primaryStartMs <= kHopTrainStartMaxMs);
    hostreplay::Capture g30;
    TEST_ASSERT_TRUE(hostreplay::loadCapture(hopVariant("wet", 30), g30));
    const hostreplay::Result r30 = hostreplay::replay(g30, networkBand(), profile);
    TEST_ASSERT_EQUAL_UINT32(0, r30.accepted);
}

// The lever, measured, not shipped: a release debounce above the gap (20 against 10/15)
// reads the dry train as one occurrence from its first step, at every onset 54..59 ms.
// Changing the shipped value is a tuning pass of its own (CLAUDE.md; step 5h "Levers").
void test_release_debounce_above_the_gap_bridges_the_dry_train() {
    detection::DetectionProfile profile = detection::makeTonalPulseScalarProfile();
    TEST_ASSERT_TRUE(hostreplay::applyProfileSet(profile, "scalar_release_debounce_ms", 20.0));
    const int gaps[] = {10, 15};
    for (int g : gaps) {
        hostreplay::Capture cap;
        TEST_ASSERT_TRUE(hostreplay::loadCapture(hopVariant("dry", g), cap));
        const hostreplay::Result r = hostreplay::replay(cap, networkBand(), profile);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, r.accepted, hopVariant("dry", g).c_str());
        TEST_ASSERT_TRUE(r.verdicts.front().primaryStartMs <= kHopTrainStartMaxMs);
        TEST_ASSERT_TRUE(r.verdicts.front().primaryDurationMs >= 150);
    }
}

// Step 5h item 3, assumption 2 (one null per spacing): one deep null (1/7 in amplitude,
// 125 Hz wide) at any of the five bins still leaves one accepted occurrence; two deep
// nulls at 3450 + 3700 (the train's middle steps on this rotation) do not: the detector
// splits the train into pieces under the 85 ms minimum. The contrast gate is not what
// fails: every accepted variant's contrast P75 stays >= 31,273 (shipped 30,500), see
// docs/lab/2026-10-20-step5h-host-checks.md for the full sweep.
void test_one_deep_null_is_survived_two_in_the_middle_are_not() {
    hostreplay::Capture cap;
    TEST_ASSERT_TRUE(hostreplay::loadCapture(kHopCsv, cap));
    const detection::DetectionProfile profile = detection::makeTonalPulseScalarProfile();
    const double bins[] = {2950, 3200, 3450, 3700, 3950};
    for (double f : bins) {
        const hostreplay::Result r = hostreplay::replay(notched(cap, f, 1.0 / 7.0, 125.0), networkBand(), profile);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, r.accepted, "one deep null");
        TEST_ASSERT_TRUE(r.verdicts.front().primaryDurationMs >= 85);
    }
    const hostreplay::Capture two = notched(notched(cap, 3450, 1.0 / 7.0, 125.0), 3700, 1.0 / 7.0, 125.0);
    const hostreplay::Result r2 = hostreplay::replay(two, networkBand(), profile);
    TEST_ASSERT_EQUAL_UINT32(0, r2.accepted);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, r2.rejected, "the split train never reaches the inspectors");
}

// Step 5h item 2: the shipped hop train against the shipped scalar profile
// (detection/HopGapContract.h; the Node prints the same check as RB hop_check).
// Hard checks pass; the soft one, gap < release debounce, does not: 10 = 10, the
// train relies on the room tail (the dry fixtures above). If this assertion
// fails because the debounce or the gap changed, the change was a tuning pass:
// update this test and the HopGapContract note with its bench evidence.
void test_shipped_hop_train_fits_the_scalar_detector() {
    const detection::DetectionProfile profile = detection::makeTonalPulseScalarProfile();
    detection::HopTrainShape shape;
    shape.steps = runtime::kDefaultHopSteps;
    shape.pulseMs = runtime::kDefaultHopPulseMs;
    shape.gapMs = runtime::kDefaultHopGapMs;
    const detection::HopGapCheck c = detection::checkHopGap(shape, profile.scalarTransient);
    TEST_ASSERT_EQUAL_UINT32(165, c.trainMs);
    TEST_ASSERT_TRUE(c.durationFits);
    TEST_ASSERT_TRUE(c.coverageFits);
    TEST_ASSERT_TRUE(c.islandFits);
    TEST_ASSERT_TRUE(c.gapWithinMaxGap);
    TEST_ASSERT_TRUE(c.tailFits);
    TEST_ASSERT_TRUE(c.hardOk());
    TEST_ASSERT_FALSE_MESSAGE(c.gapBelowDebounce, "shipped gap is below the debounce now: see the comment above");
}

// The check names what the replay shows: a 30 ms gap is over max gap (the wet_g30
// fixture is not heard), and a short train is under the minimum duration.
void test_hop_gap_check_flags_what_the_detector_cannot_read() {
    const detection::DetectionProfile profile = detection::makeTonalPulseScalarProfile();
    detection::HopTrainShape wide;
    wide.steps = 5;
    wide.pulseMs = 25;
    wide.gapMs = 30;
    const detection::HopGapCheck w = detection::checkHopGap(wide, profile.scalarTransient);
    TEST_ASSERT_FALSE(w.gapWithinMaxGap);
    TEST_ASSERT_FALSE(w.hardOk());
    detection::HopTrainShape shortTrain;
    shortTrain.steps = 2;
    shortTrain.pulseMs = 25;
    shortTrain.gapMs = 5;
    const detection::HopGapCheck s = detection::checkHopGap(shortTrain, profile.scalarTransient);
    TEST_ASSERT_FALSE(s.durationFits);
    TEST_ASSERT_TRUE(s.gapBelowDebounce);
    detection::DetectionProfile longer = profile;
    TEST_ASSERT_TRUE(hostreplay::applyProfileSet(longer, "scalar_release_debounce_ms", 20.0));
    detection::HopTrainShape shipped;
    shipped.steps = runtime::kDefaultHopSteps;
    shipped.pulseMs = runtime::kDefaultHopPulseMs;
    shipped.gapMs = runtime::kDefaultHopGapMs;
    TEST_ASSERT_TRUE(detection::checkHopGap(shipped, longer.scalarTransient).softOk());
}

// A node listening on the single 3,200 Hz target (band_low_hz = 0) does not
// hear the network signal: only one 25 ms step is on its bin, below the
// 85 ms minimum. The band is the contract, not an option.
void test_hop_train_is_not_heard_on_the_single_target() {
    hostreplay::Capture cap;
    TEST_ASSERT_TRUE(hostreplay::loadCapture(kHopCsv, cap));
    const detection::DetectionProfile profile = detection::makeTonalPulseScalarProfile();
    const hostreplay::Result r = hostreplay::replay(cap, singleTarget3200(), profile);
    TEST_ASSERT_EQUAL_UINT32(0, r.accepted);
}

// Grid consistency (docs/research/unproven-assumptions-audit.md 3.6): the
// detector's bank, the Node's hop train and the chirp all sit on one grid
// defined in RuntimeDefaults.h; the band stream reports the same bin count
// the static checks assume, and the hop fixture's steps are exactly those bins.
void test_band_grid_is_shared_by_detector_and_hop_train() {
    FreqBandStream band;
    band.resetState();
    band.setSampleRateHz(runtime::kDefaultAudioI2SSampleRateHz);
    band.setBand(runtime::kDefaultNetworkBandLowHz, runtime::kDefaultNetworkBandHighHz);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, static_cast<float>(runtime::kBandBinSpacingHz), band.bandSpacingHz());
    TEST_ASSERT_EQUAL_UINT32(5, band.bandBinCount());
    TEST_ASSERT_EQUAL_UINT32(runtime::kDefaultNetworkBandHighHz, band.bandHighHz());
    // A band wider than the bank is clamped at the high edge, and says so.
    band.setBand(1000, 7000);
    TEST_ASSERT_EQUAL_UINT32(FreqBandStream::kMaxBandBins, band.bandBinCount());
    TEST_ASSERT_EQUAL_UINT32(1000 + (FreqBandStream::kMaxBandBins - 1) * runtime::kBandBinSpacingHz, band.bandHighHz());
}

// kDcBlockerOutputScale is the First Difference gain at the chirp tone,
// sin(pi * f / fs), kept as a literal because every capture since bfbe0c8
// used it. A change of the chirp or the sample rate must come back here.
void test_dc_blocker_scale_is_the_first_difference_gain_at_the_chirp() {
    const double expected = sin(PI * static_cast<double>(runtime::kDefaultChirpFrequencyHz)
                                / static_cast<double>(runtime::kDefaultAudioI2SSampleRateHz));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, static_cast<float>(expected), runtime::kDcBlockerOutputScale);
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
    RUN_TEST(test_hop_train_is_accepted_once_in_band_mode);
    RUN_TEST(test_hop_train_is_not_heard_on_the_single_target);
    RUN_TEST(test_dry_hop_train_loses_its_first_step);
    RUN_TEST(test_dry_gaps_of_15_ms_and_more_are_not_heard);
    RUN_TEST(test_room_tail_bridges_15_ms_but_not_30_ms);
    RUN_TEST(test_release_debounce_above_the_gap_bridges_the_dry_train);
    RUN_TEST(test_one_deep_null_is_survived_two_in_the_middle_are_not);
    RUN_TEST(test_shipped_hop_train_fits_the_scalar_detector);
    RUN_TEST(test_hop_gap_check_flags_what_the_detector_cannot_read);
    RUN_TEST(test_band_grid_is_shared_by_detector_and_hop_train);
    RUN_TEST(test_dc_blocker_scale_is_the_first_difference_gain_at_the_chirp);
    return UNITY_END();
}
