#include "DetectionRuntime.h"
#include "../app/TimingUtils.h"

#include <Arduino.h>

// DetectionRuntime pipeline execution in source order.
namespace detection {

DetectionRuntime::DetectionRuntime() : _diagnostics(_scalarDetector, _featureHistory) {}

namespace {

float selectedScalarValue(const AudioSamplePacket& audioSamplePacket, const FrequencyBandMeasurementPacket& frequencyEvidence, FeatureStreamId stream) {
    switch (stream) {
        case FeatureStreamId::AmpMagnitude:
            return static_cast<float>(audioSamplePacket.audioMagnitudeValue);
        case FeatureStreamId::AmpEnvelope:
            return static_cast<float>(audioSamplePacket.smoothedLevel);
        case FeatureStreamId::FrequencyTarget:
            return frequencyEvidence.targetBandValue;
        case FeatureStreamId::FrequencyContrast:
            return frequencyEvidence.targetBandContrastValue;
        case FeatureStreamId::Unknown:
        default:
            return static_cast<float>(audioSamplePacket.level);
    }
}

} // namespace

// --- reset ------------------------------------------------------------------

void DetectionRuntime::resetState() {
    resetDetectionState();
    resetDiagnosticsCounters();
}

void DetectionRuntime::resetDetectors() {
    _scalarDetector.resetState();
    _pendingInspections.clear();
}

void DetectionRuntime::resetDetectionState() {
    resetDetectors();
    _fieldStateTracker.reset();
    _featureHistory.reset();
    _binTrack.reset();
    resetDetectionQueues();
    _diagnostics.resetBookkeeping();
}

void DetectionRuntime::resetDetectionQueues() {
    _verdictQueue.clear();
    _verdictQueueOverflowCount = 0;
}

void DetectionRuntime::resetDiagnosticsCounters() {
    _verdictQueueOverflowCount = 0;
    _diagnostics.resetCounters();
}

void DetectionRuntime::resetSourceRejectSummaries() {
    _scalarDetector.resetAcceptedOccurrenceSummary();
    _scalarDetector.resetSelectedRejectSummary();
}

void DetectionRuntime::setDiagnosticsEnabled(bool enabled) {
    _scalarDetector.setDiagnosticsEnabled(enabled);
}

// --- configuration ----------------------------------------------------------

void DetectionRuntime::setScalarTransientConfig(const ScalarTransientConfig& config) {
    _scalarTransientConfig = config;
    _scalarDetector.setConfig(_scalarTransientConfig);
}

void DetectionRuntime::setDetectorSelection(DetectorSelection selection) {
    _detectorSelection = selection;
    resetDetectors();
    _scalarDetector.setConfig(_scalarTransientConfig);
    _diagnostics.resetBookkeeping();
}

// FeatureHistory keeps one buffer per inspection module rather than one per
// known stream; the plan is the only reader of history, so it is also the
// authority on which streams are worth recording. The header can't depend
// on InspectorTypes.h, so the bound is enforced here, where both are visible.
static_assert(FeatureHistory::kMaxActiveStreams >= kMaxInspectionModules,
    "FeatureHistory must have a slot for every stream an InspectionPlan can name");

void DetectionRuntime::setInspectionPlan(const InspectionPlan& plan) {
    _inspectionPlan = plan;
    _occurrenceInspector.configure(_inspectionPlan);

    FeatureStreamId streams[kMaxInspectionModules] = {};
    size_t streamCount = 0;
    for (size_t i = 0; i < _inspectionPlan.count && i < kMaxInspectionModules; ++i) {
        const InspectionModuleConfig& module = _inspectionPlan.modules[i];
        if (module.kind == InspectionModuleKind::MagnitudeFeatureStrength) {
            streams[streamCount++] = module.magnitude.stream;
        }
    }
    // Rebinding the streams clears every buffer. Only do it when the set of
    // streams changes: a threshold-only plan change (a live PARAM SET, step 10
    // item 7) keeps the history, so inspections stay complete.
    bool sameStreams = _featureHistory.activeStreamCount() == streamCount;
    for (size_t i = 0; sameStreams && i < streamCount; ++i) {
        sameStreams = _featureHistory.activeStream(i) == streams[i];
    }
    if (!sameStreams) {
        _featureHistory.setActiveStreams(streams, streamCount);
    }
}

void DetectionRuntime::setFieldStateConfig(const FieldStateConfig& config) {
    _fieldStateTracker.setConfig(config);
}

void DetectionRuntime::setVerdictQueueEnabled(bool enabled) {
    _verdictQueueEnabled = enabled;
    if (!enabled) {
        _verdictQueue.clear();
    }
}

// --- the frame --------------------------------------------------------------

void IRAM_ATTR DetectionRuntime::observeFrame(
    const AudioSamplePacket& audioSamplePacket,
    const FrequencyBandMeasurementPacket& frequencyEvidence,
    unsigned long nowMs
) {
    _diagnostics.onObserveFrame();
    _fieldStateTracker.update(nowMs);
    if (!audioSamplePacket.valid) {
        return;
    }

    FeatureExtractor::observeFrame(audioSamplePacket, _featureHistory);
    FeatureExtractor::observeFrequencyMeasurementPacket(frequencyEvidence, nowMs, _featureHistory);
    if (frequencyEvidence.present) {
        _binTrack.note(nowMs, frequencyEvidence.peakBinHz, frequencyEvidence.targetBandValue);
    }

    bool detectorInputProcessed = false;

    if (streamRequiresFreshFrequency(_scalarTransientConfig.observedStream) && !frequencyEvidence.fresh) {
        _diagnostics.onNoFreshFrequencySkip();
    } else {
        _scalarDetector.update(
            audioSamplePacket,
            selectedScalarValue(audioSamplePacket, frequencyEvidence, _scalarTransientConfig.observedStream)
        );
        detectorInputProcessed = true;
        _diagnostics.onFreshDetectorInput();
    }

    if (detectorInputProcessed || hasPendingDetectorOutput() || !_pendingInspections.empty()) {
        _diagnostics.onDrain();
        drainDetectors(nowMs);
        _diagnostics.afterDrain();
    }
}

bool DetectionRuntime::popOccurrenceVerdict(OccurrenceVerdict& out) {
    return _verdictQueue.pop(out);
}

unsigned long DetectionRuntime::verdictQueueOverflowCount() const {
    return _verdictQueueOverflowCount;
}

unsigned long DetectionRuntime::pendingInspectionOverflowCount() const {
    return _pendingInspectionOverflowCount;
}

const FieldState& DetectionRuntime::fieldState() const {
    return _fieldStateTracker.state();
}

bool IRAM_ATTR DetectionRuntime::hasPendingDetectorOutput() const {
    return _scalarDetector.hasPendingOccurrence();
}

void DetectionRuntime::drainDetectors(unsigned long nowMs) {
    Occurrence occurrence;
    while (_scalarDetector.popOccurrence(occurrence)) {
        _diagnostics.onOccurrencePopped(occurrence);
        _fieldStateTracker.observeOccurrence(occurrence, nowMs);

        // Step 8b item 1 (decisions/2026-10-22-inspect-when-window-complete.md):
        // an occurrence whose inspection window ends after now waits until it
        // has; one that is ready goes straight through, unless an earlier one is
        // still waiting (order is kept).
        const unsigned long readyAtMs = _occurrenceInspector.windowCompleteAtMs(occurrence);
        const bool ready = !timing::after(readyAtMs, nowMs);
        if (ready && _pendingInspections.empty()) {
            ActiveDetectionDiagnostics::ReportSlot report;
            _diagnostics.captureReport(report);
            inspectAndRender(occurrence, report, nowMs);
            continue;
        }
        if (_pendingInspections.full()) {
            // Cannot happen on the shipped values (one occurrence per >= 85 ms,
            // a wait of <= 15 ms); if it does, inspect the oldest now, early.
            ++_pendingInspectionOverflowCount;
            PendingInspection& oldest = _pendingInspections.front();
            inspectAndRender(oldest.occurrence, oldest.report, nowMs);
            _pendingInspections.popFront();
        }
        PendingInspection& slot = *_pendingInspections.emplace();
        slot.occurrence = occurrence;
        slot.readyAtMs = readyAtMs;
        _diagnostics.captureReport(slot.report);
    }

    while (!_pendingInspections.empty()) {
        PendingInspection& next = _pendingInspections.front();
        if (timing::after(next.readyAtMs, nowMs)) {
            break;
        }
        inspectAndRender(next.occurrence, next.report, nowMs);
        _pendingInspections.popFront();
    }
}

void DetectionRuntime::inspectAndRender(const Occurrence& occurrence, const ActiveDetectionDiagnostics::ReportSlot& report,
                                        unsigned long nowMs) {
    const InspectedOccurrence inspected = _occurrenceInspector.inspectWithHistory(occurrence, &_featureHistory, nowMs);
    _fieldStateTracker.observeInspectedOccurrence(inspected, nowMs);
    _diagnostics.onInspected(inspected);

    // The verdict is the last step of inspection (makeVerdict); FieldState,
    // the verdict queue and the Analyzer's pipeline event all take it here,
    // with the inspected occurrence and the detector report in scope.
    OccurrenceVerdict verdict;
    if (!makeVerdict(inspected, _inspectionPlan, verdict)) {
        return;
    }
    _fieldStateTracker.observeOccurrenceVerdict(verdict, nowMs);
    _diagnostics.onVerdict(verdict, inspected, report, nowMs);
    if (_verdictQueueEnabled) {
        pushOccurrenceVerdict(verdict);
    }
}

bool DetectionRuntime::pushOccurrenceVerdict(const OccurrenceVerdict& result) {
    if (!_verdictQueueEnabled) {
        return true;
    }
    if (!_verdictQueue.push(result)) {
        ++_verdictQueueOverflowCount;
        return false;
    }
    return true;
}

} // namespace detection
