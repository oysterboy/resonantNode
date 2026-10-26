/*
The ShapeInspector on the host (DET-015, step 11b; decisions/2026-10-26-det015-bin-track-inspector.md): the
BinTrack plus the BinShape module over synthetic bin sequences, and makeVerdict's shape requirement. Pins the
rule: a steady tone is one bin (None), a hop train visits the band's bins rising (Strong), a train cut to three
steps is still Medium (the height-offset case, E002), a rotation that wraps counts as rising, and single-target
mode (no bins) is "not applicable" and passes, and all of it holds across the 32-bit ms wrap (critique D1.1).
Runs in native-test-behavior.
*/
#include <unity.h>

#include "../../src/detection/features/BinTrack.h"
#include "../../src/detection/inspection/OccurrenceInspector.h"
#include "../../src/detection/inspection/VerdictProjection.h"

#include "../../src/detection/inspection/OccurrenceInspector.cpp"
#include "../../src/detection/inspection/VerdictProjection.cpp"
#include "../../src/detection/features/FeatureHistory.cpp"

using namespace detection;

namespace {

// Fill the track: each step holds one bin for stepMs, then gapMs of nothing, from startMs.
void playSteps(BinTrack& track, uint32_t startMs, const uint32_t* bins, size_t n, unsigned long stepMs,
               unsigned long gapMs, uint16_t level = 9000) {
    uint32_t t = startMs;  // 32 bits like the board's clock, so a test can cross the wrap
    for (size_t i = 0; i < n; ++i) {
        for (unsigned long k = 0; k < stepMs; ++k) {
            track.note(t++, bins[i], level);
        }
        for (unsigned long k = 0; k < gapMs; ++k) {
            track.note(t++, 0, 0);
        }
    }
    for (unsigned long k = 0; k < 20; ++k) {
        track.note(t++, 0, 0);
    }
}

InspectionPlan shapePlan(StrengthClass minimum = StrengthClass::Medium) {
    InspectionPlan plan = InspectionPlan{};
    plan.modules[0].kind = InspectionModuleKind::BinShape;
    plan.modules[0].target = InspectionTarget::Shape;
    plan.modules[0].enabled = true;
    plan.modules[0].minimumStrength = minimum;
    plan.count = 1;
    return plan;
}

Occurrence occurrence(unsigned long startMs, unsigned long durationMs) {
    Occurrence o = {};
    o.present = true;
    o.valid = true;
    o.occurrenceType = OccurrenceType::Scalar;
    o.startMs = startMs;
    o.durationMs = durationMs;
    o.releaseMs = startMs + durationMs;
    o.strength = 9000.0f;
    return o;
}

InspectedOccurrence inspect(const BinTrack& track, const InspectionPlan& plan, const Occurrence& occ) {
    OccurrenceInspector inspector;
    inspector.configure(plan);
    inspector.setBinTrack(&track);
    inspector.setShapeMinLevel(1000.0f);  // the runtime sets the detector's release threshold here
    return inspector.inspectWithHistory(occ, nullptr, occ.startMs + occ.durationMs + 50);
}

const uint32_t kHop[5] = {2950, 3200, 3450, 3700, 3950};

}  // namespace

void setUp() {}
void tearDown() {}

void test_steady_tone_is_one_bin_and_fails_the_requirement() {
    BinTrack track;
    const uint32_t tone[1] = {3200};
    playSteps(track, 100, tone, 1, 120, 0);
    const InspectedOccurrence in = inspect(track, shapePlan(), occurrence(100, 120));
    TEST_ASSERT_TRUE(in.occurrence.shape.applicable);
    TEST_ASSERT_EQUAL_UINT8(1, in.occurrence.shape.distinctBins);
    TEST_ASSERT_EQUAL(StrengthClass::None, in.occurrence.shape.shapeClass);
    OccurrenceVerdict v;
    TEST_ASSERT_TRUE(makeVerdict(in, shapePlan(), v));
    TEST_ASSERT_FALSE(v.accepted);
    TEST_ASSERT_EQUAL(InspectionTarget::Shape, v.firstFailedRequirementTarget);
    TEST_ASSERT_EQUAL(VerdictRejectReason::SupportTooLow, v.rejectReason);
}

