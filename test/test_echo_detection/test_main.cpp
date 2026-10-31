/*
The echo room's inspection on the host (ECHO-001, step 20.2): the AttackRise module in the inspection plan reads the
envelope's history around an occurrence's onset (FeatureHistory::riseTimeMs), classes the rise, and makeVerdict
judges it like any other requirement. A clap passes, a swell does not, a sound with no quiet before it is not
measured. Runs in native-test-behavior.
*/
#include <unity.h>

#include "../../src/detection/features/FeatureHistory.h"
#include "../../src/detection/inspection/OccurrenceInspector.h"
#include "../../src/detection/inspection/VerdictProjection.h"

#include "../../src/detection/features/FeatureHistory.cpp"
#include "../../src/detection/inspection/OccurrenceInspector.cpp"
#include "../../src/detection/inspection/VerdictProjection.cpp"

using namespace detection;

namespace {

// The echo room's plan: one module, the attack, at least Medium (within 10 ms).
InspectionPlan echoPlan() {
    InspectionPlan plan = InspectionPlan{};
    plan.modules[0].kind = InspectionModuleKind::AttackRise;
    plan.modules[0].target = InspectionTarget::Attack;
    plan.modules[0].enabled = true;
    plan.modules[0].minimumStrength = StrengthClass::Medium;
    plan.count = 1;
    return plan;
}

Occurrence occurrence(unsigned long startMs) {
    Occurrence o = {};
    o.present = true;
    o.valid = true;
    o.occurrenceType = OccurrenceType::Scalar;
    o.startMs = startMs;
    o.durationMs = 40;
    o.releaseMs = startMs + 40;
    o.strength = 25000.0f;
    return o;
}

// The envelope as the echo shell records it: one value per DMA block, ~3 ms apart.
void record(FeatureHistory& h, unsigned long fromMs, unsigned long toMs, float (*level)(unsigned long)) {
    for (unsigned long t = fromMs; t <= toMs; t += 3) {
        h.record(FeatureStreamId::AmpEnvelope, t, level(t));
    }
}

FeatureHistory& history() {
    static FeatureHistory h;  // ~41 KB at the host's default size: not on the stack
    const FeatureStreamId streams[1] = {FeatureStreamId::AmpEnvelope};
    h.setActiveStreams(streams, 1);
    return h;
}

OccurrenceVerdict judge(FeatureHistory& h, const Occurrence& occ, InspectedOccurrence& inspected) {
    OccurrenceInspector inspector;
    inspector.configure(echoPlan());
    inspected = inspector.inspectWithHistory(occ, &h, occ.startMs + 100);
    OccurrenceVerdict v;
    makeVerdict(inspected, echoPlan(), v);
    return v;
}

}  // namespace

void setUp() {}
void tearDown() {}

// Quiet until 1000, loud from 1003: 10 % -> 90 % within one block.
void test_a_clap_rises_fast_and_passes() {
    FeatureHistory& h = history();
    record(h, 900, 1100, [](unsigned long t) { return t < 1003 ? 100.0f : 30000.0f; });
    InspectedOccurrence in;
    const OccurrenceVerdict v = judge(h, occurrence(1003), in);
    TEST_ASSERT_TRUE(in.occurrence.attack.applicable);
    TEST_ASSERT_TRUE(in.occurrence.attack.measured);
    TEST_ASSERT_EQUAL_UINT32(3, in.occurrence.attack.riseMs);
    TEST_ASSERT_EQUAL(StrengthClass::Strong, in.occurrence.attack.attackClass);
    TEST_ASSERT_TRUE(v.valid);
}

// A swell from 5000 to 30000 over 60 ms: 10 % -> 90 % of its peak takes ~50 ms, too slow.
void test_a_swell_rises_slowly_and_fails() {
    FeatureHistory& h = history();
    record(h, 900, 1100, [](unsigned long t) {
        return t < 1000 ? 100.0f : (t > 1060 ? 30000.0f : 100.0f + 29900.0f * (t - 1000) / 60.0f);
    });
    InspectedOccurrence in;
    const OccurrenceVerdict v = judge(h, occurrence(1018), in);
    TEST_ASSERT_TRUE(in.occurrence.attack.measured);
    TEST_ASSERT_TRUE(in.occurrence.attack.riseMs > 10);
    TEST_ASSERT_FALSE(v.valid);
    TEST_ASSERT_EQUAL(InspectionTarget::Attack, v.firstFailedRequirementTarget);
    TEST_ASSERT_EQUAL(VerdictRejectReason::SupportTooLow, v.rejectReason);
}

// Loud all through the window: no quiet bin before the crossing, the attack is not measured (missing support).
void test_no_quiet_before_the_onset_is_not_measured() {
    FeatureHistory& h = history();
    record(h, 900, 1100, [](unsigned long) { return 30000.0f; });
    InspectedOccurrence in;
    const OccurrenceVerdict v = judge(h, occurrence(1003), in);
    TEST_ASSERT_FALSE(in.occurrence.attack.measured);
    TEST_ASSERT_EQUAL(StrengthClass::Unknown, in.occurrence.attack.attackClass);
    TEST_ASSERT_FALSE(v.valid);
    TEST_ASSERT_EQUAL(VerdictRejectReason::MissingSupport, v.rejectReason);
}

// The plan's window ends postMs after the start: the runtime holds the occurrence until then.
void test_window_complete_after_the_onset() {
    OccurrenceInspector inspector;
    inspector.configure(echoPlan());
    TEST_ASSERT_EQUAL_UINT32(1003 + 60, inspector.windowCompleteAtMs(occurrence(1003)));
}

// The query itself, and a bin length above 1 ms rounding times down to the bin's start.
void test_rise_time_query() {
    FeatureHistory& h = history();
    record(h, 0, 60, [](unsigned long t) { return t < 30 ? 1000.0f : 20000.0f; });
    unsigned long rise = 0;
    TEST_ASSERT_TRUE(h.riseTimeMs(FeatureStreamId::AmpEnvelope, 0, 60, 2000.0f, 10000.0f, rise));
    TEST_ASSERT_EQUAL_UINT32(3, rise);
    TEST_ASSERT_FALSE(h.riseTimeMs(FeatureStreamId::AmpEnvelope, 40, 60, 2000.0f, 10000.0f, rise));  // no quiet in it
    TEST_ASSERT_EQUAL_UINT32(1, FeatureHistory::kBinDurationMs);  // the host builds the Node's size
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_a_clap_rises_fast_and_passes);
    RUN_TEST(test_a_swell_rises_slowly_and_fails);
    RUN_TEST(test_no_quiet_before_the_onset_is_not_measured);
    RUN_TEST(test_window_complete_after_the_onset);
    RUN_TEST(test_rise_time_query);
    return UNITY_END();
}
