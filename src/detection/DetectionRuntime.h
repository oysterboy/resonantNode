#pragma once

#include <stddef.h>

#include "../app/RingQueue.h"
#include "../audio/AudioSignal.h"
#include "DetectionDiagnostics.h"
#include "DetectionProfile.h"
#include "detectors/scalar/ScalarTransientDetector.h"
#include "evaluation/OccurrenceVerdict.h"
#include "features/FeatureExtractor.h"
#include "features/FeatureHistory.h"
#include "features/BinTrack.h"
#include "field/FieldState.h"
#include "field/FieldStateTracker.h"
#include "inspection/InspectorTypes.h"
#include "inspection/OccurrenceInspector.h"
#include "inspection/VerdictProjection.h"
#include "occurrences/Occurrence.h"
#include "inspection/InspectionFacts.h"

namespace detection {

/*
DetectionRuntime

Owns the active detection pipeline wiring:
feature observation, occurrence emission, occurrence inspection (which ends
in the OccurrenceVerdict, makeVerdict()), field-state tracking, and
OccurrenceVerdict queueing.

Consumes AudioSamplePacket and FrequencyBandMeasurementPacket.
Produces OccurrenceVerdict and FieldState.
Does not decide behavior or output.
Feature producers fan out fresh samples to FeatureHistory and the selected
detector path in parallel; FeatureHistory is for retrospective inspection, not
a live pipe into occurrence emission.

What the Analyzer reads beyond that (counters, pipeline events, the detector
report capture) lives in DetectionDiagnostics, reached through
diagnostics(); the Node build holds the empty variant (DetectionDiagnostics.h).
*/
class DetectionRuntime {
public:
    DetectionRuntime();

    void resetState();
    void resetDetectors();
    void resetDetectionState();
    // The caller skipped input (own-emit suppression on the Node): the detector restarts clean (see
    // ScalarTransientDetector::restartAfterInputGap). Occurrences already closed are still inspected.
    void restartAfterInputGap();

    void setScalarTransientConfig(const ScalarTransientConfig& config);
    void setDetectorSelection(DetectorSelection selection);
    void setInspectionPlan(const InspectionPlan& plan);
    void setFieldStateConfig(const FieldStateConfig& config);
    void setVerdictQueueEnabled(bool enabled);

    void observeFrame(const AudioSamplePacket& audioSamplePacket,
                      const FrequencyBandMeasurementPacket& frequencyEvidence, unsigned long nowMs);

    bool popOccurrenceVerdict(OccurrenceVerdict& out);
    const FieldState& fieldState() const;
    // Verdicts dropped because the 4-slot queue was full (counted in every build; the Node prints it in
    // RB STATUS). Reset with the queues.
    unsigned long verdictQueueOverflowCount() const;
    // Occurrences inspected early because the 2-slot wait queue was full
    // (0 on the shipped values).
    unsigned long pendingInspectionOverflowCount() const;
    // The detector's last refused candidate (a fact of the detector, passed through unchanged).
    ScalarTransientDetector::LastTransientReject lastDetectorReject() const {
        return _scalarDetector.lastTransientReject();
    }
    // What the latest inspection measured, per module (InspectionFacts.h); its occurrenceId ties it to a verdict.
    const InspectionFacts& lastInspectionFacts() const { return _lastInspectionFacts; }
    // The loudest detector input since the last call, then reset: the Node's RB level line, so a call that opened
    // no candidate still shows how loud it arrived against the onset threshold.
    float takeInputPeak() {
        const float peak = _inputPeak;
        _inputPeak = 0.0f;
        return peak;
    }
    const ScalarTransientConfig& scalarTransientConfig() const { return _scalarTransientConfig; }

    // The Analyzer's side (SEQ trials): the detector's per-trial summaries and
    // its EVT stats line, the diagnostics counters. Harmless in the Node build
    // (the Null variant records nothing).
    void resetSourceRejectSummaries();
    void resetDiagnosticsCounters();
    ActiveDetectionDiagnostics& diagnostics() { return _diagnostics; }
    const ActiveDetectionDiagnostics& diagnostics() const { return _diagnostics; }

private:
    static constexpr size_t kResultQueueCapacity = 4;
    static constexpr size_t kPendingInspectionCapacity = 2;

    // Occurrences waiting for their inspection window to complete, with what the diagnostics keep for their
    // event.
    struct PendingInspection {
        Occurrence occurrence = {};
        unsigned long readyAtMs = 0;
        ActiveDetectionDiagnostics::ReportSlot report = {};
    };

    void drainDetectors(unsigned long nowMs);
    void inspectAndRender(const Occurrence& occurrence, const ActiveDetectionDiagnostics::ReportSlot& report,
                          unsigned long nowMs);
    bool pushOccurrenceVerdict(const OccurrenceVerdict& result);
    bool hasPendingDetectorOutput() const;
    void resetDetectionQueues();

    ScalarTransientConfig _scalarTransientConfig = {};
    // Profile configuration applied at fixed runtime stages.
    InspectionPlan _inspectionPlan = {};

    ScalarTransientDetector _scalarDetector;
    OccurrenceInspector _occurrenceInspector;
    FieldStateTracker _fieldStateTracker;
    FeatureHistory _featureHistory;
    BinTrack _binTrack;  // the strongest band bin per ms, for the ShapeInspector (DET-015)
    // The previous inspected occurrence's release: the shape look-back's floor (code-review-2026-10-30 I-M-A).
    bool _haveLastInspectedRelease = false;
    uint32_t _lastInspectedReleaseMs = 0;
    ActiveDetectionDiagnostics _diagnostics;
    InspectionFacts _lastInspectionFacts = {};
    float _inputPeak = 0.0f;

    RingQueue<OccurrenceVerdict, kResultQueueCapacity> _verdictQueue;
    bool _verdictQueueEnabled = true;
    unsigned long _verdictQueueOverflowCount = 0;

    RingQueue<PendingInspection, kPendingInspectionCapacity> _pendingInspections;
    unsigned long _pendingInspectionOverflowCount = 0;
};

} // namespace detection
