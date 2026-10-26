#pragma once

#include <strings.h>

#include "detectors/scalar/ScalarTransientConfig.h"
#include "field/FieldState.h"
#include "inspection/InspectorTypes.h"

namespace detection {

/*
DetectionProfile

Code-defined detection profile composition.
Profiles select the detector, its config, the inspection plan (which is also
the verdict's requirements) and the field-state config.

Profiles declare composition; DetectionRuntime applies the selected fields at fixed stages.

Common enum / selector types used in this file:

```text
DetectionProfileKind { AmpExperimental, TonalPulseScalar }
DetectorSelection { ScalarTransient }
FeatureStreamId { AmpEnvelope, FrequencyTarget, FrequencyContrast }
StrengthClass { Unknown, None, Weak, Medium, Strong }
InspectionModuleKind { None, MagnitudeFeatureStrength }
```

New profile checklist:
- add the kind here
- register it in `detectionProfileForKind(...)`
- add its display name in `detectionProfileName(...)`
- add parser support in `detectionProfileKindFromName(...)`
-------------------------------------------------------------
- add a factory in this file
--- inline DetectionProfile makePROFILENAMEProfile()
- set `detectorSelection` in the profile factory
- set the detector-specific config blocks in the profile factory
- set `inspectionPlan`
- set `fieldStateConfig`
- update SEQ help/parser if the analyzer should accept it
- update RB help/parser and behavior mapping if the resonant node should accept it
---------------------------
- a new detector family: docs/specs/myspec.md 5.3, "Adding a detector family"
  (a build flag, not a new runtime `DetectorSelection` choice)
*/

enum class DetectorSelection {
    // The one detector family. FrequencyMatch was removed 2026-10-22
    // (docs/decisions/2026-10-22-remove-frequencymatch.md).
    ScalarTransient,
};


enum class DetectionProfileKind {
     // lists available Profiles
    AmpExperimental,
    TonalPulseScalar,
};

struct DetectionProfile {
    // Declaration and defaults
    
    // Identity and composition.
    DetectionProfileKind kind = DetectionProfileKind::TonalPulseScalar;
    DetectorSelection detectorSelection = DetectorSelection::ScalarTransient;

