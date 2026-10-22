#pragma once

#include "DetectionProfile.h"
#include "features/FeatureHistory.h"

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
Soft (some occurrences are judged on a window that has not arrived):
  - complete at release: an occurrence of the minimum duration is inspected at
    release + debounce; a Start or Peak anchored window ends at most
    windowPost after its start, so min duration + debounce >= windowPost. A
    Release anchored window needs windowPost <= debounce. Shipped: 85 + 10 <
    100, so occurrences of 85-89 ms are rejected as MissingSupport whatever the
    signal (audit 3.1). The fix waits on DET-017 (branch
    step8b-defer-inspection); this stays `warn` until then.
*/
namespace detection {

struct InspectionPlanCheck {
    unsigned long historyNeededMs = 0;
    unsigned long historySpanMs = 0;
    // How many ms of the latest window lie after the inspection of a
    // minimum-duration occurrence (0 when every window is complete).
    unsigned long earlyWindowMs = 0;
    bool historyFits = true;
    bool windowCompleteAtRelease = true;

    bool hardOk() const { return historyFits; }
    bool softOk() const { return windowCompleteAtRelease; }
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
