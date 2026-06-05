#pragma once

#include <stddef.h>

#include "../io/AudioSignal.h"
#include "DetectionProfile.h"
#include "occurrences/FrequencyOccurrenceSource.h"
#include "occurrences/ScalarOccurrenceSource.h"
#include "inspector/OccurrenceInspector.h"
#include "inspector/InspectorTypes.h"
#include "patterns/PatternAssembler.h"
#include "patterns/PatternRules.h"
#include "patterns/PatternResult.h"
#include "occurrences/Occurrence.h"
#include "occurrences/InspectedOccurrence.h"
#include "field/FieldStateTracker.h"
#include "field/FieldState.h"
#include "features/FeatureExtractor.h"
#include "features/FeatureHistory.h"
#include "features/FrequencyMatchEvaluation.h"

namespace detection {

/*
DetectionRuntime

Owns the active detection pipeline wiring:
feature observation, occurrence emission, occurrence inspection, pattern assembly,
pattern rules, field-state tracking, and PatternResult queueing.

Consumes AudioSamplePacket and FrequencyBandMeasurementPacket.
Produces PatternResult and FieldState.
Does not decide behavior or output.
Feature producers fan out fresh samples to FeatureHistory and the selected
OccurrenceSource in parallel; FeatureHistory is for retrospective inspection,
not a live pipe into occurrence emission.
*/
struct DetectionPipelineResult {
    bool hasPattern = false;
    PatternResult pattern = {};

    bool hasOccurrence = false;
    Occurrence occurrence = {};

    InspectedOccurrence inspectedOccurrence = {};

    bool hasField = false;
    FieldState field = {};

    const char* profileName = "unknown";
    unsigned long timestampMs = 0;
};

struct SourceCandidateSummary {
    bool present = false;
    unsigned long candidateCount = 0;
    unsigned long rejectCount = 0;
    unsigned long bestDurationMs = 0;
    unsigned long secondBestDurationMs = 0;
    unsigned long bestOpenMs = 0;
    unsigned long bestPeakMs = 0;
    unsigned long bestLastMatchMs = 0;
    unsigned long bestCloseMs = 0;
    float bestPeakPrimary = 0.0f;
    float bestPeakSecondary = 0.0f;
    const char* bestRejectReason = "none";
    const char* bestGateReason = "none";
    const char* closeCause = "none";
    unsigned long scoreTooLowFrames = 0;
    unsigned long contrastTooLowFrames = 0;
    unsigned long scoreAndContrastTooLowFrames = 0;
    float maxPeakPrimary = 0.0f;
    unsigned long maxPeakPrimaryMs = 0;
    float maxPeakSecondary = 0.0f;
    unsigned long maxPeakSecondaryMs = 0;
    unsigned long totalMatchMs = 0;
    unsigned long totalGapMs = 0;
    unsigned long maxGapMs = 0;
    unsigned long islandCount = 0;
};

struct SourceCandidateSnapshot {
    bool present = false;
    unsigned long peakMs = 0;
    unsigned long durationMs = 0;
    unsigned long sampleCount = 0;
    float peakPrimary = 0.0f;
    float peakSecondary = 0.0f;
    const char* reason = "none";
    const char* gateReason = "none";
    const char* scope = "unknown";
};

struct DetectionDiagnostics {
    unsigned long observedAtMs = 0;

    const char* occurrenceSource = "unknown";
    const char* detectorKind = "unknown";

    bool acceptedPresent = false;
    unsigned long acceptedStartMs = 0;
    unsigned long acceptedPeakMs = 0;
    unsigned long acceptedReleaseMs = 0;
    unsigned long acceptedDurationMs = 0;
    float acceptedStrength = 0.0f;
    float acceptedScore = 0.0f;
    float acceptedContrast = 0.0f;

    unsigned long frequencyFrames = 0;
    unsigned long frequencyValidFrames = 0;
    unsigned long frequencyScoreOkFrames = 0;
    unsigned long frequencyContrastOkFrames = 0;
    unsigned long frequencyBothOkFrames = 0;
    unsigned long frequencyMatchFrames = 0;
    unsigned long frequencyRejectFrames = 0;
    unsigned long frequencyReleaseScoreOkFrames = 0;
    unsigned long frequencyReleaseContrastOkFrames = 0;
    unsigned long frequencyReleaseBothOkFrames = 0;
    unsigned long frequencyReleaseScoreTooLowFrames = 0;
    unsigned long frequencyReleaseContrastTooLowFrames = 0;
    unsigned long frequencyReleaseScoreAndContrastTooLowFrames = 0;
    unsigned long frequencyReleaseNoEvidenceFrames = 0;
    unsigned long frequencyDiagLongestMatchStreakFrames = 0;
    unsigned long frequencyDiagLongestMatchStreakStartMs = 0;
    unsigned long frequencyDiagLongestMatchStreakEndMs = 0;

    float frequencyScoreMean = 0.0f;
    float frequencyContrastMean = 0.0f;
    float frequencyScoreMin = 0.0f;
    float frequencyContrastMin = 0.0f;
    float frequencyScoreMax = 0.0f;
    float frequencyContrastMax = 0.0f;
    unsigned long frequencyScoreMaxMs = 0;
    unsigned long frequencyContrastMaxMs = 0;
    float frequencyPeakScore = 0.0f;
    float frequencyPeakContrast = 0.0f;
    unsigned long frequencyPeakSampleCount = 0;
    SourceCandidateSummary sourceSummary = {};
    SourceCandidateSnapshot sourceLastCandidate = {};
    float frequencyScoreThreshold = 0.0f;
    float frequencyContrastThreshold = 0.0f;