    // Stage configuration.
    ScalarTransientConfig scalarTransient = {};
    InspectionPlan inspectionPlan = {};
    FieldStateConfig fieldStateConfig = {};
};

// The first enabled inspection module: the requirement the status lines name.
inline const InspectionModuleConfig* firstEnabledRequirement(const InspectionPlan& plan) {
    const size_t count = plan.count > kMaxInspectionModules ? kMaxInspectionModules : plan.count;
    for (size_t i = 0; i < count; ++i) {
        if (plan.modules[i].enabled) {
            return &plan.modules[i];
        }
    }
    return nullptr;
}

// TonalPulseScalar amp inspector medium limit. 1,500 since 2026-10-16 (was 2,500): the amp
// value does not separate hops from radio noise, it stays a floor against very weak signals.
// docs/decisions/2026-10-16-hop-acceptance-contrast-gate-not-amp.md. Build override kept
// for experiments (-D SCALAR_AMP_MEDIUM_THRESHOLD=...).
#ifndef SCALAR_AMP_MEDIUM_THRESHOLD
#define SCALAR_AMP_MEDIUM_THRESHOLD 1500.0f
#endif

// TonalPulseScalar contrast inspector medium limit. 30,500 since 2026-10-16 (was 20,000):
// the contrast P75 is the gate against non-tonal sound (26 of 27 radio false answers below
// 30,500, chirps and bin-aligned hops at 31,000-32,750). Same decision file. Build override
// kept for experiments (-D SCALAR_CONTRAST_MEDIUM_THRESHOLD=...).
#ifndef SCALAR_CONTRAST_MEDIUM_THRESHOLD
#define SCALAR_CONTRAST_MEDIUM_THRESHOLD 30500.0f
#endif

// Actual profiles. These are the concrete profile definitions used at runtime.
inline DetectionProfile makeTonalPulseScalarProfile() {
    DetectionProfile profile;

    // Identity and occurrence routing.
    profile.kind = DetectionProfileKind::TonalPulseScalar; // Profile ID for tonal pulse scalar analysis.
    profile.detectorSelection = DetectorSelection::ScalarTransient; // Use the scalar transient detector path.
    // This profile observes a frequency-derived scalar stream, not raw PCM.
    // "Offline" values here are deduced from RAW PCM captures and then mapped
    // into the normalized magnitude-like 0..32767 scale.
    profile.scalarTransient.observedStream = FeatureStreamId::FrequencyTarget; // Track the target-band scalar stream.
    profile.scalarTransient.onsetDetectionThreshold = 2000.0f; // Attack threshold for candidate start.
    profile.scalarTransient.onsetReleaseThreshold = 1500.0f; // Release threshold for candidate end.
    profile.scalarTransient.minTransientDurationMs = 92; // Reject pulses that are too short. 92 since 2026-10-23 (was 85; DET-017, owner): above a 40 ms beep + room reflection (86 ms, bench/sessions/2026-10-22-7d-dur40-raw), below the shortest hop train with one step in a deep null (97 ms, host notch sweep). 100 lost the deep null on the middle step.
    profile.scalarTransient.maxTransientDurationMs = 300; // Reject pulses that are too long. 300 since 2026-10-16 (was 200): a 5-step hop train (25 ms x 5, gap 10, 165 ms) is seen as up to 214 ms with the room tail; the 100 ms chirp is unaffected. Noise cost of 300 alone not yet measured (lab note 2026-10-16, open items).

    profile.scalarTransient.releaseDebounceMs = 10; // Require a short stable release before ending.
    profile.scalarTransient.cooldownAfterOnsetMs = 50; // Avoid immediate re-triggering on the same pulse.
    // Minimum strength gate for the matched mean / peak utility switch.
    profile.scalarTransient.requireMinStrength = true;
    profile.scalarTransient.minTransientPeakStrength = 3000.0f; // Minimum peak gate for the carrier.
    profile.scalarTransient.minMatchedMeanStrength = 0.0f; // Mean over samples above the release threshold.
    // Carrier quality gates to reject fragmented or weak target-band coverage.
    profile.scalarTransient.requireCarrierQuality = true;
    profile.scalarTransient.minCoverageAboveReleaseMs = 80; // Minimum time above release level.
    profile.scalarTransient.minLongestIslandMs = 20; // Longest continuous island above release level.
    profile.scalarTransient.maxGapMs = 20; // Largest allowed gap inside the candidate.


    // Inspection: Frequency Contrast
    profile.inspectionPlan = InspectionPlan{};  // not "= {}": g++ 13 rejects a brace list on this aggregate (NODE-007)
    profile.inspectionPlan.modules[0].kind = InspectionModuleKind::MagnitudeFeatureStrength; // Magnitude inspector module.
    profile.inspectionPlan.modules[0].target = InspectionTarget::Contrast; // Route this observation as contrast evidence.
    profile.inspectionPlan.modules[0].enabled = true; // Enable the module in the matcher.
    profile.inspectionPlan.modules[0].magnitude.stream = FeatureStreamId::FrequencyContrast; // Measure frequency contrast.
    profile.inspectionPlan.modules[0].magnitude.anchor = MagnitudeInspectionAnchor::Start; // Window from occurrence start.
    profile.inspectionPlan.modules[0].magnitude.windowPreMs = 0; // No look-back before the anchor.
    profile.inspectionPlan.modules[0].magnitude.windowPostMs = 100; // Inspect the first 100 ms after onset.
    profile.inspectionPlan.modules[0].minimumStrength = StrengthClass::Medium; // Require at least medium evidence.
    profile.inspectionPlan.modules[0].magnitude.mode = MagnitudeInspectionMode::P75; // Use a robust percentile summary.
    profile.inspectionPlan.modules[0].magnitude.supportStrength.strongPeakThreshold = 300000.0f; // Above the 0..32,767 contrast range on purpose: Strong is unused for contrast; the requirement is Medium (step 8b item 4, owner 2026-10-22).
    profile.inspectionPlan.modules[0].magnitude.supportStrength.mediumPeakThreshold = SCALAR_CONTRAST_MEDIUM_THRESHOLD; // Medium contrast limit, SCALAR_CONTRAST_MEDIUM_THRESHOLD above; live as detection.contrast_medium.
    profile.inspectionPlan.modules[0].magnitude.supportStrength.weakPeakThreshold = 10000.0f; // Weak contrast threshold.


    // Secondary Inspection: Amplitude
    profile.inspectionPlan.modules[1].kind = InspectionModuleKind::MagnitudeFeatureStrength; // Magnitude inspector module.
    profile.inspectionPlan.modules[1].target = InspectionTarget::Amp; // Route this observation as amplitude evidence.
    profile.inspectionPlan.modules[1].enabled = true; // Enable the module in the matcher.
    profile.inspectionPlan.modules[1].magnitude.stream = FeatureStreamId::AmpMagnitude; // Measure the magnitude stream.
    profile.inspectionPlan.modules[1].magnitude.anchor = MagnitudeInspectionAnchor::Start; // Window from occurrence start.
    profile.inspectionPlan.modules[1].magnitude.windowPreMs = 0; // No look-back before the anchor.
    profile.inspectionPlan.modules[1].magnitude.windowPostMs = 100; // Inspect the first 100 ms after onset.
    profile.inspectionPlan.modules[1].minimumStrength = StrengthClass::Medium; // Require at least medium evidence.
    profile.inspectionPlan.modules[1].magnitude.mode = MagnitudeInspectionMode::P75; // Use a robust percentile summary.
    profile.inspectionPlan.modules[1].magnitude.supportStrength.strongPeakThreshold = 5000.0f; // Strong amplitude threshold.
    profile.inspectionPlan.modules[1].magnitude.supportStrength.mediumPeakThreshold = SCALAR_AMP_MEDIUM_THRESHOLD; // Medium amplitude limit, SCALAR_AMP_MEDIUM_THRESHOLD above; live as detection.amp_medium.
    profile.inspectionPlan.modules[1].magnitude.supportStrength.weakPeakThreshold = 1000.0f; // Weak amplitude threshold.

    // Third: shape (DET-015, decisions/2026-10-26-det015-bin-track-inspector.md, E002). The Node hears the hop
    // train only (decisions/2026-10-23-node-hears-the-hop-train-only.md): at least 3 distinct band bins with 2
    // steps up over the occurrence. A steady tone (plain chirp, beep) is one bin and is rejected; a train cut to
    // three steps still passes. Band mode only: with no band bins the module is not applicable and passes.
    profile.inspectionPlan.modules[2].kind = InspectionModuleKind::BinShape;
    profile.inspectionPlan.modules[2].target = InspectionTarget::Shape;
    profile.inspectionPlan.modules[2].enabled = true;
    profile.inspectionPlan.modules[2].minimumStrength = StrengthClass::Medium;
    profile.inspectionPlan.modules[2].shape.tailMs = 0;  // margin after the release; a tuning question for the board
    // A deep null in the middle of a train splits it at the detector: the first steps fall under the minimum
    // duration and only the rest is an occurrence (the 5h notch sweep, null at 3450). The track still holds the
    // first part, so the inspector looks back 120 ms, and a step over one missing bin counts as rising. With these
    // every single-null variant passes again; no recorded capture changes (E002, step 11b).
    profile.inspectionPlan.modules[2].shape.preMs = 120;
    profile.inspectionPlan.modules[2].shape.maxRisingStepBins = 2;
    // The level a bin must reach to count is the detector's release threshold, read live by the runtime (not a
    // field of the plan: review D1.5).
    profile.inspectionPlan.count = 3; // contrast, amp, shape.


    // Field-state windowing.
    profile.fieldStateConfig.occurrenceWindowMs = 4000; // Window for occurrence density tracking.
    profile.fieldStateConfig.patternWindowMs = 4000; // Window for pattern density tracking.
    profile.fieldStateConfig.busyOccurrenceCountThreshold = 3; // Occurrence count considered busy.
    profile.fieldStateConfig.denseOccurrenceCountThreshold = 6; // Occurrence count considered dense.
    profile.fieldStateConfig.busyActivityThreshold = 0.45f; // Activity threshold for busy field state.
    return profile;
}

inline DetectionProfile makeAmpExperimentalProfile() {
    DetectionProfile profile;

    // Identity and occurrence routing.
    profile.kind = DetectionProfileKind::AmpExperimental;
    profile.detectorSelection = DetectorSelection::ScalarTransient;
    profile.scalarTransient.observedStream = FeatureStreamId::AmpEnvelope;
    // Lab-calibrated AMP thresholds: the default scalar detector thresholds are
    // too high for the current scalar magnitude range on analyzer runs.
    profile.scalarTransient.onsetDetectionThreshold = 18000.0f;
    profile.scalarTransient.onsetReleaseThreshold = 12000.0f;
    profile.scalarTransient.cooldownAfterOnsetMs = 300;
    profile.scalarTransient.minTransientDurationMs = 60;
    profile.scalarTransient.maxTransientDurationMs = 240;
    profile.scalarTransient.minTransientPeakStrength = 15000.0f;
    profile.scalarTransient.releaseDebounceMs = 30;
    profile.scalarTransient.requireMinStrength = true;
    // Inspector composition.


    profile.inspectionPlan.modules[0].kind = InspectionModuleKind::MagnitudeFeatureStrength;
    profile.inspectionPlan.modules[0].target = InspectionTarget::Amp;
    profile.inspectionPlan.modules[0].enabled = true;
    profile.inspectionPlan.modules[0].minimumStrength = StrengthClass::Medium;
    profile.inspectionPlan.modules[0].magnitude.anchor = MagnitudeInspectionAnchor::Peak;
    profile.inspectionPlan.modules[0].magnitude.stream = FeatureStreamId::AmpEnvelope;
    profile.inspectionPlan.modules[0].magnitude.mode = MagnitudeInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[0].magnitude.windowPreMs = 10;
    profile.inspectionPlan.modules[0].magnitude.windowPostMs = 90;
    profile.inspectionPlan.modules[0].magnitude.supportStrength.strongPeakThreshold = 18000.0f;
    profile.inspectionPlan.modules[0].magnitude.supportStrength.mediumPeakThreshold = 12000.0f;
    profile.inspectionPlan.modules[0].magnitude.supportStrength.weakPeakThreshold = 8000.0f;

    profile.inspectionPlan.modules[1].kind = InspectionModuleKind::MagnitudeFeatureStrength;
    profile.inspectionPlan.modules[1].target = InspectionTarget::TargetScore;
    profile.inspectionPlan.modules[1].enabled = true;
    profile.inspectionPlan.modules[1].minimumStrength = StrengthClass::Medium;
    profile.inspectionPlan.modules[1].magnitude.anchor = MagnitudeInspectionAnchor::Peak;
    profile.inspectionPlan.modules[1].magnitude.stream = FeatureStreamId::FrequencyTarget;
    profile.inspectionPlan.modules[1].magnitude.mode = MagnitudeInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[1].magnitude.windowPreMs = 10;
    profile.inspectionPlan.modules[1].magnitude.windowPostMs = 90;
    profile.inspectionPlan.modules[1].magnitude.supportStrength.strongPeakThreshold = 18000.0f;
    profile.inspectionPlan.modules[1].magnitude.supportStrength.mediumPeakThreshold = 12000.0f;
    profile.inspectionPlan.modules[1].magnitude.supportStrength.weakPeakThreshold = 8000.0f;
    //profile.inspectionPlan.modules[1].magnitude.minSustainedMs = 25;
    
    profile.inspectionPlan.modules[2].kind = InspectionModuleKind::MagnitudeFeatureStrength;
    profile.inspectionPlan.modules[2].target = InspectionTarget::Contrast;
    profile.inspectionPlan.modules[2].enabled = true;
    profile.inspectionPlan.modules[2].minimumStrength = StrengthClass::Medium;
    profile.inspectionPlan.modules[2].magnitude.anchor = MagnitudeInspectionAnchor::Peak;
    profile.inspectionPlan.modules[2].magnitude.stream = FeatureStreamId::FrequencyContrast;
    profile.inspectionPlan.modules[2].magnitude.mode = MagnitudeInspectionMode::PeakCentered;
    profile.inspectionPlan.modules[2].magnitude.windowPreMs = 10;
    profile.inspectionPlan.modules[2].magnitude.windowPostMs = 90;
    profile.inspectionPlan.modules[2].magnitude.supportStrength.strongPeakThreshold = 80.0f;
    profile.inspectionPlan.modules[2].magnitude.supportStrength.mediumPeakThreshold = 50.0f;
    profile.inspectionPlan.modules[2].magnitude.supportStrength.weakPeakThreshold = 25.0f;
    //profile.inspectionPlan.modules[2].magnitude.minSustainedMs = 25;  
    
    profile.inspectionPlan.count = 3;

    // Field-state windowing.
    profile.fieldStateConfig.occurrenceWindowMs = 4000;
    profile.fieldStateConfig.patternWindowMs = 4000;
    profile.fieldStateConfig.busyOccurrenceCountThreshold = 3;
    profile.fieldStateConfig.denseOccurrenceCountThreshold = 6;
    profile.fieldStateConfig.busyActivityThreshold = 0.45f;
    return profile;
}


// Profile lookup by kind.
inline const DetectionProfile& detectionProfileForKind(DetectionProfileKind kind) {
    static const DetectionProfile kAmpExperimental = makeAmpExperimentalProfile();
    static const DetectionProfile kTonalPulseScalar = makeTonalPulseScalarProfile();

    switch (kind) {
        case DetectionProfileKind::AmpExperimental:
            return kAmpExperimental;
        case DetectionProfileKind::TonalPulseScalar:
        default:
            return kTonalPulseScalar;
    }
}

// Human-readable names for profile kinds used in logs and help text.
inline const char* detectionProfileName(DetectionProfileKind kind) {
    switch (kind) {
        case DetectionProfileKind::AmpExperimental:
            return "AmpExperimental";
        case DetectionProfileKind::TonalPulseScalar:
            return "TonalPulseScalar";
    }
    return "unknown";
}

// Human-readable names for profile-selected detector routing.
inline const char* detectorSelectionName(DetectorSelection kind) {
    switch (kind) {
        case DetectorSelection::ScalarTransient:
            return "ScalarTransient";
    }
    return "unknown";
}

// Parse profile names from user-facing text.
inline bool detectionProfileKindFromName(const char* name, DetectionProfileKind& outKind) {
    if (name == nullptr) {
        return false;
    }

    if (strcasecmp(name, "ampexperimental") == 0 || strcasecmp(name, "amp") == 0) {
        outKind = DetectionProfileKind::AmpExperimental;
        return true;
    }
    if (strcasecmp(name, "tonalpulsescalar") == 0 || strcasecmp(name, "tonal_pulse_scalar") == 0 || strcasecmp(name, "scalar_freq_experimental") == 0) {
        outKind = DetectionProfileKind::TonalPulseScalar;
        return true;
    }

    return false;
}

} // namespace detection

