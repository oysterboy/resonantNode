/*
NodeSignalComposer on the host (docs/refactors/archive/node-split.md):
the Node's half of an emission. Hop frequencies per base bin (no wrap, the
8-bin band since 2026-10-30), the shipped 5-step train, the idle call with and without the hop, the grid read from a
real FreqBandStream after the band moved, and the span bound the output.*
ranges keep (8 x 250 + 7 x 250 fits OutputLimits). Runs in native-test-behavior.
*/
#include <unity.h>

#include "../../src/modes/resonant/ResonantNodeSignalComposer.cpp"
#include "../../src/detection/features/FreqBandStream.cpp"
#include "../../src/output/ChirpOutput.cpp"
#include "../../src/output/SoundOutput.cpp"

namespace {

using Grid = NodeSignalComposer::BandGrid;

// The shipped band 2950-4700 on the 250 Hz grid: eight bins.
Grid shippedGrid() {
    Grid g;
    g.lowHz = 2950;
    g.spacingHz = 250;
    g.bins = 8;
    return g;
}

void test_hop_frequencies_per_base_bin() {
    NodeSignalComposer c;
    uint32_t hz[8] = {};
    c.values().hopSteps = 5;
    TEST_ASSERT_EQUAL_UINT32(3, c.maxBaseBin(shippedGrid()));
    for (uint32_t base = 0; base <= 3; ++base) {
        TEST_ASSERT_EQUAL_UINT32(5, c.hopFrequencies(shippedGrid(), base, hz, 8));
        for (size_t k = 0; k < 5; ++k) {
            TEST_ASSERT_EQUAL_UINT32(2950 + (base + k) * 250, hz[k]);  // climbing, never wrapping
        }
    }
    // A base too high for the train is lowered until the train fits: base 7 plays as base 3 (top bin 4700).
    c.hopFrequencies(shippedGrid(), 7, hz, 8);
    TEST_ASSERT_EQUAL_UINT32(3700, hz[0]);
    TEST_ASSERT_EQUAL_UINT32(4700, hz[4]);
    // Capacity caps the steps, so does the grid (8 steps on 8 bins: base 0 only); no bins or no room gives none.
    c.values().hopSteps = 8;
    TEST_ASSERT_EQUAL_UINT32(0, c.maxBaseBin(shippedGrid()));
    TEST_ASSERT_EQUAL_UINT32(8, c.hopFrequencies(shippedGrid(), 0, hz, 8));
    TEST_ASSERT_EQUAL_UINT32(2950, hz[0]);
    TEST_ASSERT_EQUAL_UINT32(3, c.hopFrequencies(shippedGrid(), 0, hz, 3));
    TEST_ASSERT_EQUAL_UINT32(0, c.hopFrequencies(shippedGrid(), 0, hz, 0));
    TEST_ASSERT_EQUAL_UINT32(0, c.hopFrequencies(Grid{}, 0, hz, 8));
}

void test_five_step_train_from_a_heard_pattern() {
    NodeSignalComposer c;  // shipped values: on, 15 ms x 5, no gap (a glide)
    const OutputRequest r = c.compose(shippedGrid(), ChirpPattern::Single, true, 2);
    TEST_ASSERT_EQUAL(static_cast<int>(OutputRequest::Kind::HopTrain), static_cast<int>(r.kind));
    TEST_ASSERT_EQUAL(static_cast<int>(ChirpPattern::Single), static_cast<int>(r.pattern));
    TEST_ASSERT_EQUAL_UINT32(5, r.hopSteps);
    TEST_ASSERT_EQUAL_UINT32(15, r.pulseMs);
    TEST_ASSERT_EQUAL_UINT32(0, r.gapMs);
    const uint32_t expect[5] = {3450, 3700, 3950, 4200, 4450};
    TEST_ASSERT_EQUAL_UINT32_ARRAY(expect, r.hopHz, 5);
    TEST_ASSERT_EQUAL_UINT32(0, r.announceHz);
    TEST_ASSERT_EQUAL(static_cast<int>(OutputReject::None), static_cast<int>(SoundOutput::validate(r, OutputLimits())));
}

void test_plain_pattern_when_no_train() {
    NodeSignalComposer c;
    // Not the network signal: the pattern.
    OutputRequest r = c.compose(shippedGrid(), ChirpPattern::Triple, false, 0);
    TEST_ASSERT_EQUAL(static_cast<int>(OutputRequest::Kind::Pattern), static_cast<int>(r.kind));
    // Hop off, or a grid without bins: the network signal falls back to the pattern.
    c.values().hopEnabled = false;
    r = c.compose(shippedGrid(), ChirpPattern::Single, true, 0);
    TEST_ASSERT_EQUAL(static_cast<int>(OutputRequest::Kind::Pattern), static_cast<int>(r.kind));
    c.values().hopEnabled = true;
    r = c.compose(Grid{}, ChirpPattern::Single, true, 0);
    TEST_ASSERT_EQUAL(static_cast<int>(OutputRequest::Kind::Pattern), static_cast<int>(r.kind));
    TEST_ASSERT_EQUAL_UINT32(0, r.hopSteps);
}

void test_idle_call_with_hop() {
    NodeSignalComposer c;
    const OutputRequest r = c.compose(shippedGrid(), ChirpPattern::Idle, false, 1);
    TEST_ASSERT_EQUAL(static_cast<int>(OutputRequest::Kind::IdleCall), static_cast<int>(r.kind));
    TEST_ASSERT_EQUAL_UINT32(300, r.announceHz);
    TEST_ASSERT_EQUAL_UINT32(500, r.announceMs);
    TEST_ASSERT_EQUAL_UINT32(NodeSignalComposer::kIdleAnnounceGapMs, r.announceGapMs);
    TEST_ASSERT_EQUAL_UINT32(5, r.hopSteps);
    TEST_ASSERT_EQUAL_UINT32(3200, r.hopHz[0]);
    TEST_ASSERT_EQUAL_UINT32(15, r.pulseMs);
}

void test_idle_call_without_hop() {
    NodeSignalComposer c;
    c.values().hopEnabled = false;
    OutputRequest r = c.compose(shippedGrid(), ChirpPattern::Idle, false, 0);
    TEST_ASSERT_EQUAL(static_cast<int>(OutputRequest::Kind::IdleCall), static_cast<int>(r.kind));
    TEST_ASSERT_EQUAL_UINT32(0, r.hopSteps);  // the call is the plain chirp
    TEST_ASSERT_EQUAL_UINT32(300, r.announceHz);
    // Below what the output plays (OutputLimits.minHz): raised to it, not rejected (review 12.0 M8).
    c.values().idleAnnounceHz = 50;
    r = c.compose(shippedGrid(), ChirpPattern::Idle, false, 0);
    TEST_ASSERT_EQUAL_UINT32(OutputLimits().minHz, r.announceHz);
    TEST_ASSERT_EQUAL(static_cast<int>(OutputReject::None), static_cast<int>(SoundOutput::validate(r, OutputLimits())));
    // No announcement tone: the plain idle pattern.
    c.values().idleAnnounceHz = 0;
    r = c.compose(shippedGrid(), ChirpPattern::Idle, false, 0);
    TEST_ASSERT_EQUAL(static_cast<int>(OutputRequest::Kind::Pattern), static_cast<int>(r.kind));
    TEST_ASSERT_EQUAL(static_cast<int>(ChirpPattern::Idle), static_cast<int>(r.pattern));
}

void test_bins_from_a_moved_band() {
    FreqBandStream band;
    band.setSampleRateHz(runtime::kDefaultAudioI2SSampleRateHz);
    band.setBand(runtime::kDefaultNetworkBandLowHz, runtime::kDefaultNetworkBandHighHz);
    Grid g = NodeSignalComposer::gridOf(band);
    TEST_ASSERT_EQUAL_UINT32(2950, g.lowHz);
    TEST_ASSERT_EQUAL_UINT32(runtime::kBandBinSpacingHz, g.spacingHz);
    TEST_ASSERT_EQUAL_UINT32(8, g.bins);

    // Moved and narrowed below the train's steps: the train follows the applied band and climbs it once (3 steps
    // on 3 bins, from bin 0 whatever the base), never repeating a bin.
    band.setBand(2200, 2700);
    g = NodeSignalComposer::gridOf(band);
    TEST_ASSERT_EQUAL_UINT32(2200, g.lowHz);
    TEST_ASSERT_EQUAL_UINT32(3, g.bins);
    NodeSignalComposer c;
    const OutputRequest r = c.compose(g, ChirpPattern::Single, true, 4);
    TEST_ASSERT_EQUAL_UINT32(3, r.hopSteps);
    const uint32_t expect[3] = {2200, 2450, 2700};
    TEST_ASSERT_EQUAL_UINT32_ARRAY(expect, r.hopHz, 3);

    // Wider than the bank: the grid is the clamped one, not the asked one.
    band.setBand(1000, 6000);
    g = NodeSignalComposer::gridOf(band);
    TEST_ASSERT_EQUAL_UINT32(FreqBandStream::kMaxBandBins, g.bins);
    TEST_ASSERT_EQUAL_UINT32(band.bandHighHz(), g.lowHz + (g.bins - 1) * g.spacingHz);
    // What NetworkBand compares with when it re-applies the band (review 12.0 M7): the clamped edge, so an
    // unchanged wide band matches and the bank is not rebuilt every loop.
    TEST_ASSERT_EQUAL_UINT32(band.bandHighHz(), band.clampedBandHighHz(1000, 6000));
    TEST_ASSERT_EQUAL_UINT32(2700, band.clampedBandHighHz(2200, 2700));

    // Single-target mode: no bins, no train.
    band.setBand(0, 0);
    TEST_ASSERT_EQUAL_UINT32(0, NodeSignalComposer::gridOf(band).bins);
}

void test_longest_train_and_call_fit_the_span_limit() {
    // The output.* upper bounds (ResonantNodeParams.cpp): 8 steps of 250 + 7 gaps of 250 = 3750 ms, and the idle
    // call adds announce 2000 + its 200 ms gap = 5950 ms <= OutputLimits.maxSpanMs (6000).
    Grid wide;
    wide.lowHz = 2000;
    wide.spacingHz = 250;
    wide.bins = 8;
    NodeSignalComposer c;
    c.values().hopSteps = 8;
    c.values().hopPulseMs = 250;
    c.values().hopGapMs = 250;
    OutputRequest r = c.compose(wide, ChirpPattern::Single, true, 0);
    TEST_ASSERT_EQUAL_UINT32(8, r.hopSteps);
    TEST_ASSERT_EQUAL_UINT32(3750, SoundOutput::plannedSpanMs(r, runtime::kDefaultChirpDurationMs));
    TEST_ASSERT_EQUAL(static_cast<int>(OutputReject::None), static_cast<int>(SoundOutput::validate(r, OutputLimits())));

    c.values().idleAnnounceMs = 2000;
    r = c.compose(wide, ChirpPattern::Idle, false, 0);
    TEST_ASSERT_EQUAL(static_cast<int>(OutputRequest::Kind::IdleCall), static_cast<int>(r.kind));
    TEST_ASSERT_EQUAL_UINT32(5950, SoundOutput::plannedSpanMs(r, runtime::kDefaultChirpDurationMs));
    TEST_ASSERT_EQUAL(static_cast<int>(OutputReject::None), static_cast<int>(SoundOutput::validate(r, OutputLimits())));
}

void test_trill_before_the_climb() {
    // The cricket-like call: 4 pulses on the base bin, then the 5-step climb, 20 ms pulses, 15 ms gaps (300 ms).
    NodeSignalComposer c;
    TEST_ASSERT_EQUAL_UINT32(0, c.values().hopTrillPulses);  // shipped: no trill, the plain train
    c.values().hopTrillPulses = 4;
    c.values().hopPulseMs = 20;
    c.values().hopGapMs = 15;
    OutputRequest r = c.compose(shippedGrid(), ChirpPattern::Single, true, 2);
    TEST_ASSERT_EQUAL_UINT32(9, r.hopSteps);
    TEST_ASSERT_EQUAL_UINT32(300, detection::hopTrainMs(c.trainShape(5)));
    const uint32_t expect[9] = {3450, 3450, 3450, 3450, 3450, 3700, 3950, 4200, 4450};
    TEST_ASSERT_EQUAL_UINT32_ARRAY(expect, r.hopHz, 9);
    TEST_ASSERT_EQUAL_UINT32(300, SoundOutput::plannedSpanMs(r, runtime::kDefaultChirpDurationMs));
    TEST_ASSERT_EQUAL(static_cast<int>(OutputReject::None), static_cast<int>(SoundOutput::validate(r, OutputLimits())));
    // The idle call carries the same trill after its announcement.
    r = c.compose(shippedGrid(), ChirpPattern::Idle, false, 0);
    TEST_ASSERT_EQUAL(static_cast<int>(OutputRequest::Kind::IdleCall), static_cast<int>(r.kind));
    TEST_ASSERT_EQUAL_UINT32(9, r.hopSteps);
    TEST_ASSERT_EQUAL_UINT32(2950, r.hopHz[3]);
    TEST_ASSERT_EQUAL_UINT32(3200, r.hopHz[5]);
}

void test_gapped_trill_before_the_glide() {
    // 10 trill pulses of 15 ms with 15 ms gaps, then the shipped glide (5 x 15 ms, no gap): 375 ms. Only the trill
    // has gaps; the detector reads it as one sound once the release debounce outlasts the trill gap.
    NodeSignalComposer c;
    c.values().hopTrillPulses = 10;
    c.values().hopTrillGapMs = 15;
    const OutputRequest r = c.compose(shippedGrid(), ChirpPattern::Single, true, 0);
    TEST_ASSERT_EQUAL_UINT32(15, r.hopSteps);
    TEST_ASSERT_EQUAL_UINT32(10, r.trillPulses);
    TEST_ASSERT_EQUAL_UINT32(15, r.trillGapMs);
    TEST_ASSERT_EQUAL_UINT32(0, r.gapMs);
    TEST_ASSERT_EQUAL_UINT32(2950, r.hopHz[9]);
    TEST_ASSERT_EQUAL_UINT32(2950, r.hopHz[10]);
    TEST_ASSERT_EQUAL_UINT32(3950, r.hopHz[14]);
    TEST_ASSERT_EQUAL_UINT32(375, SoundOutput::plannedSpanMs(r, runtime::kDefaultChirpDurationMs));
    TEST_ASSERT_EQUAL(static_cast<int>(OutputReject::None), static_cast<int>(SoundOutput::validate(r, OutputLimits())));

    const detection::HopTrainShape shape = c.trainShape(5);
    TEST_ASSERT_EQUAL_UINT32(375, detection::hopTrainMs(shape));
    detection::ScalarTransientConfig sc;
    sc.minTransientDurationMs = 70;
    sc.maxTransientDurationMs = 450;
    sc.requireCarrierQuality = true;
    sc.minCoverageAboveReleaseMs = 60;
    sc.minLongestIslandMs = 20;
    sc.maxGapMs = 20;
    sc.releaseDebounceMs = 10;  // shipped: the 15 ms trill gap closes the candidate (soft)
    detection::HopGapCheck chk = detection::checkHopGap(shape, sc);
    TEST_ASSERT_TRUE(chk.hardOk());
    TEST_ASSERT_FALSE(chk.gapBelowDebounce);
    sc.releaseDebounceMs = 20;  // the cricket preset
    chk = detection::checkHopGap(shape, sc);
    TEST_ASSERT_TRUE(chk.hardOk());
    TEST_ASSERT_TRUE(chk.softOk());  // 375 + 50 tail <= 450
    sc.maxGapMs = 10;                // a trill gap past max gap can never be one sound
    TEST_ASSERT_FALSE(detection::checkHopGap(shape, sc).hardOk());
}

void test_broadband_settings_ride_on_the_train() {
    // 24.1: off by default, so the shipped request carries no burst; set, the request carries the values.
    NodeSignalComposer c;
    TEST_ASSERT_EQUAL_UINT32(0, c.values().broadbandLevel);
    OutputRequest r = c.compose(shippedGrid(), ChirpPattern::Single, true, 2);
    TEST_ASSERT_EQUAL_UINT32(0, r.broadbandPct);
    TEST_ASSERT_FALSE(r.broadbandTrillOnly);
    c.values().broadbandLevel = 40;
    c.values().broadbandScope = 1;
    c.values().broadbandLowHz = 6800;
    r = c.compose(shippedGrid(), ChirpPattern::Single, true, 2);
    TEST_ASSERT_EQUAL_UINT32(40, r.broadbandPct);
    TEST_ASSERT_TRUE(r.broadbandTrillOnly);
    TEST_ASSERT_EQUAL_UINT32(6800, r.broadbandLowHz);
    // The idle call's train carries it too; the announcement tone does not need it.
    r = c.compose(shippedGrid(), ChirpPattern::Idle, false, 0);
    TEST_ASSERT_EQUAL_UINT32(40, r.broadbandPct);
}

void test_trill_shape_rides_on_the_train() {
    // 24.4.1: off by default, so the shipped request is the plain trill; set, the request carries the values.
    NodeSignalComposer c;
    OutputRequest r = c.compose(shippedGrid(), ChirpPattern::Single, true, 2);
    TEST_ASSERT_EQUAL_UINT32(0, r.trillFadeMs);
    TEST_ASSERT_EQUAL_UINT32(0, r.trillDroopHz);
    TEST_ASSERT_EQUAL_UINT32(0, r.trillSwellPulses);
    c.values().trillFadeMs = 4;
    c.values().trillDroopHz = 150;
    c.values().trillSwellPulses = 3;
    r = c.compose(shippedGrid(), ChirpPattern::Idle, false, 0);
    TEST_ASSERT_EQUAL_UINT32(4, r.trillFadeMs);
    TEST_ASSERT_EQUAL_UINT32(150, r.trillDroopHz);
    TEST_ASSERT_EQUAL_UINT32(3, r.trillSwellPulses);
    TEST_ASSERT_EQUAL_UINT32(0, r.trillPulseEndMs);
    TEST_ASSERT_EQUAL_UINT32(0, r.trillPulseJitterMs);
    c.values().trillPulseEndMs = 25;
    c.values().trillPulseJitterMs = 4;
    r = c.compose(shippedGrid(), ChirpPattern::Idle, false, 0);
    TEST_ASSERT_EQUAL_UINT32(25, r.trillPulseEndMs);
    TEST_ASSERT_EQUAL_UINT32(4, r.trillPulseJitterMs);
    TEST_ASSERT_EQUAL_UINT32(0, r.trillShapeScope);
    c.values().trillShapeScope = 2;
    r = c.compose(shippedGrid(), ChirpPattern::Idle, false, 0);
    TEST_ASSERT_EQUAL_UINT32(2, r.trillShapeScope);
}

void test_trill_is_trimmed_to_the_span_limit() {
    // Steps, pulse and gap at their tops with 12 trill pulses (15 ms trill gaps) would be 6930 ms. The trill is
    // shortened, the call stays valid instead of falling back to the plain chirp.
    Grid wide;
    wide.lowHz = 2000;
    wide.spacingHz = 250;
    wide.bins = 8;
    NodeSignalComposer c;
    c.values().hopSteps = 8;
    c.values().hopPulseMs = 250;
    c.values().hopGapMs = 250;
    c.values().hopTrillPulses = 12;
    OutputRequest r = c.compose(wide, ChirpPattern::Single, true, 0);
    TEST_ASSERT_EQUAL_UINT32(16, r.hopSteps);  // 16 x 250 + 8 trill gaps x 15 + 7 x 250 = 5870 <= 6000
    TEST_ASSERT_EQUAL(static_cast<int>(OutputReject::None), static_cast<int>(SoundOutput::validate(r, OutputLimits())));
    c.values().idleAnnounceMs = 2000;
    r = c.compose(wide, ChirpPattern::Idle, false, 0);
    TEST_ASSERT_EQUAL_UINT32(8, r.hopSteps);  // 2200 of announcement leave 3800: the climb alone
    TEST_ASSERT_EQUAL(static_cast<int>(OutputReject::None), static_cast<int>(SoundOutput::validate(r, OutputLimits())));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_hop_frequencies_per_base_bin);
    RUN_TEST(test_five_step_train_from_a_heard_pattern);
    RUN_TEST(test_plain_pattern_when_no_train);
    RUN_TEST(test_idle_call_with_hop);
    RUN_TEST(test_idle_call_without_hop);
    RUN_TEST(test_bins_from_a_moved_band);
    RUN_TEST(test_longest_train_and_call_fit_the_span_limit);
    RUN_TEST(test_trill_before_the_climb);
    RUN_TEST(test_gapped_trill_before_the_glide);
    RUN_TEST(test_broadband_settings_ride_on_the_train);
    RUN_TEST(test_trill_shape_rides_on_the_train);
    RUN_TEST(test_trill_is_trimmed_to_the_span_limit);
    return UNITY_END();
}
