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

Common enum / selector types used in this file (as of 2026-10-28; the enum
definitions are the source):

```text
DetectionProfileKind { AmpExperimental, TonalPulseScalar }
DetectorSelection { ScalarTransient }
FeatureStreamId { Unknown, AmpMagnitude, AmpEnvelope, FrequencyTarget, FrequencyContrast }
StrengthClass { Unknown, None, Weak, Medium, Strong }
InspectionModuleKind { None, MagnitudeFeatureStrength, BinShape }
InspectionTarget { None, Amp, TargetScore, Contrast, TargetBand, Shape }
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
    AmpExperimental,
    TonalPulseScalar,
};

struct DetectionProfile {
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

// TonalPulseScalar inspector medium limits (decisions/2026-10-16-hop-acceptance-contrast-gate-not-amp.md): the
// contrast P75 is the gate against non-tonal sound, the amp value only a floor against very weak signals. Live as
// detection.contrast_medium / detection.amp_medium; the build overrides are kept for experiments.
#ifndef SCALAR_AMP_MEDIUM_THRESHOLD
#define SCALAR_AMP_MEDIUM_THRESHOLD 1500.0f
#endif

#ifndef SCALAR_CONTRAST_MEDIUM_THRESHOLD
#define SCALAR_CONTRAST_MEDIUM_THRESHOLD 30500.0f
#endif

// One magnitude inspection module, enabled and requiring Medium, as every shipped one is. The summary of the
// window (mode) classifies as Strong / Medium / Weak at or above the three thresholds (classifySupportStrength).
inline InspectionModuleConfig magnitudeModule(InspectionTarget target, FeatureStreamId stream,
                                              MagnitudeInspectionAnchor anchor, MagnitudeInspectionMode mode,
                                              uint32_t windowPreMs, uint32_t windowPostMs, float strong, float medium,
                                              float weak) {
    InspectionModuleConfig config;
    config.kind = InspectionModuleKind::MagnitudeFeatureStrength;
    config.target = target;
    config.enabled = true;
    config.minimumStrength = StrengthClass::Medium;
    config.magnitude.stream = stream;
    config.magnitude.anchor = anchor;
    config.magnitude.mode = mode;
    config.magnitude.windowPreMs = windowPreMs;
    config.magnitude.windowPostMs = windowPostMs;
    config.magnitude.supportStrength.strongPeakThreshold = strong;
    config.magnitude.supportStrength.mediumPeakThreshold = medium;
    config.magnitude.supportStrength.weakPeakThreshold = weak;
    return config;
}

// One bin-shape module (DET-015), enabled and requiring Medium; the class limits keep ShapeInspectionConfig's
// defaults (live as detection.shape_*).
inline InspectionModuleConfig binShapeModule(uint32_t preMs, uint32_t tailMs, uint16_t maxRisingStepBins) {
    InspectionModuleConfig config;
    config.kind = InspectionModuleKind::BinShape;
    config.target = InspectionTarget::Shape;
    config.enabled = true;
    config.minimumStrength = StrengthClass::Medium;
    config.shape.tailMs = tailMs;
    config.shape.preMs = preMs;
    config.shape.maxRisingStepBins = maxRisingStepBins;
    return config;
}

// The shipped profile: the Node and the Analyzer's default.
inline DetectionProfile makeTonalPulseScalarProfile() {
    DetectionProfile profile;
    profile.kind = DetectionProfileKind::TonalPulseScalar;
    profile.detectorSelection = DetectorSelection::ScalarTransient;

    // The detector watches the target-band scalar (FrequencyTarget, 0..32,767), not raw PCM; the limits were
    // read off RAW PCM captures mapped onto that scale.
    profile.scalarTransient.observedStream = FeatureStreamId::FrequencyTarget;
    profile.scalarTransient.onsetDetectionThreshold = 2000.0f;
    profile.scalarTransient.onsetReleaseThreshold = 1500.0f;
    profile.scalarTransient.minTransientDurationMs = 92;  // decisions/2026-10-23-node-hears-the-hop-train-only.md
    // decisions/2026-10-16-hop-acceptance-contrast-gate-not-amp.md (a hop train reads up to 214 ms); the noise
    // cost of 300 alone is not measured.
    profile.scalarTransient.maxTransientDurationMs = 300;
    profile.scalarTransient.releaseDebounceMs = 10;
    profile.scalarTransient.cooldownAfterOnsetMs = 50;
    profile.scalarTransient.requireMinStrength = true;
    profile.scalarTransient.minTransientPeakStrength = 3000.0f;
    profile.scalarTransient.minMatchedMeanStrength = 0.0f;
    profile.scalarTransient.requireCarrierQuality = true;
    profile.scalarTransient.minCoverageAboveReleaseMs = 80;
    profile.scalarTransient.minLongestIslandMs = 20;
    profile.scalarTransient.maxGapMs = 20;

    // Inspection: contrast (the gate), amp (a floor), shape (the hop train). The P75 windows start at the
    // occurrence. The contrast Strong limit sits above the 0..32,767 range on purpose: Strong is unused there, the
    // requirement is Medium (step 8b item 4, owner 2026-10-22); the inspector has no disabled class.
    profile.inspectionPlan = InspectionPlan{};  // not "= {}": g++ 13 rejects a brace list on this aggregate (NODE-007)
    profile.inspectionPlan.modules[0] = magnitudeModule(InspectionTarget::Contrast, FeatureStreamId::FrequencyContrast,
                                                        MagnitudeInspectionAnchor::Start, MagnitudeInspectionMode::P75,
                                                        0, 100, 300000.0f, SCALAR_CONTRAST_MEDIUM_THRESHOLD, 10000.0f);
    profile.inspectionPlan.modules[1] =
        magnitudeModule(InspectionTarget::Amp, FeatureStreamId::AmpMagnitude, MagnitudeInspectionAnchor::Start,
                        MagnitudeInspectionMode::P75, 0, 100, 5000.0f, SCALAR_AMP_MEDIUM_THRESHOLD, 1000.0f);
    // Shape (decisions/2026-10-26-det015-bin-track-inspector.md, E002; the Node hears the hop train only,
    // decisions/2026-10-23-node-hears-the-hop-train-only.md): 3 distinct band bins with 2 steps up; a steady tone
    // is one bin and is rejected. Band mode only: with no band bins the module does not apply and passes. preMs 120
    // and a step over one missing bin keep a train the detector split at a deep null (E002, step 11b); tailMs is a
    // tuning question for the board. The bin level is the detector's release threshold, read live (review D1.5).
    profile.inspectionPlan.modules[2] = binShapeModule(120, 0, 2);
    profile.inspectionPlan.count = 3;

    profile.fieldStateConfig.occurrenceWindowMs = 4000;
    profile.fieldStateConfig.patternWindowMs = 4000;
    profile.fieldStateConfig.busyOccurrenceCountThreshold = 3;
    profile.fieldStateConfig.denseOccurrenceCountThreshold = 6;
    return profile;
}

// The amplitude-envelope profile (SEQ profile=AmpExperimental), lab-calibrated: the default scalar limits are too
// high for the envelope's range on Analyzer runs.
inline DetectionProfile makeAmpExperimentalProfile() {
    DetectionProfile profile;
    profile.kind = DetectionProfileKind::AmpExperimental;
    profile.detectorSelection = DetectorSelection::ScalarTransient;

    profile.scalarTransient.observedStream = FeatureStreamId::AmpEnvelope;
    profile.scalarTransient.onsetDetectionThreshold = 18000.0f;
    profile.scalarTransient.onsetReleaseThreshold = 12000.0f;
    profile.scalarTransient.cooldownAfterOnsetMs = 300;
    profile.scalarTransient.minTransientDurationMs = 60;
    profile.scalarTransient.maxTransientDurationMs = 240;
    profile.scalarTransient.minTransientPeakStrength = 15000.0f;
    profile.scalarTransient.releaseDebounceMs = 30;
    profile.scalarTransient.requireMinStrength = true;

    // Inspection: amp, target score, contrast, each peak-centered (10 ms before, 90 after).
    profile.inspectionPlan.modules[0] =
        magnitudeModule(InspectionTarget::Amp, FeatureStreamId::AmpEnvelope, MagnitudeInspectionAnchor::Peak,
                        MagnitudeInspectionMode::PeakCentered, 10, 90, 18000.0f, 12000.0f, 8000.0f);
    profile.inspectionPlan.modules[1] = magnitudeModule(
        InspectionTarget::TargetScore, FeatureStreamId::FrequencyTarget, MagnitudeInspectionAnchor::Peak,
        MagnitudeInspectionMode::PeakCentered, 10, 90, 18000.0f, 12000.0f, 8000.0f);
    profile.inspectionPlan.modules[2] =
        magnitudeModule(InspectionTarget::Contrast, FeatureStreamId::FrequencyContrast, MagnitudeInspectionAnchor::Peak,
                        MagnitudeInspectionMode::PeakCentered, 10, 90, 80.0f, 50.0f, 25.0f);
    profile.inspectionPlan.count = 3;

    profile.fieldStateConfig.occurrenceWindowMs = 4000;
    profile.fieldStateConfig.patternWindowMs = 4000;
    profile.fieldStateConfig.busyOccurrenceCountThreshold = 3;
    profile.fieldStateConfig.denseOccurrenceCountThreshold = 6;
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
    if (strcasecmp(name, "tonalpulsescalar") == 0 || strcasecmp(name, "tonal_pulse_scalar") == 0 ||
        strcasecmp(name, "scalar_freq_experimental") == 0) {
        outKind = DetectionProfileKind::TonalPulseScalar;
        return true;
    }

    return false;
}

}  // namespace detection
