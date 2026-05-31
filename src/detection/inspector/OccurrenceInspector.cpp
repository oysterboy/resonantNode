#include "OccurrenceInspector.h"
// OccurrenceInspector evidence annotation and inspection in source order.
namespace {

void setRejected(detection::InspectedOccurrence& out, detection::OccurrenceRejectReason reason) {
    out.decision = detection::OccurrenceDecision::Rejected;
    out.accepted = false;
    out.rejected = true;
    out.rejectReason = reason;
    out.confidence = 0.0f;
}

void fillScalarObservation(
    detection::ScalarInspectionObservation& obs,
    const detection::Occurrence& candidate,
    const detection::ScalarWindow& scalarWindow,
    const detection::ScalarWindow& preFloorWindow,
    const char* preFloorAnchor,
    bool available,
    const detection::ScalarFeatureInspectionConfig& config
) {
    const float peak = scalarWindow.peak;
    const float mean = scalarWindow.mean;
    const float last = scalarWindow.last;
    const size_t sampleCount = scalarWindow.sampleCount;
    const size_t sustainedCount = scalarWindow.sustainedCount;
    const unsigned long sustainedMs = scalarWindow.sustainedMs;
    const float sustainedThreshold = scalarWindow.sustainedThreshold;
    float classificationValue = peak;
    float lift = 0.0f;
    const char* supportBasis = "peak_absolute";
    bool classified = available;
    detection::StrengthClass strength = detection::StrengthClass::Unknown;

    switch (config.mode) {
        case detection::ScalarInspectionMode::PeakAbsolute:
            classificationValue = peak;
            supportBasis = "peak_absolute";
            strength = available ? classifyAmpStrength(classificationValue, available, config.strength) : detection::StrengthClass::Unknown;
            break;
        case detection::ScalarInspectionMode::MeanAbsolute:
            classificationValue = mean;
            supportBasis = "mean_absolute";
            strength = available ? classifyAmpStrength(classificationValue, available, config.strength) : detection::StrengthClass::Unknown;
            break;
        case detection::ScalarInspectionMode::SustainedAboveThreshold:
            classificationValue = peak;
            supportBasis = "sustained_above_threshold";
            {
                const size_t requiredSustainedCount = config.minSustainedCount > 0
                    ? config.minSustainedCount
                    : static_cast<size_t>(config.minSustainedMs > 0 ? config.minSustainedMs : 1U);
                const bool sustainedEnough = sustainedCount >= requiredSustainedCount;
                classified = available && sustainedEnough;
                strength = classified ? classifyAmpStrength(classificationValue, available, config.strength) : detection::StrengthClass::Unknown;
            }
            break;
        case detection::ScalarInspectionMode::PeakCentered:
            classificationValue = mean;
            supportBasis = "peak_centered_mean";
            lift = peak - mean;
            strength = available ? classifyAmpStrength(classificationValue, available, config.strength) : detection::StrengthClass::Unknown;
            break;
        case detection::ScalarInspectionMode::PeakCenteredLift:
            lift = peak - mean;
            classificationValue = lift;
            supportBasis = "peak_centered_lift";
            strength = available ? classifyAmpStrength(classificationValue, available, config.strength) : detection::StrengthClass::Unknown;
            break;
    }

    if (!classified) {
        lift = 0.0f;
    } else if (config.mode != detection::ScalarInspectionMode::PeakCentered && config.mode != detection::ScalarInspectionMode::PeakCenteredLift) {
        lift = classificationValue - mean;
    }

    const bool preferPeakAnchor = config.mode == detection::ScalarInspectionMode::PeakCentered ||
        config.mode == detection::ScalarInspectionMode::PeakCenteredLift;
    const char* eventAnchorName = preferPeakAnchor
        ? (candidate.peakMs != 0
            ? "peak"
            : (candidate.startMs != 0
                ? "start"
                : (candidate.releaseMs != 0 ? "release" : "fallback")))
        : (candidate.startMs != 0
            ? "start"
            : (candidate.releaseMs != 0
                ? "release"
                : (candidate.peakMs != 0 ? "peak" : "fallback")));
    const char* preFloorAnchorName = preFloorAnchor != nullptr ? preFloorAnchor : "none";

    obs.available = available;
    obs.observedOnly = true;
    obs.stream = scalarWindow.stream;
    obs.mode = config.mode;
    obs.supportBasis = supportBasis;
    obs.note = available ? "scalar_observed" : "scalar_unavailable";
    obs.windowStartMs = static_cast<int16_t>(-static_cast<int32_t>(config.windowPreMs));
    obs.windowEndMs = static_cast<int16_t>(config.windowPostMs);
    obs.anchor = eventAnchorName;
    obs.windowMs = scalarWindow.durationMs;
    obs.valueCount = scalarWindow.valueCount;
    obs.bucketCount = scalarWindow.bucketCount;
    obs.coveredMs = scalarWindow.coveredMs;
    obs.valuesPerBucket = scalarWindow.valuesPerBucket;
    obs.coverageRatio = scalarWindow.coverageRatio;
    obs.preFloorAvailable = preFloorWindow.valid;
    obs.preFloorAnchor = preFloorAnchorName;
    obs.preFloorNote = preFloorWindow.valid ? "pre_floor_observed" : "pre_floor_unavailable";
    obs.preFloorWindowStartMs = static_cast<int16_t>(-250);
    obs.preFloorWindowEndMs = static_cast<int16_t>(-50);
    obs.preFloorWindowMs = preFloorWindow.durationMs;
    obs.preFloorValueCount = preFloorWindow.valueCount;
    obs.preFloorBucketCount = preFloorWindow.bucketCount;
    obs.preFloorCoveredMs = preFloorWindow.coveredMs;
    obs.preFloorCoverageRatio = preFloorWindow.coverageRatio;
    obs.preFloorMedian = preFloorWindow.median;
    obs.preFloorP75 = preFloorWindow.p75;
    obs.preFloorRms = preFloorWindow.rms;
    obs.preFloorTrimmedMean = preFloorWindow.trimmedMean;
    obs.liftP75 = preFloorWindow.valid ? (scalarWindow.p75 - preFloorWindow.median) : 0.0f;
    obs.liftRms = preFloorWindow.valid ? (scalarWindow.rms - preFloorWindow.rms) : 0.0f;
    obs.liftTrimmedMean = preFloorWindow.valid ? (scalarWindow.trimmedMean - preFloorWindow.median) : 0.0f;
    obs.peak = peak;
    obs.mean = mean;
    obs.rms = scalarWindow.rms;
    obs.median = scalarWindow.median;
    obs.p75 = scalarWindow.p75;
    obs.p90 = scalarWindow.p90;
    obs.trimmedMean = scalarWindow.trimmedMean;
    obs.last = last;
    obs.baseline = mean;
    obs.lift = lift;
    obs.classificationValue = classificationValue;
    obs.sampleCount = sampleCount;
    obs.sustainedCount = sustainedCount;
    obs.sustainedMs = sustainedMs;
    obs.sustainedThreshold = sustainedThreshold;
    obs.strength = strength;

    (void)candidate;
}

} // namespace