void test_hop_train_is_strong_and_passes() {
    BinTrack track;
    playSteps(track, 100, kHop, 5, 25, 10);
    const InspectedOccurrence in = inspect(track, shapePlan(), occurrence(100, 165));
    TEST_ASSERT_EQUAL_UINT8(5, in.occurrence.shape.distinctBins);
    TEST_ASSERT_EQUAL_UINT8(4, in.occurrence.shape.risingSteps);
    TEST_ASSERT_EQUAL_UINT16(2950, in.occurrence.shape.firstBinHz);
    TEST_ASSERT_EQUAL_UINT16(3950, in.occurrence.shape.lastBinHz);
    TEST_ASSERT_EQUAL(StrengthClass::Strong, in.occurrence.shape.shapeClass);
    OccurrenceVerdict v;
    TEST_ASSERT_TRUE(makeVerdict(in, shapePlan(), v));
    TEST_ASSERT_TRUE(v.accepted);
}

void test_train_cut_to_three_steps_is_medium() {
    BinTrack track;
    playSteps(track, 100, kHop, 5, 25, 10);
    // the detector released after step 3 (the height-offset case): the occurrence covers 3 x 25 + 2 x 10 ms
    const InspectedOccurrence in = inspect(track, shapePlan(), occurrence(100, 95));
    TEST_ASSERT_EQUAL_UINT8(3, in.occurrence.shape.distinctBins);
    TEST_ASSERT_EQUAL_UINT8(2, in.occurrence.shape.risingSteps);
    TEST_ASSERT_EQUAL(StrengthClass::Medium, in.occurrence.shape.shapeClass);
    // with a margin after the release, the inspector sees the whole train
    InspectionPlan withTail = shapePlan();
    withTail.modules[0].shape.tailMs = 70;
    const InspectedOccurrence full = inspect(track, withTail, occurrence(100, 95));
    TEST_ASSERT_EQUAL_UINT8(5, full.occurrence.shape.distinctBins);
    TEST_ASSERT_EQUAL(StrengthClass::Strong, full.occurrence.shape.shapeClass);
}

void test_rotation_that_wraps_counts_as_rising() {
    BinTrack track;
    const uint32_t rotated[5] = {3450, 3700, 3950, 2950, 3200};  // rotation 2
    playSteps(track, 100, rotated, 5, 25, 10);
    const InspectedOccurrence in = inspect(track, shapePlan(), occurrence(100, 165));
    TEST_ASSERT_EQUAL_UINT8(4, in.occurrence.shape.risingSteps);
    TEST_ASSERT_EQUAL_UINT8(0, in.occurrence.shape.otherSteps);
    TEST_ASSERT_EQUAL(StrengthClass::Strong, in.occurrence.shape.shapeClass);
}

void test_random_jumps_are_not_rising() {
    BinTrack track;
    const uint32_t jumps[5] = {3700, 2950, 3450, 3200, 3950};
    playSteps(track, 100, jumps, 5, 25, 10);
    const InspectedOccurrence in = inspect(track, shapePlan(), occurrence(100, 165));
    TEST_ASSERT_EQUAL_UINT8(5, in.occurrence.shape.distinctBins);
    TEST_ASSERT_TRUE(in.occurrence.shape.risingSteps < 2);
    TEST_ASSERT_EQUAL(StrengthClass::Weak, in.occurrence.shape.shapeClass);
}

void test_flicker_shorter_than_a_run_is_folded_away() {
    BinTrack track;
    // a steady 3200 tone with 2 ms of the neighbour bin in the middle
    unsigned long t = 100;
    for (int k = 0; k < 60; ++k) track.note(t++, 3200, 9000);
    for (int k = 0; k < 2; ++k) track.note(t++, 3450, 9500);
    for (int k = 0; k < 60; ++k) track.note(t++, 3200, 9000);
    for (int k = 0; k < 20; ++k) track.note(t++, 0, 0);
    const InspectedOccurrence in = inspect(track, shapePlan(), occurrence(100, 122));
    TEST_ASSERT_EQUAL_UINT8(1, in.occurrence.shape.distinctBins);
    TEST_ASSERT_EQUAL_UINT8(1, in.occurrence.shape.runs);
}

