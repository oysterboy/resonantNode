#pragma once

#include <stddef.h>

// Keep the canonical detector-report contract compiled through the active
// runtime header chain. DetectionRuntime snapshots scalar and frequency
// detector reports for analyzer/report consumers.
#include "DetectorReport.h"
#include "../io/AudioSignal.h"
#include "DetectionProfile.h"
#include "detectors/FrequencyMatchDetector.h"
#include "detectors/ScalarTransientDetector.h"
#include "inspector/OccurrenceInspector.h"
#include "inspector/InspectorTypes.h"
#include "patterns/PatternMatcher.h"
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
feature observation, occurrence emission, occurrence inspection, pattern
matching, field-state tracking, and PatternResult queueing.

Consumes AudioSamplePacket and FrequencyBandMeasurementPacket.
Produces PatternResult and FieldState.
Does not decide behavior or output.
Feature producers fan out fresh samples to FeatureHistory and the selected
detector path in parallel; FeatureHistory is for retrospective inspection, not
a live pipe into occurrence emission.
*/
struct DetectionPipelineResult {
    bool hasPattern = false;
    PatternResult pattern = {};
    bool hasPatternReport = false;
    PatternMatcherReport patternReport = {};

    bool hasOccurrence = false;
    Occurrence occurrence = {};

    InspectedOccurrence inspectedOccurrence = {};

    bool hasField = false;
    FieldState field = {};

    const char* profileName = "unknown";
    unsigned long timestampMs = 0;
};

class DetectionRuntime {
public:
    DetectionRuntime();

    void resetState();
    void resetDiagnostics();
    void resetDiagnosticsCounters();
    void resetDetectors();
    void resetSourceRejectSummaries();
    void resetDetectionState();
    void setDiagnosticsEnabled(bool enabled);

    void setFrequencyMatchConfig(const FrequencyMatchConfig& config);
    void setScalarTransientConfig(const ScalarTransientConfig& config);
    void setDetectorSelection(DetectorSelection selection);
    void setInspectionPlan(const InspectionPlan& plan);
    void setPatternMatcherConfig(const PatternMatcherConfig& config);
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
    // Generic report access is the canonical upward path.
    const DetectorReport& activeDetectorReport() const;
    const PatternMatcherReport& activePatternMatcherReport() const;
    const FieldState& fieldState() const;
    const FeatureHistory& featureHistory() const;

private:
    static constexpr size_t kResultQueueCapacity = 4;

    // Pipeline stages in execution order.
    void drainDetectors(unsigned long nowMs);
    void drainPatternMatcher(unsigned long nowMs);
    bool pushPatternResult(const PatternResult& result);
    void capturePipelineResult(
        const PatternResult& result,
        const InspectedOccurrence* inspectedOccurrence,
        unsigned long nowMs
    );
    void refreshDetectorReports(unsigned long nowMs);

    FrequencyMatchConfig _frequencyMatchConfig = {};
    ScalarTransientConfig _scalarTransientConfig = {};
    // Profile configuration applied at fixed runtime stages.
    DetectorSelection _detectorSelection = DetectorSelection::FrequencyMatch;
    InspectionPlan _inspectionPlan = {};
    PatternMatcherConfig _patternMatcherConfig = {};
    const char* _profileName = "unknown";

    FrequencyMatchDetector _frequencyDetector;
    ScalarTransientDetector _scalarDetector;
    OccurrenceInspector _occurrenceInspector;
    PatternMatcher _patternMatcher;
    FieldStateTracker _fieldStateTracker;
    FeatureHistory _featureHistory;

    PatternResult _resultQueue[kResultQueueCapacity] = {};
    size_t _resultReadIndex = 0;
    size_t _resultCount = 0;

    DetectionPipelineResult _latestPipelineResult = {};
    bool _hasLatestPipelineResult = false;
    DetectorReport _detectorReport = {};
    InspectedOccurrence _lastInspectedOccurrence = {};
};

} // namespace detection


