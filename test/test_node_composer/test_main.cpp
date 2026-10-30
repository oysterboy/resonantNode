/*
NodeSignalComposer on the host (step 11i N5, docs/refactors/node-split.md):
the Node's half of an emission. Hop frequencies per base bin (no wrap, the
8-bin band since 2026-10-30), the shipped 5-step train, the idle call with and without the hop, the grid read from a
real FreqBandStream after the band moved, and the span bound the output.*
ranges keep (8 x 250 + 7 x 250 fits OutputLimits, step 11g B2 / review
D1.2). Runs in native-test-behavior.
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
    NodeSignalComposer c;  // shipped values: on, 25 ms x 5, gap 10
    const OutputRequest r = c.compose(shippedGrid(), ChirpPattern::Single, true, 2);
    TEST_ASSERT_EQUAL(static_cast<int>(OutputRequest::Kind::HopTrain), static_cast<int>(r.kind));
    TEST_ASSERT_EQUAL(static_cast<int>(ChirpPattern::Single), static_cast<int>(r.pattern));
    TEST_ASSERT_EQUAL_UINT32(5, r.hopSteps);
    TEST_ASSERT_EQUAL_UINT32(25, r.pulseMs);
    TEST_ASSERT_EQUAL_UINT32(10, r.gapMs);
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
    TEST_ASSERT_EQUAL_UINT32(25, r.pulseMs);
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
    return UNITY_END();
}