void test_single_target_mode_is_not_applicable_and_passes() {
    BinTrack track;
    for (unsigned long t = 100; t < 300; ++t) {
        track.note(t, 0, 9000);  // no band bins: FreqBandStream reports 0 outside band mode
    }
    const InspectedOccurrence in = inspect(track, shapePlan(), occurrence(100, 120));
    TEST_ASSERT_FALSE(in.occurrence.shape.applicable);
    TEST_ASSERT_EQUAL(StrengthClass::Unknown, in.occurrence.shape.shapeClass);
    OccurrenceVerdict v;
    TEST_ASSERT_TRUE(makeVerdict(in, shapePlan(), v));
    TEST_ASSERT_TRUE(v.accepted);
}

void test_track_forgets_what_left_the_ring() {
    BinTrack track;
    track.note(10, 3200, 9000);
    uint32_t hz = 0;
    uint16_t level = 0;
    TEST_ASSERT_TRUE(track.at(10, hz, level));
    TEST_ASSERT_EQUAL_UINT32(3200, hz);
    track.note(10 + BinTrack::kMs, 0, 0);
    TEST_ASSERT_FALSE(track.at(10, hz, level));  // out of the ring
    TEST_ASSERT_FALSE(track.at(11, hz, level));  // cleared on the way
}

InspectionPlan shippedShapePlan() {
    InspectionPlan plan = shapePlan();
    plan.modules[0].shape.preMs = 120;
    plan.modules[0].shape.maxRisingStepBins = 2;
    return plan;
}

// A deep null at 3450 splits the train at the detector: the occurrence is only 3700, 3950 (the 5h notch case).
// Looking back 120 ms finds 2950, 3200 in the track, and the step over the missing bin counts as rising.
void test_train_split_by_a_middle_null_passes_with_the_look_back() {
    BinTrack track;
    const uint32_t nulled[4] = {2950, 3200, 3700, 3950};
    unsigned long t = 100;
    for (int s = 0; s < 2; ++s) {  // steps 1-2, then the null step's 35 ms of nothing
        for (int k = 0; k < 25; ++k) track.note(t++, nulled[s], 9000);
        for (int k = 0; k < 10; ++k) track.note(t++, 0, 0);
    }
    for (int k = 0; k < 35; ++k) track.note(t++, 0, 0);
    const unsigned long occStart = t;
    for (int s = 2; s < 4; ++s) {
        for (int k = 0; k < 25; ++k) track.note(t++, nulled[s], 9000);
        for (int k = 0; k < 10; ++k) track.note(t++, 0, 0);
    }
    for (int k = 0; k < 20; ++k) track.note(t++, 0, 0);
    const Occurrence occ = occurrence(occStart, 60);
    TEST_ASSERT_EQUAL(StrengthClass::Weak, inspect(track, shapePlan(), occ).occurrence.shape.shapeClass);
    const InspectedOccurrence in = inspect(track, shippedShapePlan(), occ);
    TEST_ASSERT_EQUAL_UINT8(4, in.occurrence.shape.distinctBins);
    TEST_ASSERT_EQUAL_UINT8(3, in.occurrence.shape.risingSteps);
    TEST_ASSERT_TRUE(in.occurrence.shape.shapeClass >= StrengthClass::Medium);
}

// The shipped settings still keep random jumps and a steady tone out.
void test_shipped_settings_still_reject_jumps_and_steady_tones() {
    BinTrack track;
    const uint32_t jumps[5] = {3700, 2950, 3450, 3200, 3950};
    playSteps(track, 300, jumps, 5, 25, 10);
    TEST_ASSERT_TRUE(inspect(track, shippedShapePlan(), occurrence(300, 165)).occurrence.shape.shapeClass < StrengthClass::Medium);
    BinTrack steady;
    const uint32_t tone[1] = {3200};
    playSteps(steady, 300, tone, 1, 130, 0);
    TEST_ASSERT_EQUAL(StrengthClass::None, inspect(steady, shippedShapePlan(), occurrence(300, 130)).occurrence.shape.shapeClass);
}