    bool frequencyNearMiss = false;
    const char* frequencyNearMissReason = "none";

    bool frequencyPresent = false;
    bool frequencyValidWindow = false;
    bool frequencyMatched = false;
    bool frequencyScoreOk = false;
    bool frequencyContrastOk = false;
    const char* frequencyRejectReason = "none";
    const char* frequencyNoEmitReason = "none";
    const char* frequencyGateReason = "none";
    const char* frequencyWouldCandidateReason = "none";
    const char* frequencyCandidateState = "none";
    bool frequencyReadyOk = false;
    bool frequencyGateOpen = false;
    bool frequencyOpened = false;
    bool frequencyReleased = false;
    bool frequencyEmitted = false;
    bool frequencyValidRelease = false;
    bool frequencyEmitAllowed = false;
    unsigned long frequencyOpenMs = 0;
    unsigned long frequencyPeakMs = 0;
    unsigned long frequencyReleaseMs = 0;
    unsigned long frequencyDurationMs = 0;
    unsigned long frequencyMinDurationMs = 0;
    unsigned long frequencyMaxDurationMs = 0;

    const char* scalarRejectReason = "none";
    const char* scalarNoEmitReason = "none";
    const char* scalarGateReason = "none";
    bool scalarOpened = false;
    bool scalarReleased = false;
    bool scalarValidRelease = false;
    bool scalarEmitAllowed = false;
    unsigned long scalarOpenMs = 0;
    unsigned long scalarPeakMs = 0;
    unsigned long scalarReleaseMs = 0;
    unsigned long scalarDurationMs = 0;
    unsigned long scalarMinDurationMs = 0;
    unsigned long scalarMaxDurationMs = 0;
    float scalarPeakStrength = 0.0f;

    const char* scalarOnsetRejectReason = "none";
    const char* scalarTransientRejectReason = "none";
    unsigned long scalarTransientRejectedDurationMs = 0;
    float scalarTransientRejectedStrength = 0.0f;

    float ampCenteredMagnitude = 0.0f;
    float ampLevel = 0.0f;
    float ampBaseline = 0.0f;
    float ampLift = 0.0f;
};

class DetectionRuntime {
public:
    DetectionRuntime();

    void resetState();
    void resetDiagnostics();
    void resetDiagnosticsCounters();
    void resetOccurrenceSources();
    void resetSourceRejectSummaries();
    void resetDetectionState();
    void setDiagnosticsEnabled(bool enabled);
    void captureDiagnostics();

    void setFrequencyMatchConfig(const FrequencyMatchConfig& config);
    void setScalarTransientConfig(const ScalarTransientConfig& config);
    void setOccurrenceSource(OccurrenceSourceKind kind);
    void setInspectionPlan(const InspectionPlan& plan);
    void setPatternRulesConfig(const PatternRulesConfig& config);
    void setFieldStateConfig(const FieldStateConfig& config);
    void setProfileName(const char* profileName);

    void observeFrame(
        const AudioSamplePacket& audioSamplePacket,
        const FrequencyBandMeasurementPacket& frequencyEvidence,
        unsigned long nowMs
    );

    bool popPatternResult(PatternResult& out);
    bool hasLatestPipelineResult() const;
    const DetectionPipelineResult& latestPipelineResult() const;
    const DetectionDiagnostics& diagnostics() const;
    const FrequencyOccurrenceSource& frequencyEmitter() const;
    const FieldState& fieldState() const;
    const FeatureHistory& featureHistory() const;

private:
    static constexpr size_t kResultQueueCapacity = 8;

    // Pipeline stages in execution order.
    void drainOccurrenceSources(unsigned long nowMs);
    void drainPatternAssembler(unsigned long nowMs);
    bool pushPatternResult(const PatternResult& result);
    void capturePipelineResult(
        const PatternResult& result,
        const Occurrence* occurrence,
        const InspectedOccurrence* inspectedOccurrence,
        unsigned long nowMs
    );

    FrequencyMatchConfig _frequencyMatchConfig = {};
    ScalarTransientConfig _scalarTransientConfig = {};
    // Profile configuration applied at fixed runtime stages.
    OccurrenceSourceKind _occurrenceSourceKind = OccurrenceSourceKind::FrequencyMatch;
    InspectionPlan _inspectionPlan = {};
    PatternRulesConfig _patternRulesConfig = {};
    const char* _profileName = "unknown";

    FrequencyOccurrenceSource _frequencyEmitter;
    ScalarOccurrenceSource _scalarEmitter;
    OccurrenceInspector _occurrenceInspector;
    PatternAssembler _patternAssembler;
    PatternRules _patternRules;
    FieldStateTracker _fieldStateTracker;
    FeatureHistory _featureHistory;

    PatternResult _resultQueue[kResultQueueCapacity] = {};
    size_t _resultReadIndex = 0;
    size_t _resultCount = 0;

    DetectionPipelineResult _latestPipelineResult = {};
    bool _hasLatestPipelineResult = false;
    DetectionDiagnostics _diagnostics = {};
    bool _diagnosticsEnabled = true;
    Occurrence _lastOccurrence = {};
    InspectedOccurrence _lastInspectedOccurrence = {};
};

} // namespace detection


