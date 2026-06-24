#include "OccurrenceInspector.h"
// OccurrenceInspector evidence annotation and inspection in source order.
namespace {

unsigned long anchorMsForOccurrence(const detection::Occurrence& occurrence, detection::ScalarInspectionAnchor anchor);

detection::ScalarInspectionAnchor resolvedAnchorForOccurrence(
    const detection::Occurrence& occurrence,
    detection::ScalarInspectionAnchor anchor
) {
    switch (anchor) {
        case detection::ScalarInspectionAnchor::Start:
            return occurrence.startMs != 0 ? detection::ScalarInspectionAnchor::Start
                : (occurrence.peakMs != 0 ? detection::ScalarInspectionAnchor::Peak
                    : (occurrence.releaseMs != 0 ? detection::ScalarInspectionAnchor::Release : detection::ScalarInspectionAnchor::Fallback));
        case detection::ScalarInspectionAnchor::Release:
            return occurrence.releaseMs != 0 ? detection::ScalarInspectionAnchor::Release
                : (occurrence.peakMs != 0 ? detection::ScalarInspectionAnchor::Peak
                    : (occurrence.startMs != 0 ? detection::ScalarInspectionAnchor::Start : detection::ScalarInspectionAnchor::Fallback));
        case detection::ScalarInspectionAnchor::Peak:
        case detection::ScalarInspectionAnchor::Fallback:
        case detection::ScalarInspectionAnchor::None:
        default:
            return occurrence.peakMs != 0 ? detection::ScalarInspectionAnchor::Peak
                : (occurrence.startMs != 0 ? detection::ScalarInspectionAnchor::Start
                    : (occurrence.releaseMs != 0 ? detection::ScalarInspectionAnchor::Release : detection::ScalarInspectionAnchor::Fallback));
    }
}

unsigned long anchorMsForOccurrence(const detection::Occurrence& occurrence, detection::ScalarInspectionAnchor anchor) {
    switch (resolvedAnchorForOccurrence(occurrence, anchor)) {
        case detection::ScalarInspectionAnchor::Start:
            return occurrence.startMs;
        case detection::ScalarInspectionAnchor::Release:
            return occurrence.releaseMs;
        case detection::ScalarInspectionAnchor::Peak:
            return occurrence.peakMs;
        case detection::ScalarInspectionAnchor::Fallback:
        case detection::ScalarInspectionAnchor::None:
        default:
            return occurrence.peakMs != 0 ? occurrence.peakMs : (occurrence.startMs != 0 ? occurrence.startMs : occurrence.releaseMs);
    }
}

void setRejected(detection::InspectedOccurrence& out, detection::OccurrenceRejectReason reason) {
    out.decision = detection::OccurrenceDecision::Rejected;
    out.rejectReason = reason;
}

void fillScalarObservation(
    detection::ScalarInspectionObservation& obs,
    const detection::Occurrence& occurrence,
    const detection::ScalarWindow& scalarWindow,
    bool available,
    const detection::ScalarFeatureInspectionConfig& config
) {
    const float peak = scalarWindow.peak;
    const float mean = scalarWindow.mean;
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
            strength = available ? classifySupportStrength(classificationValue, available, config.supportStrength) : detection::StrengthClass::Unknown;
            break;
        case detection::ScalarInspectionMode::MeanAbsolute:
            classificationValue = mean;
            supportBasis = detection::ScalarInspectionBasis::MeanAbsolute;
            strength = available ? classifySupportStrength(classificationValue, available, config.supportStrength) : detection::StrengthClass::Unknown;
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
                strength = classified ? classifySupportStrength(classificationValue, available, config.supportStrength) : detection::StrengthClass::Unknown;
            }
            break;
        case detection::ScalarInspectionMode::PeakCentered:
            classificationValue = mean;
            supportBasis = detection::ScalarInspectionBasis::PeakCenteredMean;
            strength = available ? classifySupportStrength(classificationValue, available, config.supportStrength) : detection::StrengthClass::Unknown;
            break;
        case detection::ScalarInspectionMode::PeakCenteredLift:
            classificationValue = peak - mean;
            supportBasis = detection::ScalarInspectionBasis::PeakCenteredLift;
            strength = available ? classifySupportStrength(classificationValue, available, config.supportStrength) : detection::StrengthClass::Unknown;
            break;
        case detection::ScalarInspectionMode::Rms:
            classificationValue = scalarWindow.rms;
            supportBasis = detection::ScalarInspectionBasis::Rms;
            strength = available ? classifySupportStrength(classificationValue, available, config.supportStrength) : detection::StrengthClass::Unknown;
            break;
        case detection::ScalarInspectionMode::P75:
            classificationValue = scalarWindow.p75;
            supportBasis = detection::ScalarInspectionBasis::P75;
            strength = available ? classifySupportStrength(classificationValue, available, config.supportStrength) : detection::StrengthClass::Unknown;
            break;
    }

    const detection::ScalarInspectionAnchor eventAnchor = resolvedAnchorForOccurrence(occurrence, config.anchor);
    obs.available = available;
    obs.hasValues = scalarWindow.hasValues;
    obs.coverageComplete = scalarWindow.coverageComplete;
    obs.requestedFutureAtInspection = scalarWindow.requestedFutureAtInspection;
    obs.stream = scalarWindow.stream;
    obs.mode = config.mode;
    obs.supportBasis = supportBasis;
    obs.note = available ? detection::ScalarInspectionNote::ScalarObserved : detection::ScalarInspectionNote::ScalarUnavailable;
    obs.inspectionNowMs = scalarWindow.inspectionNowMs;
    obs.anchorMs = scalarWindow.requestedStartMs + config.windowPreMs;
    obs.requestedStartMs = scalarWindow.requestedStartMs;
    obs.requestedEndMs = scalarWindow.requestedEndMs;
    obs.availableStartMs = scalarWindow.availableStartMs;
    obs.availableEndMs = scalarWindow.availableEndMs;
    obs.leftMissingMs = scalarWindow.leftMissingMs;
    obs.rightMissingMs = scalarWindow.rightMissingMs;
    obs.coveredDurationMs = scalarWindow.coveredDurationMs;
    obs.windowStartMs = static_cast<int16_t>(-static_cast<int32_t>(config.windowPreMs));
    obs.windowEndMs = static_cast<int16_t>(config.windowPostMs);
    obs.anchor = eventAnchor;
    obs.windowMs = scalarWindow.durationMs;
    obs.valueCount = scalarWindow.valueCount;
    obs.bucketCount = scalarWindow.bucketCount;
    obs.coveredMs = scalarWindow.coveredMs;
    obs.valuesPerBucket = scalarWindow.valuesPerBucket;
    obs.coverageRatio = scalarWindow.coverageRatio;
    obs.internalCoverageKnown = scalarWindow.internalCoverageKnown;
    obs.spanMs = scalarWindow.spanMs;
    obs.latestValueAgeMs = scalarWindow.latestValueAgeMs;
    obs.first = scalarWindow.first;
    obs.last = scalarWindow.last;
    obs.min = scalarWindow.min;
    obs.max = scalarWindow.max;
    obs.peak = peak;
    obs.peakTimeMs = scalarWindow.peakTimeMs;
    obs.rise = scalarWindow.rise;
    obs.mean = mean;
    obs.rms = scalarWindow.rms;
    obs.median = scalarWindow.median;
    obs.p75 = scalarWindow.p75;
    obs.p90 = scalarWindow.p90;
    obs.trimmedMean = scalarWindow.trimmedMean;
    obs.classificationValue = classificationValue;
    obs.sampleCount = sampleCount;
    obs.freshValueCount = scalarWindow.freshValueCount;
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
    const FeatureHistory* featureHistory,
    unsigned long inspectionNowMs
) const {
    out.scalarObservationCount = 0;

    for (size_t i = 0; i < _inspectionPlan.count; ++i) {
        runInspectionModule(out, occurrence, featureHistory, inspectionNowMs, _inspectionPlan.modules[i]);
    }
}