// Critique D1.1 (reproduced on the board 2026-10-26: after the ms wrap the track froze, the shape was "not
// applicable" and plain chirps passed). A train that crosses 0xFFFFFFFF is still Strong, an occurrence just after
// the wrap still finds its first steps before it with the look-back, and a steady tone after the wrap still fails.
void test_shape_holds_across_the_ms_wrap() {
    const uint32_t nearWrap = 0xFFFFFFFFu - 60u;
    BinTrack track;
    track.note(nearWrap - 400u, 0, 0);  // the track has been running before the train
    playSteps(track, nearWrap, kHop, 5, 25, 10);
    const InspectedOccurrence crossing = inspect(track, shippedShapePlan(), occurrence(nearWrap, 165));
    TEST_ASSERT_EQUAL_UINT8(5, crossing.occurrence.shape.distinctBins);
    TEST_ASSERT_EQUAL(StrengthClass::Strong, crossing.occurrence.shape.shapeClass);
    // The detector split the train: the occurrence is steps 3-5, starting after the wrap.
    const InspectedOccurrence split = inspect(track, shippedShapePlan(), occurrence(nearWrap + 70u, 95));
    TEST_ASSERT_EQUAL_UINT8(5, split.occurrence.shape.distinctBins);
    TEST_ASSERT_TRUE(split.occurrence.shape.shapeClass >= StrengthClass::Medium);
    // Well after the wrap the track still moves: a steady tone is one bin, not "not applicable".
    const uint32_t tone[1] = {3450};
    playSteps(track, 2000, tone, 1, 130, 0);
    const InspectedOccurrence steady = inspect(track, shippedShapePlan(), occurrence(2000, 130));
    TEST_ASSERT_TRUE(steady.occurrence.shape.applicable);
    TEST_ASSERT_EQUAL(StrengthClass::None, steady.occurrence.shape.shapeClass);
    OccurrenceVerdict v;
    TEST_ASSERT_TRUE(makeVerdict(steady, shippedShapePlan(), v));
    TEST_ASSERT_FALSE(v.accepted);
}

// Review D1.5: the level a bin must reach is the detector's release threshold, handed to the inspector live
// (DetectionRuntime::setScalarTransientConfig), not a copy in the plan. Above the train's level nothing counts.
void test_shape_level_follows_the_release_threshold() {
    BinTrack track;
    playSteps(track, 100, kHop, 5, 25, 10, 9000);
    OccurrenceInspector inspector;
    inspector.configure(shapePlan());
    inspector.setBinTrack(&track);
    inspector.setShapeMinLevel(8000.0f);
    TEST_ASSERT_EQUAL(StrengthClass::Strong, inspector.inspectWithHistory(occurrence(100, 165), nullptr, 400).occurrence.shape.shapeClass);
    inspector.setShapeMinLevel(9500.0f);
    TEST_ASSERT_FALSE(inspector.inspectWithHistory(occurrence(100, 165), nullptr, 400).occurrence.shape.applicable);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_steady_tone_is_one_bin_and_fails_the_requirement);
    RUN_TEST(test_hop_train_is_strong_and_passes);
    RUN_TEST(test_train_cut_to_three_steps_is_medium);
    RUN_TEST(test_rotation_that_wraps_counts_as_rising);
    RUN_TEST(test_random_jumps_are_not_rising);
    RUN_TEST(test_flicker_shorter_than_a_run_is_folded_away);
    RUN_TEST(test_single_target_mode_is_not_applicable_and_passes);
    RUN_TEST(test_track_forgets_what_left_the_ring);
    RUN_TEST(test_train_split_by_a_middle_null_passes_with_the_look_back);
    RUN_TEST(test_shipped_settings_still_reject_jumps_and_steady_tones);
    RUN_TEST(test_shape_holds_across_the_ms_wrap);
    RUN_TEST(test_shape_level_follows_the_release_threshold);
    return UNITY_END();
}
