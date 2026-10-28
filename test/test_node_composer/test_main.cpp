/*
NodeSignalComposer on the host (step 11i N5, docs/refactors/node-split.md):
the Node's half of an emission. Hop frequencies per rotation, the shipped
5-step train, the idle call with and without the hop, the grid read from a
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

// The shipped band 2950-3950 on the 250 Hz grid: five bins.
Grid shippedGrid() {
    Grid g;
    g.lowHz = 2950;
    g.spacingHz = 250;
    g.bins = 5;
    return g;
}

void test_hop_frequencies_per_rotation() {
    NodeSignalComposer c;
    uint32_t hz[8] = {};
    c.values().hopSteps = 5;
    for (uint32_t rot = 0; rot < 5; ++rot) {
        c.values().hopRotation = rot;
        TEST_ASSERT_EQUAL_UINT32(5, c.hopFrequencies(shippedGrid(), hz, 8));
        for (size_t k = 0; k < 5; ++k) {
            TEST_ASSERT_EQUAL_UINT32(2950 + ((k + rot) % 5) * 250, hz[k]);
        }
    }
    // A rotation past the bin count wraps like the modulo it is.
    c.values().hopRotation = 7;
    c.hopFrequencies(shippedGrid(), hz, 8);
    TEST_ASSERT_EQUAL_UINT32(3450, hz[0]);
    // Capacity caps the steps; no bins or no room gives none.
    c.values().hopSteps = 8;
    TEST_ASSERT_EQUAL_UINT32(3, c.hopFrequencies(shippedGrid(), hz, 3));
    TEST_ASSERT_EQUAL_UINT32(0, c.hopFrequencies(shippedGrid(), hz, 0));
    TEST_ASSERT_EQUAL_UINT32(0, c.hopFrequencies(Grid{}, hz, 8));
}

void test_five_step_train_from_a_heard_pattern() {
    NodeSignalComposer c;  // shipped values: on, 25 ms x 5, gap 10
    c.values().hopRotation = 2;
    const OutputRequest r = c.compose(shippedGrid(), ChirpPattern::Single, true);
    TEST_ASSERT_EQUAL(static_cast<int>(OutputRequest::Kind::HopTrain), static_cast<int>(r.kind));
    TEST_ASSERT_EQUAL(static_cast<int>(ChirpPattern::Single), static_cast<int>(r.pattern));
    TEST_ASSERT_EQUAL_UINT32(5, r.hopSteps);
    TEST_ASSERT_EQUAL_UINT32(25, r.pulseMs);
    TEST_ASSERT_EQUAL_UINT32(10, r.gapMs);
    const uint32_t expect[5] = {3450, 3700, 3950, 2950, 3200};
    TEST_ASSERT_EQUAL_UINT32_ARRAY(expect, r.hopHz, 5);
    TEST_ASSERT_EQUAL_UINT32(0, r.announceHz);
    TEST_ASSERT_EQUAL(static_cast<int>(OutputReject::None), static_cast<int>(SoundOutput::validate(r, OutputLimits())));
}

void test_plain_pattern_when_no_train() {
    NodeSignalComposer c;
    // Not the network signal: the pattern.
    OutputRequest r = c.compose(shippedGrid(), ChirpPattern::Triple, false);
    TEST_ASSERT_EQUAL(static_cast<int>(OutputRequest::Kind::Pattern), static_cast<int>(r.kind));
    // Hop off, or a grid without bins: the network signal falls back to the pattern.
    c.values().hopEnabled = false;
    r = c.compose(shippedGrid(), ChirpPattern::Single, true);
    TEST_ASSERT_EQUAL(static_cast<int>(OutputRequest::Kind::Pattern), static_cast<int>(r.kind));
    c.values().hopEnabled = true;
    r = c.compose(Grid{}, ChirpPattern::Single, true);
    TEST_ASSERT_EQUAL(static_cast<int>(OutputRequest::Kind::Pattern), static_cast<int>(r.kind));
    TEST_ASSERT_EQUAL_UINT32(0, r.hopSteps);
}

void test_idle_call_with_hop() {
    NodeSignalComposer c;
    c.values().hopRotation = 1;
    const OutputRequest r = c.compose(shippedGrid(), ChirpPattern::Idle, false);
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
    OutputRequest r = c.compose(shippedGrid(), ChirpPattern::Idle, false);
    TEST_ASSERT_EQUAL(static_cast<int>(OutputRequest::Kind::IdleCall), static_cast<int>(r.kind));
    TEST_ASSERT_EQUAL_UINT32(0, r.hopSteps);  // the call is the plain chirp
    TEST_ASSERT_EQUAL_UINT32(300, r.announceHz);
    // No announcement tone: the plain idle pattern.
    c.values().idleAnnounceHz = 0;
    r = c.compose(shippedGrid(), ChirpPattern::Idle, false);
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
    TEST_ASSERT_EQUAL_UINT32(5, g.bins);

    // Moved and narrowed: the train follows the applied band.
    band.setBand(2200, 2700);
    g = NodeSignalComposer::gridOf(band);
    TEST_ASSERT_EQUAL_UINT32(2200, g.lowHz);
    TEST_ASSERT_EQUAL_UINT32(3, g.bins);
    NodeSignalComposer c;
    c.values().hopRotation = 4;
    const OutputRequest r = c.compose(g, ChirpPattern::Single, true);
    const uint32_t expect[5] = {2450, 2700, 2200, 2450, 2700};
    TEST_ASSERT_EQUAL_UINT32_ARRAY(expect, r.hopHz, 5);

    // Wider than the bank: the grid is the clamped one, not the asked one.
    band.setBand(1000, 6000);
    g = NodeSignalComposer::gridOf(band);
    TEST_ASSERT_EQUAL_UINT32(FreqBandStream::kMaxBandBins, g.bins);
    TEST_ASSERT_EQUAL_UINT32(band.bandHighHz(), g.lowHz + (g.bins - 1) * g.spacingHz);

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
    OutputRequest r = c.compose(wide, ChirpPattern::Single, true);
    TEST_ASSERT_EQUAL_UINT32(8, r.hopSteps);
    TEST_ASSERT_EQUAL_UINT32(3750, SoundOutput::plannedSpanMs(r, runtime::kDefaultChirpDurationMs));
    TEST_ASSERT_EQUAL(static_cast<int>(OutputReject::None), static_cast<int>(SoundOutput::validate(r, OutputLimits())));

    c.values().idleAnnounceMs = 2000;
    r = c.compose(wide, ChirpPattern::Idle, false);
    TEST_ASSERT_EQUAL(static_cast<int>(OutputRequest::Kind::IdleCall), static_cast<int>(r.kind));
    TEST_ASSERT_EQUAL_UINT32(5950, SoundOutput::plannedSpanMs(r, runtime::kDefaultChirpDurationMs));
    TEST_ASSERT_EQUAL(static_cast<int>(OutputReject::None), static_cast<int>(SoundOutput::validate(r, OutputLimits())));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_hop_frequencies_per_rotation);
    RUN_TEST(test_five_step_train_from_a_heard_pattern);
    RUN_TEST(test_plain_pattern_when_no_train);
    RUN_TEST(test_idle_call_with_hop);
    RUN_TEST(test_idle_call_without_hop);
    RUN_TEST(test_bins_from_a_moved_band);
    RUN_TEST(test_longest_train_and_call_fit_the_span_limit);
    return UNITY_END();
}
