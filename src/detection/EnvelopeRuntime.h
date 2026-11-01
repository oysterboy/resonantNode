#pragma once

#include <stddef.h>
#include <stdint.h>

#include "detectors/scalar/ScalarTransientDetector.h"
#include "evaluation/OccurrenceVerdict.h"
#include "features/FeatureHistory.h"
#include "inspection/OccurrenceInspector.h"

namespace detection {

/*
EnvelopeRuntime (ECHO-001 / ECHO-002, step 20)

The echo modes' detection chain, myspec's order on the amplitude envelope alone (no band, no DetectionRuntime): per
block the envelope's Strength16 level into a FeatureHistory and the ScalarTransientDetector; an occurrence it finishes
is held until the plan's inspection window is in the history, the OccurrenceInspector runs the mode's plan (the echo
room's attack, Whispernet's modules) and makeVerdict judges it. The echo shell and the host echo replay run this one
class, so a WAV replayed on the PC is judged as the board judges it.

Owns: the history, the detector, the inspector and the plan, the occurrence held for its window, the verdicts not yet
taken. Does not: compute the envelope (EnvelopeFollower), decide (Behavior), or reconstruct detector truth.
*/
class EnvelopeRuntime {
public:
    struct Result {
        OccurrenceVerdict verdict;
        InspectedOccurrence inspected;  // the occurrence with the plan's facts (attack, ...)
    };

    void configure(const ScalarTransientConfig& detector, const InspectionPlan& plan);
    void reset();
    // After a gap in the input (the own replay): no occurrence carries over it.
    void restartAfterInputGap();

    // One block: its envelope level (Strength16), its time on the 64-bit time base, its time in ms.
    void onBlock(float level, uint64_t blockUs, uint32_t blockMs);

    // The next judged occurrence, oldest first. False when none is waiting.
    bool popResult(Result& out);

private:
    void judgePending(uint32_t nowMs);

    static constexpr size_t kResults = 2;  // taken every block; an InspectedOccurrence is large
    FeatureHistory _history;
    OccurrenceInspector _inspector;
    InspectionPlan _plan = {};
    ScalarTransientDetector _detector;
    Occurrence _pending = {};
    bool _havePending = false;
    Result _results[kResults];
    size_t _resultCount = 0;
};

}  // namespace detection
