#include "OccurrenceInspector.h"
// OccurrenceInspector evidence annotation and inspection in source order.
namespace {

void setRejected(detection::InspectedOccurrence& out, detection::OccurrenceRejectReason reason) {
    out.decision = detection::OccurrenceDecision::Rejected;
    out.rejectReason = reason;
}

void fillScalarObservation(
    detection::ScalarInspectionObservation& obs,
    const detection::Occurrence& occurrence,
    const detection::ScalarWindow& scalarWindow,
    const detection::ScalarWindow& preFloorWindow,
    detection::ScalarInspectionAnchor preFloorAnchor,
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
    detection::ScalarInspectionBasis supportBasis = detection::ScalarInspectionBasis::PeakAbsolute;
    bool classified = available;
    detection::StrengthClass strength = detection::StrengthClass::Unknown;

    switch (config.mode) {
        case detection::ScalarInspectionMode::PeakAbsolute:
            classificationValue = peak;
            supportBasis = detection::ScalarInspectionBasis::PeakAbsolute;
            strength = available ? classifyAmpStrength(classificationValue, available, config.strength) : detection::StrengthClass::Unknown;
            break;
        case detection::ScalarInspectionMode::MeanAbsolute:
            classificationValue = mean;
            supportBasis = detection::ScalarInspectionBasis::MeanAbsolute;
            strength = available ? classifyAmpStrength(classificationValue, available, config.strength) : detection::StrengthClass::Unknown;
            break;
        case detection::ScalarInspectionMode::SustainedAboveThreshold:
            classificationValue = peak;
            supportBasis = detection::ScalarInspectionBasis::SustainedAboveThreshold;
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
            supportBasis = detection::ScalarInspectionBasis::PeakCenteredMean;
            strength = available ? classifyAmpStrength(classificationValue, available, config.strength) : detection::StrengthClass::Unknown;
            break;
        case detection::ScalarInspectionMode::PeakCenteredLift:
            classificationValue = peak - mean;
            supportBasis = detection::ScalarInspectionBasis::PeakCenteredLift;
            strength = available ? classifyAmpStrength(classificationValue, available, config.strength) : detection::StrengthClass::Unknown;
            break;
    }

    const bool preferPeakAnchor = config.mode == detection::ScalarInspectionMode::PeakCentered ||
        config.mode == detection::ScalarInspectionMode::PeakCenteredLift;
    detection::ScalarInspectionAnchor eventAnchor = preferPeakAnchor
        ? (occurrence.peakMs != 0
            ? detection::ScalarInspectionAnchor::Peak
            : (occurrence.startMs != 0
                ? detection::ScalarInspectionAnchor::Start
                : (occurrence.releaseMs != 0 ? detection::ScalarInspectionAnchor::Release : detection::ScalarInspectionAnchor::Fallback)))
        : (occurrence.startMs != 0
            ? detection::ScalarInspectionAnchor::Start
            : (occurrence.releaseMs != 0
                ? detection::ScalarInspectionAnchor::Release
                : (occurrence.peakMs != 0 ? detection::ScalarInspectionAnchor::Peak : detection::ScalarInspectionAnchor::Fallback)));
    obs.available = available;
    obs.stream = scalarWindow.stream;
    obs.mode = config.mode;
    obs.supportBasis = supportBasis;
    obs.note = available ? detection::ScalarInspectionNote::ScalarObserved : detection::ScalarInspectionNote::ScalarUnavailable;
    obs.windowStartMs = static_cast<int16_t>(-static_cast<int32_t>(config.windowPreMs));
    obs.windowEndMs = static_cast<int16_t>(config.windowPostMs);
    obs.anchor = eventAnchor;
    obs.windowMs = scalarWindow.durationMs;
    obs.valueCount = scalarWindow.valueCount;
    obs.bucketCount = scalarWindow.bucketCount;
    obs.coveredMs = scalarWindow.coveredMs;
    obs.valuesPerBucket = scalarWindow.valuesPerBucket;
    obs.coverageRatio = scalarWindow.coverageRatio;
    obs.spanMs = scalarWindow.spanMs;
    obs.latestValueAgeMs = scalarWindow.latestValueAgeMs;
    obs.preFloorAvailable = preFloorWindow.valid;
    obs.preFloorAnchor = preFloorAnchor;
    obs.preFloorNote = preFloorWindow.valid ? detection::ScalarInspectionNote::PreFloorObserved : detection::ScalarInspectionNote::PreFloorUnavailable;
    obs.preFloorWindowStartMs = static_cast<int16_t>(-static_cast<int32_t>(config.preFloorWindowPreMs));
    obs.preFloorWindowEndMs = static_cast<int16_t>(-static_cast<int32_t>(config.preFloorWindowPostMs));
    obs.preFloorWindowMs = preFloorWindow.durationMs;
    obs.preFloorValueCount = preFloorWindow.valueCount;
    obs.preFloorBucketCount = preFloorWindow.bucketCount;
    obs.preFloorCoveredMs = preFloorWindow.coveredMs;
    obs.preFloorCoverageRatio = preFloorWindow.coverageRatio;
    obs.preFloorSpanMs = preFloorWindow.spanMs;
    obs.preFloorLatestValueAgeMs = preFloorWindow.latestValueAgeMs;
    obs.preFloorMedian = preFloorWindow.median;
    obs.preFloorP75 = preFloorWindow.p75;
    obs.preFloorRms = preFloorWindow.rms;
    obs.preFloorTrimmedMean = preFloorWindow.trimmedMean;
    obs.peak = peak;
    obs.mean = mean;
    obs.rms = scalarWindow.rms;
    obs.median = scalarWindow.median;
    obs.p75 = scalarWindow.p75;
    obs.p90 = scalarWindow.p90;
    obs.trimmedMean = scalarWindow.trimmedMean;
    obs.last = last;
    obs.classificationValue = classificationValue;
    obs.sampleCount = sampleCount;
    obs.sustainedCount = sustainedCount;
    obs.sustainedMs = sustainedMs;
    obs.sustainedThreshold = sustainedThreshold;
    obs.strength = strength;

    (void)occurrence;
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
    const Occurrence& occurrence,
    const FeatureHistory* featureHistory
) const {
    out.scalarObservationCount = 0;

    for (size_t i = 0; i < _inspectionPlan.count; ++i) {
        runInspectionModule(out, occurrence, featureHistory, _inspectionPlan.modules[i]);
    }
}

void OccurrenceInspector::annotateAmpStrength(
    InspectedOccurrence& out,
    const Occurrence& occurrence,
    const FeatureHistory* featureHistory,
    const ScalarFeatureInspectionConfig& config,
    EvidenceTarget target
) const {
    const bool peakCenteredWindow = config.mode == detection::ScalarInspectionMode::PeakCentered;
    const unsigned long anchorMs = peakCenteredWindow
        ? occurrence.peakMs
        : occurrence.startMs;
    const unsigned long startMs = anchorMs > config.windowPreMs ? anchorMs - config.windowPreMs : 0UL;
    const unsigned long endMs = peakCenteredWindow
        ? occurrence.peakMs + config.windowPostMs
        : (occurrence.endMs != 0
            ? occurrence.endMs + config.windowPostMs
            : (occurrence.releaseMs != 0 ? occurrence.releaseMs + config.windowPostMs : occurrence.peakMs + config.windowPostMs));
    const unsigned long preFloorAnchorMs = occurrence.peakMs != 0
        ? occurrence.peakMs
        : (occurrence.startMs != 0
            ? occurrence.startMs
            : (occurrence.releaseMs != 0 ? occurrence.releaseMs : occurrence.endMs));
    const detection::ScalarInspectionAnchor preFloorAnchorValue = occurrence.peakMs != 0
        ? detection::ScalarInspectionAnchor::Peak
        : (occurrence.startMs != 0
            ? detection::ScalarInspectionAnchor::Start
            : (occurrence.releaseMs != 0 ? detection::ScalarInspectionAnchor::Release : detection::ScalarInspectionAnchor::Fallback));
    const unsigned long preFloorStartMs = preFloorAnchorMs > config.preFloorWindowPreMs ? preFloorAnchorMs - config.preFloorWindowPreMs : 0UL;
    const unsigned long preFloorEndMs = preFloorAnchorMs > config.preFloorWindowPostMs ? preFloorAnchorMs - config.preFloorWindowPostMs : 0UL;
    ScalarInspectionObservation observation = {};
    observation.stream = config.stream;
    observation.mode = config.mode;
    observation.supportBasis = detection::ScalarInspectionBasis::CenteredMagnitudePeak;
    observation.note = detection::ScalarInspectionNote::WindowInvalid;
    ScalarWindow preFloorWindow = {};
    if (config.enabled && featureHistory != nullptr) {
        const float sustainedThreshold = config.mode == detection::ScalarInspectionMode::SustainedAboveThreshold
            ? config.strength.weakPeakThreshold
            : 0.0f;
        const ScalarWindow scalarWindow = featureHistory->getWindow(config.stream, startMs, endMs, sustainedThreshold);
        preFloorWindow = featureHistory->getWindow(config.stream, preFloorStartMs, preFloorEndMs, 0.0f);
        if (scalarWindow.valid) {
            fillScalarObservation(observation, occurrence, scalarWindow, preFloorWindow, preFloorAnchorValue, true, config);
        } else {
            observation.note = detection::ScalarInspectionNote::WindowInvalid;
        }
    } else if (!config.enabled) {
        observation.note = detection::ScalarInspectionNote::InspectionDisabled;
    } else if (featureHistory == nullptr) {
        observation.note = detection::ScalarInspectionNote::MissingFeatureHistory;
    }

    if (out.scalarObservationCount < kMaxInspectionModules) {
        out.scalarObservations[out.scalarObservationCount++] = observation;
    }

    if (target == detection::EvidenceTarget::AmpStrength) {
        out.occurrence.scalar.present = observation.available;
        out.occurrence.scalar.value = observation.available ? observation.classificationValue : 0.0f;
        out.occurrence.scalar.baseline = observation.available ? observation.mean : 0.0f;
        out.occurrence.scalar.lift = observation.available
            ? (out.occurrence.scalar.value - out.occurrence.scalar.baseline)
            : 0.0f;
        out.occurrence.scalar.strength = observation.available ? observation.classificationValue : 0.0f;
        out.occurrence.scalar.strengthClass = observation.strength;
    } else if (target == detection::EvidenceTarget::FrequencyScoreStrength) {
        out.occurrence.frequency.scoreStrength = observation.strength;
    } else if (target == detection::EvidenceTarget::FrequencyContrastQuality) {
        out.occurrence.frequency.contrastQuality = observation.strength;
    } else if (target == detection::EvidenceTarget::TargetBandStrength) {
        out.occurrence.frequency.targetBandStrength = observation.strength;
    }

    out.occurrence.scalar.evidence = observation;
}

void OccurrenceInspector::runInspectionModule(
    InspectedOccurrence& out,
    const Occurrence& occurrence,
    const FeatureHistory* featureHistory,
    const InspectionModuleConfig& module
) const {
    switch (module.kind) {
    case InspectionModuleKind::ScalarFeatureStrength:
            if (module.target == EvidenceTarget::AmpStrength ||
                module.target == EvidenceTarget::FrequencyScoreStrength ||
                module.target == EvidenceTarget::FrequencyContrastQuality ||
                module.target == EvidenceTarget::TargetBandStrength) {
                annotateAmpStrength(out, occurrence, featureHistory, module.scalar, module.target);
            }
            break;
        case InspectionModuleKind::None:
        default:
            break;
    }
}

InspectedOccurrence OccurrenceInspector::inspectImpl(
    const Occurrence& occurrence,
    const FeatureHistory* featureHistory
) const {
    if (!occurrence.present) {
        InspectedOccurrence out;
        out.occurrence = occurrence;
        setRejected(out, OccurrenceRejectReason::UnsupportedKind);
        return out;
    }

    if (!occurrence.valid) {
        InspectedOccurrence out;
        out.occurrence = occurrence;
        setRejected(out, OccurrenceRejectReason::InvalidTiming);
        return out;
    }

    if (occurrence.valid) {
        return inspectAcceptedOccurrenceResult(occurrence, featureHistory);
    }

    InspectedOccurrence out;
    out.occurrence = occurrence;
    setRejected(out, OccurrenceRejectReason::UnsupportedKind);
    return out;
}

InspectedOccurrence OccurrenceInspector::inspect(
    const Occurrence& occurrence
) const {
    return inspectImpl(occurrence, nullptr);
}

InspectedOccurrence OccurrenceInspector::inspectWithHistory(
    const Occurrence& occurrence,
    const FeatureHistory* featureHistory
) const {
    return inspectImpl(occurrence, featureHistory);
}

InspectedOccurrence OccurrenceInspector::inspectAcceptedOccurrenceResult(
    const Occurrence& occurrence,
    const FeatureHistory* featureHistory
) const {
    InspectedOccurrence out;
    out.occurrence = occurrence;
    out.occurrence.confidence = occurrence.confidence > 0.0f ? occurrence.confidence : 1.0f;
    out.decision = OccurrenceDecision::Accepted;
    out.rejectReason = OccurrenceRejectReason::None;
    inspectAcceptedOccurrence(out, occurrence, featureHistory);
    return out;
}

} // namespace detection