void OccurrenceInspector::annotateScalarFeatureStrength(
    InspectedOccurrence& out,
    const Occurrence& occurrence,
    const FeatureHistory* featureHistory,
    unsigned long inspectionNowMs,
    const ScalarFeatureInspectionConfig& config,
    InspectionTarget target
) const {
    const unsigned long anchorMs = anchorMsForOccurrence(occurrence, config.anchor);
    const unsigned long startMs = anchorMs > config.windowPreMs ? anchorMs - config.windowPreMs : 0UL;
    const unsigned long endMs = anchorMs + config.windowPostMs;
    ScalarInspectionObservation observation = {};
    observation.stream = config.stream;
    observation.mode = config.mode;
    observation.supportBasis = detection::ScalarInspectionBasis::CenteredMagnitudePeak;
    observation.note = detection::ScalarInspectionNote::WindowInvalid;
    observation.inspectionNowMs = inspectionNowMs;
    observation.anchorMs = anchorMs;
    observation.requestedStartMs = startMs;
    observation.requestedEndMs = endMs;
    observation.requestedFutureAtInspection = endMs > inspectionNowMs;
    if (config.enabled && featureHistory != nullptr) {
        const float sustainedThreshold = config.mode == detection::ScalarInspectionMode::SustainedAboveThreshold
            ? config.supportStrength.weakPeakThreshold
            : 0.0f;
        const ScalarWindow scalarWindow = featureHistory->getWindow(config.stream, startMs, endMs, inspectionNowMs, sustainedThreshold);
        if (scalarWindow.valid) {
            const bool usable = scalarWindow.hasValues && scalarWindow.coverageComplete;
            fillScalarObservation(observation, occurrence, scalarWindow, usable, config);
            if (scalarWindow.hasValues && !scalarWindow.coverageComplete) {
                observation.note = scalarWindow.requestedFutureAtInspection
                    ? detection::ScalarInspectionNote::WindowInvalid
                    : detection::ScalarInspectionNote::ScalarUnavailable;
            }
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

    switch (target) {
        case InspectionTarget::Amp:
        out.occurrence.scalar.present = observation.available;
        out.occurrence.scalar.value = observation.available ? observation.classificationValue : 0.0f;
        out.occurrence.scalar.baseline = observation.available ? observation.mean : 0.0f;
        out.occurrence.scalar.lift = observation.available
            ? (out.occurrence.scalar.value - out.occurrence.scalar.baseline)
            : 0.0f;
        out.occurrence.scalar.strength = observation.available ? observation.classificationValue : 0.0f;
        out.occurrence.scalar.strengthClass = observation.strength;
            break;
        case InspectionTarget::TargetScore:
        out.occurrence.frequency.scoreStrength = observation.strength;
            break;
        case InspectionTarget::Contrast:
        out.occurrence.frequency.contrastQuality = observation.strength;
            break;
        case InspectionTarget::TargetBand:
        out.occurrence.frequency.targetBandStrength = observation.strength;
            break;
        case InspectionTarget::None:
        default:
            break;
    }

    out.occurrence.scalar.evidence = observation;
}

void OccurrenceInspector::runInspectionModule(
    InspectedOccurrence& out,
    const Occurrence& occurrence,
    const FeatureHistory* featureHistory,
    unsigned long inspectionNowMs,
    const InspectionModuleConfig& module
) const {
    switch (module.kind) {
    case InspectionModuleKind::ScalarFeatureStrength:
        annotateScalarFeatureStrength(out, occurrence, featureHistory, inspectionNowMs, module.scalar, module.target);
        break;
        case InspectionModuleKind::None:
        default:
            break;
    }
}

InspectedOccurrence OccurrenceInspector::inspectImpl(
    const Occurrence& occurrence,
    const FeatureHistory* featureHistory,
    unsigned long inspectionNowMs
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
        return inspectAcceptedOccurrenceResult(occurrence, featureHistory, inspectionNowMs);
    }

    InspectedOccurrence out;
    out.occurrence = occurrence;
    setRejected(out, OccurrenceRejectReason::UnsupportedKind);
    return out;
}

InspectedOccurrence OccurrenceInspector::inspect(
    const Occurrence& occurrence
) const {
    return inspectImpl(occurrence, nullptr, 0UL);
}

InspectedOccurrence OccurrenceInspector::inspectWithHistory(
    const Occurrence& occurrence,
    const FeatureHistory* featureHistory,
    unsigned long inspectionNowMs
) const {
    return inspectImpl(occurrence, featureHistory, inspectionNowMs);
}

InspectedOccurrence OccurrenceInspector::inspectAcceptedOccurrenceResult(
    const Occurrence& occurrence,
    const FeatureHistory* featureHistory,
    unsigned long inspectionNowMs
) const {
    InspectedOccurrence out;
    out.occurrence = occurrence;
    out.occurrence.confidence = occurrence.confidence > 0.0f ? occurrence.confidence : 1.0f;
    out.decision = OccurrenceDecision::Accepted;
    out.rejectReason = OccurrenceRejectReason::None;
    inspectAcceptedOccurrence(out, occurrence, featureHistory, inspectionNowMs);
    return out;
}

} // namespace detection