namespace detection {

void OccurrenceInspector::configure(const InspectionPlan& plan) {
    _inspectionPlan = plan;
}

void OccurrenceInspector::reset() {
}

void OccurrenceInspector::inspectAcceptedOccurrence(
    InspectedOccurrence& out,
    const Occurrence& candidate,
    const FeatureHistory* featureHistory
) const {
    out.ampStrength = StrengthClass::Unknown;
    out.ampStrengthEvidence = {};
    out.scalarObservationCount = 0;
    out.frequencyScoreStrength = StrengthClass::Unknown;
    out.frequencyContrastQuality = StrengthClass::Unknown;
    out.targetBandStrength = StrengthClass::Unknown;

    for (size_t i = 0; i < _inspectionPlan.count; ++i) {
        runInspectionModule(out, candidate, featureHistory, _inspectionPlan.modules[i]);
    }

    out.occurrence.confidence = out.confidence;
    out.occurrence.ampEvidencePresent = candidate.ampEvidencePresent;
    out.occurrence.ampStrength = out.ampStrength;
    out.occurrence.ampStrengthEvidence = out.ampStrengthEvidence;
    out.occurrence.frequencyScoreStrength = out.frequencyScoreStrength;
    out.occurrence.frequencyContrastQuality = out.frequencyContrastQuality;
    out.occurrence.targetBandStrength = out.targetBandStrength;
}

void OccurrenceInspector::annotateAmpStrength(
    InspectedOccurrence& out,
    const Occurrence& candidate,
    const FeatureHistory* featureHistory,
    const ScalarFeatureInspectionConfig& config,
    EvidenceTarget target
) const {
    const bool peakCenteredWindow = config.mode == detection::ScalarInspectionMode::PeakCentered;
    const unsigned long anchorMs = peakCenteredWindow
        ? candidate.peakMs
        : candidate.startMs;
    const unsigned long startMs = anchorMs > config.windowPreMs ? anchorMs - config.windowPreMs : 0UL;
    const unsigned long endMs = peakCenteredWindow
        ? candidate.peakMs + config.windowPostMs
        : (candidate.endMs != 0
            ? candidate.endMs + config.windowPostMs
            : (candidate.releaseMs != 0 ? candidate.releaseMs + config.windowPostMs : candidate.peakMs + config.windowPostMs));
    const unsigned long preFloorAnchorMs = candidate.peakMs != 0
        ? candidate.peakMs
        : (candidate.startMs != 0
            ? candidate.startMs
            : (candidate.releaseMs != 0 ? candidate.releaseMs : candidate.endMs));
    const char* preFloorAnchorName = candidate.peakMs != 0
        ? "peak"
        : (candidate.startMs != 0
            ? "start"
            : (candidate.releaseMs != 0 ? "release" : "fallback"));
    const unsigned long preFloorStartMs = preFloorAnchorMs > 250UL ? preFloorAnchorMs - 250UL : 0UL;
    const unsigned long preFloorEndMs = preFloorAnchorMs > 50UL ? preFloorAnchorMs - 50UL : 0UL;
    ScalarInspectionObservation observation = {};
    observation.stream = config.stream;
    observation.mode = config.mode;
    observation.supportBasis = "centered_magnitude_peak";
    observation.note = "window_unavailable";
    ScalarWindow preFloorWindow = {};
    if (config.enabled && featureHistory != nullptr) {
        const float sustainedThreshold = config.mode == detection::ScalarInspectionMode::SustainedAboveThreshold
            ? config.strength.weakPeakThreshold
            : 0.0f;
        const ScalarWindow scalarWindow = featureHistory->getWindow(config.stream, startMs, endMs, sustainedThreshold);
        preFloorWindow = featureHistory->getWindow(config.stream, preFloorStartMs, preFloorEndMs, 0.0f);
        if (scalarWindow.valid) {
            fillScalarObservation(observation, candidate, scalarWindow, preFloorWindow, preFloorAnchorName, true, config);
        } else {
            observation.note = "window_invalid";
        }
    } else if (!config.enabled) {
        observation.note = "inspection_disabled";
    } else if (featureHistory == nullptr) {
        observation.note = "missing_feature_history";
    }

    if (out.scalarObservationCount < kMaxInspectionModules) {
        out.scalarObservations[out.scalarObservationCount++] = observation;
    }

    out.occurrence.ampLevel = observation.available ? observation.classificationValue : 0.0f;
    out.occurrence.ampBaseline = observation.available ? observation.baseline : 0.0f;
    if (target == detection::EvidenceTarget::AmpStrength) {
        out.ampStrength = observation.strength;
        out.ampStrengthEvidence = observation;
    } else if (target == detection::EvidenceTarget::FrequencyScoreStrength) {
        out.frequencyScoreStrength = observation.strength;
    } else if (target == detection::EvidenceTarget::FrequencyContrastQuality) {
        out.frequencyContrastQuality = observation.strength;
    } else if (target == detection::EvidenceTarget::TargetBandStrength) {
        out.targetBandStrength = observation.strength;
    }
}

void OccurrenceInspector::runInspectionModule(
    InspectedOccurrence& out,
    const Occurrence& candidate,
    const FeatureHistory* featureHistory,
    const InspectionModuleConfig& module
) const {
    switch (module.kind) {
    case InspectionModuleKind::ScalarFeatureStrength:
            if (module.target == EvidenceTarget::AmpStrength ||
                module.target == EvidenceTarget::FrequencyScoreStrength ||
                module.target == EvidenceTarget::FrequencyContrastQuality ||
                module.target == EvidenceTarget::TargetBandStrength) {
                annotateAmpStrength(out, candidate, featureHistory, module.scalar, module.target);
            }
            break;
        case InspectionModuleKind::None:
        default:
            break;
    }
}

InspectedOccurrence OccurrenceInspector::inspectImpl(
    const Occurrence& candidate,
    const FeatureHistory* featureHistory
) const {
    if (!candidate.present) {
        InspectedOccurrence out;
        out.occurrence = candidate;
        setRejected(out, OccurrenceRejectReason::UnsupportedKind);
        return out;
    }

    if (!candidate.valid) {
        InspectedOccurrence out;
        out.occurrence = candidate;
        out.durationMs = candidate.durationMs;
        out.strength = candidate.strength;
        out.confidence = 0.0f;
        setRejected(out, OccurrenceRejectReason::InvalidTiming);
        return out;
    }

    if (candidate.valid) {
        return inspectAcceptedOccurrenceResult(candidate, featureHistory);
    }

    InspectedOccurrence out;
    out.occurrence = candidate;
    out.durationMs = candidate.durationMs;
    out.strength = candidate.strength;
    setRejected(out, OccurrenceRejectReason::UnsupportedKind);
    return out;
}

InspectedOccurrence OccurrenceInspector::inspect(
    const Occurrence& candidate
) const {
    return inspectImpl(candidate, nullptr);
}

InspectedOccurrence OccurrenceInspector::inspectWithHistory(
    const Occurrence& candidate,
    const FeatureHistory* featureHistory
) const {
    return inspectImpl(candidate, featureHistory);
}

InspectedOccurrence OccurrenceInspector::inspectAcceptedOccurrenceResult(
    const Occurrence& candidate,
    const FeatureHistory* featureHistory
) const {
    InspectedOccurrence out;
    out.occurrence = candidate;
    out.durationMs = candidate.durationMs;
    out.strength = candidate.strength;
    out.confidence = candidate.confidence > 0.0f ? candidate.confidence : 1.0f;

    out.decision = OccurrenceDecision::Accepted;
    out.accepted = true;
    out.rejected = false;
    out.rejectReason = OccurrenceRejectReason::None;
    inspectAcceptedOccurrence(out, candidate, featureHistory);
    return out;
}

} // namespace detection

