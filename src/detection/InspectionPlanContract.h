#pragma once

#include "DetectionProfile.h"
#include "features/FeatureHistory.h"
#include "features/BinTrack.h"

/*
InspectionPlanContract (step 8b item 2; audit 3.1, 3.7; #32 P4)

The scalar detector's durations (detection.scalar_*) and the inspection plan's
windows are set independently, and so is the FeatureHistory depth. This is
the check between them, used by the Node at boot and after a PARAM SET (RB
plan_check), and by the host tests on the shipped profile.

Hard (inspection reads a window the history no longer holds):
  - history: max duration + release debounce + window pre + window post fits
    the history span (FEATURE_HISTORY_BINS x 1 ms). Conservative: it holds for
    any anchor, also if inspection waits for the window to complete.
  - shape: shape pre + max duration + release debounce + shape tail + 1 fits
    the bin track (BinTrack::kMs).
Soft (some occurrences are judged on a window that has not arrived):
  - complete at release: an occurrence of the minimum duration is inspected at
    release + debounce; a Start or Peak anchored window ends at most
    windowPost after its start, so min duration + debounce >= windowPost. A
    Release anchored window needs windowPost <= debounce. Until 2026-10-23: 85 + 10 <
    100, so occurrences of 85-89 ms are rejected as MissingSupport whatever the
    signal (audit 3.1). Since DET-017 (2026-10-23) the shipped values pass
    (92 + 10 >= 100), and the runtime also waits for an incomplete window.
  - shape level (review 12.0 L4): the shape rule's level is the scalar release
    threshold (DetectionRuntime), compared with the bin track's
    FrequencyScore16 levels (0..32767). It means something only when the
    detector watches FrequencyTarget and the threshold is in that range; a
    threshold above 32767 silently turns the shape rule off.
*/
namespace detection {

struct InspectionPlanCheck {
    unsigned long historyNeededMs = 0;
    unsigned long historySpanMs = 0;
    // How many ms of the latest window lie after the inspection of a
    // minimum-duration occurrence (0 when every window is complete).
    unsigned long earlyWindowMs = 0;
    bool historyFits = true;
    bool shapeFits = true;  // DET-015: the BinShape window fits BinTrack
    bool windowCompleteAtRelease = true;
    bool shapeLevelInDomain = true;  // the shape level is a FrequencyTarget value in 0..32767 (L4)
    // Every module's medium limit is at or below its strong one: the class tests Strong first, so a medium limit
    // above strong is silently ignored (e.g. amp_medium 8000 with strong 5000 passes from 5000; deep review I-M-B).
    bool classLimitsOrdered = true;

    bool hardOk() const { return historyFits && shapeFits; }
    bool softOk() const { return windowCompleteAtRelease && shapeLevelInDomain && classLimitsOrdered; }
};

inline InspectionPlanCheck checkInspectionPlan(const ScalarTransientConfig& scalar, const InspectionPlan& plan,
                                               unsigned long historySpanMs = FeatureHistory::kBinsPerStream *
                                                   FeatureHistory::kBinDurationMs) {
    InspectionPlanCheck c;
    c.historySpanMs = historySpanMs;
    const unsigned long inspectedAfterStartMs = scalar.minTransientDurationMs + scalar.releaseDebounceMs;
    const size_t count = plan.count > kMaxInspectionModules ? kMaxInspectionModules : plan.count;
    for (size_t i = 0; i < count; ++i) {
        const InspectionModuleConfig& module = plan.modules[i];
        if (module.kind == InspectionModuleKind::BinShape &&
            (module.shape.mediumMinBins > module.shape.strongMinBins ||
             module.shape.mediumMinRisingSteps > module.shape.strongMinRisingSteps)) {
            c.classLimitsOrdered = false;
        }
        if (module.kind == InspectionModuleKind::MagnitudeFeatureStrength &&
            module.magnitude.supportStrength.mediumPeakThreshold >
                module.magnitude.supportStrength.strongPeakThreshold) {
            c.classLimitsOrdered = false;
        }
        if (module.kind == InspectionModuleKind::BinShape) {
            // DET-015: the shape window (look-back + the longest occurrence + margin) must fit the bin track
            // at the time it is read, which is up to the release debounce after the release; the track holds
            // the kMs ms up to and including now, hence the + 1.
            const unsigned long shapeNeeded =
                module.shape.preMs + scalar.maxTransientDurationMs + scalar.releaseDebounceMs + module.shape.tailMs + 1;
            if (shapeNeeded > BinTrack::kMs) {
                c.shapeFits = false;
            }
            if (scalar.observedStream != FeatureStreamId::FrequencyTarget ||
                scalar.onsetReleaseThreshold > static_cast<float>(audio::kStrength16Max)) {
                c.shapeLevelInDomain = false;
            }
            continue;
        }
        if (module.kind != InspectionModuleKind::MagnitudeFeatureStrength) {
            continue;
        }
        const MagnitudeFeatureInspectionConfig& m = module.magnitude;
        const unsigned long needed =
            scalar.maxTransientDurationMs + scalar.releaseDebounceMs + m.windowPreMs + m.windowPostMs;
        if (needed > c.historyNeededMs) {
            c.historyNeededMs = needed;
        }
        // Window end after the occurrence start, worst case for the anchor.
        const unsigned long endAfterStartMs = m.anchor == MagnitudeInspectionAnchor::Release
            ? scalar.minTransientDurationMs + m.windowPostMs
            : m.windowPostMs;
        if (endAfterStartMs > inspectedAfterStartMs && endAfterStartMs - inspectedAfterStartMs > c.earlyWindowMs) {
            c.earlyWindowMs = endAfterStartMs - inspectedAfterStartMs;
        }
    }
    c.historyFits = c.historyNeededMs <= c.historySpanMs;
    c.windowCompleteAtRelease = c.earlyWindowMs == 0;
    return c;
}

}  // namespace detection
