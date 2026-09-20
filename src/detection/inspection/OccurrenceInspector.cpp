#include "OccurrenceInspector.h"
// OccurrenceInspector evidence annotation and inspection in source order.
namespace {

unsigned long anchorMsForOccurrence(const detection::Occurrence& occurrence, detection::MagnitudeInspectionAnchor anchor);

detection::MagnitudeInspectionAnchor resolvedAnchorForOccurrence(
    const detection::Occurrence& occurrence,
    detection::MagnitudeInspectionAnchor anchor
) {
    switch (anchor) {
        case detection::MagnitudeInspectionAnchor::Start:
            return occurrence.startMs != 0 ? detection::MagnitudeInspectionAnchor::Start
                : (occurrence.peakMs != 0 ? detection::MagnitudeInspectionAnchor::Peak
                    : (occurrence.releaseMs != 0 ? detection::MagnitudeInspectionAnchor::Release : detection::MagnitudeInspectionAnchor::Fallback));
        case detection::MagnitudeInspectionAnchor::Release:
            return occurrence.releaseMs != 0 ? detection::MagnitudeInspectionAnchor::Release
                : (occurrence.peakMs != 0 ? detection::MagnitudeInspectionAnchor::Peak
                    : (occurrence.startMs != 0 ? detection::MagnitudeInspectionAnchor::Start : detection::MagnitudeInspectionAnchor::Fallback));
        case detection::MagnitudeInspectionAnchor::Peak:
        case detection::MagnitudeInspectionAnchor::Fallback:
        case detection::MagnitudeInspectionAnchor::None:
        default:
            return occurrence.peakMs != 0 ? detection::MagnitudeInspectionAnchor::Peak
                : (occurrence.startMs != 0 ? detection::MagnitudeInspectionAnchor::Start
                    : (occurrence.releaseMs != 0 ? detection::MagnitudeInspectionAnchor::Release : detection::MagnitudeInspectionAnchor::Fallback));
    }
}

unsigned long anchorMsForOccurrence(const detection::Occurrence& occurrence, detection::MagnitudeInspectionAnchor anchor) {
    switch (resolvedAnchorForOccurrence(occurrence, anchor)) {
        case detection::MagnitudeInspectionAnchor::Start:
            return occurrence.startMs;
        case detection::MagnitudeInspectionAnchor::Release:
            return occurrence.releaseMs;
        case detection::MagnitudeInspectionAnchor::Peak:
            return occurrence.peakMs;
        case detection::MagnitudeInspectionAnchor::Fallback:
        case detection::MagnitudeInspectionAnchor::None:
        default:
            return occurrence.peakMs != 0 ? occurrence.peakMs : (occurrence.startMs != 0 ? occurrence.startMs : occurrence.releaseMs);
    }
}

void setRejected(detection::InspectedOccurrence& out, detection::OccurrenceRejectReason reason) {
    out.decision = detection::OccurrenceDecision::Rejected;
    out.rejectReason = reason;
}

void fillMagnitudeObservation(
    detection::MagnitudeInspectionObservation& obs,
    const detection::Occurrence& occurrence,
    const detection::MagnitudeWindow& magnitudeWindow,
    bool available,
    const detection::MagnitudeFeatureInspectionConfig& config
) {
    const float peak = magnitudeWindow.peak;
    const float mean = magnitudeWindow.mean;
    const size_t sampleCount = magnitudeWindow.sampleCount;
    const size_t sustainedCount = magnitudeWindow.sustainedCount;
    const unsigned long sustainedMs = magnitudeWindow.sustainedMs;
    const float sustainedThreshold = magnitudeWindow.sustainedThreshold;
    float classificationValue = peak;
    detection::MagnitudeInspectionBasis supportBasis = detection::MagnitudeInspectionBasis::PeakAbsolute;
    bool classified = available;
    detection::StrengthClass strength = detection::StrengthClass::Unknown;

    switch (config.mode) {
        case detection::MagnitudeInspectionMode::PeakAbsolute:
            classificationValue = peak;
            supportBasis = detection::MagnitudeInspectionBasis::PeakAbsolute;
            strength = available ? classifySupportStrength(classificationValue, available, config.supportStrength) : detection::StrengthClass::Unknown;
            break;
        case detection::MagnitudeInspectionMode::MeanAbsolute:
            classificationValue = mean;
            supportBasis = detection::MagnitudeInspectionBasis::MeanAbsolute;
            strength = available ? classifySupportStrength(classificationValue, available, config.supportStrength) : detection::StrengthClass::Unknown;
            break;
        case detection::MagnitudeInspectionMode::SustainedAboveThreshold:
            classificationValue = peak;
            supportBasis = detection::MagnitudeInspectionBasis::SustainedAboveThreshold;
            {
                const size_t requiredSustainedCount = config.minSustainedCount > 0
                    ? config.minSustainedCount
                    : static_cast<size_t>(config.minSustainedMs > 0 ? config.minSustainedMs : 1U);
                const bool sustainedEnough = sustainedCount >= requiredSustainedCount;
                classified = available && sustainedEnough;
                strength = classified ? classifySupportStrength(classificationValue, available, config.supportStrength) : detection::StrengthClass::Unknown;
            }
            break;
        case detection::MagnitudeInspectionMode::PeakCentered:
            classificationValue = mean;
            supportBasis = detection::MagnitudeInspectionBasis::PeakCenteredMean;
            strength = available ? classifySupportStrength(classificationValue, available, config.supportStrength) : detection::StrengthClass::Unknown;
            break;
        case detection::MagnitudeInspectionMode::PeakCenteredLift:
            classificationValue = peak - mean;
            supportBasis = detection::MagnitudeInspectionBasis::PeakCenteredLift;
            strength = available ? classifySupportStrength(classificationValue, available, config.supportStrength) : detection::StrengthClass::Unknown;
            break;
        case detection::MagnitudeInspectionMode::Rms:
            classificationValue = magnitudeWindow.rms;
            supportBasis = detection::MagnitudeInspectionBasis::Rms;
            strength = available ? classifySupportStrength(classificationValue, available, config.supportStrength) : detection::StrengthClass::Unknown;
            break;
        case detection::MagnitudeInspectionMode::P75:
            classificationValue = magnitudeWindow.p75;
            supportBasis = detection::MagnitudeInspectionBasis::P75;
            strength = available ? classifySupportStrength(classificationValue, available, config.supportStrength) : detection::StrengthClass::Unknown;
            break;
    }

    const detection::MagnitudeInspectionAnchor eventAnchor = resolvedAnchorForOccurrence(occurrence, config.anchor);
    obs.available = available;
    obs.hasValues = magnitudeWindow.hasValues;
    obs.coverageComplete = magnitudeWindow.coverageComplete;
    obs.requestedFutureAtInspection = magnitudeWindow.requestedFutureAtInspection;
    obs.stream = magnitudeWindow.stream;
    obs.mode = config.mode;
    obs.supportBasis = supportBasis;
    obs.note = available ? detection::MagnitudeInspectionNote::MagnitudeObserved : detection::MagnitudeInspectionNote::MagnitudeUnavailable;
    obs.inspectionNowMs = magnitudeWindow.inspectionNowMs;
    obs.anchorMs = magnitudeWindow.requestedStartMs + config.windowPreMs;
    obs.requestedStartMs = magnitudeWindow.requestedStartMs;
    obs.requestedEndMs = magnitudeWindow.requestedEndMs;
    obs.availableStartMs = magnitudeWindow.availableStartMs;
    obs.availableEndMs = magnitudeWindow.availableEndMs;
    obs.leftMissingMs = magnitudeWindow.leftMissingMs;
    obs.rightMissingMs = magnitudeWindow.rightMissingMs;
    obs.coveredDurationMs = magnitudeWindow.coveredDurationMs;
    obs.windowStartMs = static_cast<int16_t>(-static_cast<int32_t>(config.windowPreMs));
    obs.windowEndMs = static_cast<int16_t>(config.windowPostMs);
    obs.anchor = eventAnchor;
    obs.windowMs = magnitudeWindow.durationMs;
    obs.valueCount = magnitudeWindow.valueCount;
    obs.bucketCount = magnitudeWindow.bucketCount;
    obs.coveredMs = magnitudeWindow.coveredMs;
    obs.valuesPerBucket = magnitudeWindow.valuesPerBucket;
    obs.coverageRatio = magnitudeWindow.coverageRatio;
    obs.internalCoverageKnown = magnitudeWindow.internalCoverageKnown;
    obs.spanMs = magnitudeWindow.spanMs;
    obs.latestValueAgeMs = magnitudeWindow.latestValueAgeMs;
    obs.first = magnitudeWindow.first;
    obs.last = magnitudeWindow.last;
    obs.min = magnitudeWindow.min;
    obs.max = magnitudeWindow.max;
    obs.peak = peak;
    obs.peakTimeMs = magnitudeWindow.peakTimeMs;
    obs.rise = magnitudeWindow.rise;
    obs.mean = mean;
    obs.rms = magnitudeWindow.rms;
    obs.median = magnitudeWindow.median;
    obs.p75 = magnitudeWindow.p75;
    obs.p90 = magnitudeWindow.p90;
    obs.trimmedMean = magnitudeWindow.trimmedMean;
    obs.classificationValue = classificationValue;
    obs.sampleCount = sampleCount;
    obs.freshValueCount = magnitudeWindow.freshValueCount;
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
    out.magnitudeObservationCount = 0;

    for (size_t i = 0; i < _inspectionPlan.count; ++i) {
        runInspectionModule(out, occurrence, featureHistory, inspectionNowMs, _inspectionPlan.modules[i]);
    }
}

void OccurrenceInspector::annotateMagnitudeFeatureStrength(
    InspectedOccurrence& out,
    const Occurrence& occurrence,
    const FeatureHistory* featureHistory,
    unsigned long inspectionNowMs,
    const MagnitudeFeatureInspectionConfig& config,
    InspectionTarget target
) const {
    const unsigned long anchorMs = anchorMsForOccurrence(occurrence, config.anchor);
    const unsigned long startMs = anchorMs > config.windowPreMs ? anchorMs - config.windowPreMs : 0UL;
    const unsigned long endMs = anchorMs + config.windowPostMs;
    MagnitudeInspectionObservation observation = {};
    observation.stream = config.stream;
    observation.mode = config.mode;
    observation.supportBasis = detection::MagnitudeInspectionBasis::CenteredMagnitudePeak;
    observation.note = detection::MagnitudeInspectionNote::WindowInvalid;
    observation.inspectionNowMs = inspectionNowMs;
    observation.anchorMs = anchorMs;
    observation.requestedStartMs = startMs;
    observation.requestedEndMs = endMs;
    observation.requestedFutureAtInspection = endMs > inspectionNowMs;
    if (config.enabled && featureHistory != nullptr) {
        const float sustainedThreshold = config.mode == detection::MagnitudeInspectionMode::SustainedAboveThreshold
            ? config.supportStrength.weakPeakThreshold
            : 0.0f;
        const MagnitudeWindow magnitudeWindow = featureHistory->getWindow(config.stream, startMs, endMs, inspectionNowMs, sustainedThreshold);
        if (magnitudeWindow.valid) {
            const bool usable = magnitudeWindow.valid;
            fillMagnitudeObservation(observation, occurrence, magnitudeWindow, usable, config);
        } else {
            if (magnitudeWindow.requestedFutureAtInspection) {
                observation.note = detection::MagnitudeInspectionNote::FutureWindowUnavailable;
            } else if (!magnitudeWindow.hasValues) {
                observation.note = detection::MagnitudeInspectionNote::MagnitudeUnavailable;
            } else if (!magnitudeWindow.coverageComplete) {
                observation.note = detection::MagnitudeInspectionNote::HistoryWindowIncomplete;
            } else {
                observation.note = detection::MagnitudeInspectionNote::MagnitudeUnavailable;
            }
        }
    } else if (!config.enabled) {
        observation.note = detection::MagnitudeInspectionNote::InspectionDisabled;
    } else if (featureHistory == nullptr) {
        observation.note = detection::MagnitudeInspectionNote::MissingFeatureHistory;
    }

    if (out.magnitudeObservationCount < kMaxInspectionModules) {
        out.magnitudeObservations[out.magnitudeObservationCount++] = observation;
    }

    switch (target) {
        case InspectionTarget::Amp:
        out.occurrence.magnitude.present = observation.available;
        out.occurrence.magnitude.value = observation.available ? observation.classificationValue : 0.0f;
        out.occurrence.magnitude.baseline = observation.available ? observation.mean : 0.0f;
        out.occurrence.magnitude.lift = observation.available
            ? (out.occurrence.magnitude.value - out.occurrence.magnitude.baseline)
            : 0.0f;
        out.occurrence.magnitude.strength = observation.available ? observation.classificationValue : 0.0f;
        out.occurrence.magnitude.strengthClass = observation.strength;
            break;
        case InspectionTarget::TargetScore:
        out.occurrence.band.scoreStrength = observation.strength;
            break;
        case InspectionTarget::Contrast:
        out.occurrence.band.contrastQuality = observation.strength;
            break;
        case InspectionTarget::TargetBand:
        out.occurrence.band.targetBandStrength = observation.strength;
            break;
        case InspectionTarget::None:
        default:
            break;
    }

    out.occurrence.magnitude.evidence = observation;
}

void OccurrenceInspector::runInspectionModule(
    InspectedOccurrence& out,
    const Occurrence& occurrence,
    const FeatureHistory* featureHistory,
    unsigned long inspectionNowMs,
    const InspectionModuleConfig& module
) const {
    switch (module.kind) {
    case InspectionModuleKind::MagnitudeFeatureStrength:
        annotateMagnitudeFeatureStrength(out, occurrence, featureHistory, inspectionNowMs, module.magnitude, module.target);
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

