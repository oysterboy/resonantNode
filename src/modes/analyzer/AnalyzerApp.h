#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../../hal/AudioSourceI2S.h"
#include "../../detection/detectors/AmpTransientDetector.h"
#include "../../detection/DetectionProfile.h"
#include "../../io/AudioSignal.h"
#include "../../detection/DetectionRuntime.h"
#include "../../detection/signals/FrequencyEvidenceEvaluation.h"
#include "../../detection/signals/InspectedSignal.h"
#include "../../detection/features/FreqBandStream.h"
#include "../../detection/features/FeatureHistory.h"
#include "../../detection/patterns/PatternResult.h"
#include "../../detection/signals/SignalCandidate.h"
#include "../../hal/AudioSource.h"
#include "AnalyzerReporting.h"

/*
AnalyzerApp

Owns the analyzer-mode orchestration layer for the Resonant project.

Responsibilities:
- wire up the chosen audio source and detector chain
- run base calibration, capture, and sequence-test sessions
- manage detector parameters and control-claim handshakes
- collect candidate, frequency, and timing diagnostics
- print runtime summaries and debug output for analyzer workflows

Does NOT:
- implement the detector algorithms themselves
- own the audio signal processing primitives
- make resonance behavior decisions

File structure:
- public mode lifecycle
- session state bundles
- setup and control helpers
- detector parameter helpers
- session/capture/sequence workflows
- reporting and debug output
- runtime state
*/
class AnalyzerApp {
public:
    using FrequencyEvidence = detection::FrequencyEvidence;
    using PatternCandidate = detection::PatternCandidate;
    using PatternResult = detection::PatternResult;

    enum AnalyzerLogFlags : uint32_t {
        ANALYZER_LOG_NONE = 0,
        ANALYZER_LOG_SUMMARY = 1u << 0,
        ANALYZER_LOG_TRIAL = 1u << 1,
        ANALYZER_LOG_CANDIDATE = 1u << 2,
        ANALYZER_LOG_EXPLAIN = 1u << 4,
        ANALYZER_LOG_TRIAL_SUMMARY = 1u << 5,
        ANALYZER_LOG_CUSTOM = 1u << 6,
        ANALYZER_LOG_AMP_WINDOW = ANALYZER_LOG_CUSTOM,
    };

    static constexpr uint32_t DEFAULT_ANALYZER_LOG_FLAGS =
        ANALYZER_LOG_SUMMARY |
        ANALYZER_LOG_TRIAL;

    AnalyzerApp(int inputPin = 34);

    void begin();
    void update();
    unsigned long loopDelayMs() const;

private:
    // Session state bundles, ordered from lighter calibration state to the most specialized sequence-test state.
    struct BaseSession {
        bool active = false;
        bool quiet = false;

        unsigned long durationMs = 10000;
        unsigned long startedAtMs = 0;
        unsigned long lastStatusPrintMs = 0;

        unsigned long ignoredRawSamples = 0;
        unsigned long samples = 0;
        unsigned long rawSum = 0;
        int rawMin = 0;
        int rawMax = 0;

        float deltaSum = 0.0f;
        float deltaMin = 0.0f;
        float deltaMax = 0.0f;
        float baselineSum = 0.0f;
        float baselineMin = 0.0f;
        float baselineMax = 0.0f;
    };

    struct CaptureSession {
        bool active = false;
        bool quiet = false;

        unsigned long totalTrials = 20;
        unsigned long periodMs = 2500;
        unsigned long windowStartOffsetMs = 0;
        unsigned long windowEndOffsetMs = 500;
        unsigned long toneHz = 3200;
        unsigned long durationMs = 100;

        unsigned long startedAtMs = 0;
        unsigned long nextTriggerAtMs = 0;
        unsigned long currentTrial = 0;
        unsigned long currentTrialStartMs = 0;
        unsigned long currentTrialEndMs = 0;
        bool currentTrialFinalized = false;

        int currentRawMin = 0;
        int currentRawMax = 0;
        float currentDeltaMin = 0.0f;
        float currentDeltaMax = 0.0f;

        int quietRawMin = 0;
        int quietRawMax = 0;
        unsigned long quietRawSum = 0;
        unsigned long quietRawSamples = 0;
        float quietDeltaMin = 0.0f;
        float quietDeltaMax = 0.0f;
        float quietDeltaSum = 0.0f;
        unsigned long quietDeltaSamples = 0;

        unsigned long completed = 0;
        unsigned long totalRawSwing = 0;
        float totalDeltaSwing = 0.0f;
        int bestRawSwing = 0;
        float bestDeltaSwing = 0.0f;
        unsigned long lastStatusPrintMs = 0;
    };

    struct SequenceTest {
        static constexpr size_t kMaxTrialCandidates = 16;
        static constexpr size_t kMaxDuplicateDts = 8;
        enum class CandidateOrigin {
            PreWindow,
            InWindow,
            PostWindow,
        };

        // Per-trial candidate snapshots and reports.
        struct CandidateSample {
            unsigned long candidateMs = 0;
            long dtFromTriggerMs = 0;
            long dtFromTrialStartMs = 0;
            unsigned long durationMs = 0;
            float strength = 0.0f;
            CandidateOrigin origin = CandidateOrigin::InWindow;
        };

        // Live per-trial diagnostics and rejection bookkeeping.
        struct TrialDiagnostics {
            bool onsetSeen = false;
            bool transientAccepted = false;

            unsigned long firstOnsetMs = 0;
            unsigned long lastOnsetMs = 0;
            unsigned long acceptedTransientMs = 0;
            float acceptedTransientStrength = 0.0f;
            unsigned long acceptedTransientDurationMs = 0;
            float acceptedTransientOnsetStrength = 0.0f;
            float acceptedTransientReleaseStrength = 0.0f;
            float acceptedAmbientBaseline = 0.0f;
            uint64_t acceptedTransientOnsetSample = 0;
            uint64_t acceptedTransientPeakSample = 0;
            uint64_t acceptedTransientReleaseSample = 0;
            unsigned long acceptedTransientPeakMs = 0;
            unsigned long acceptedTransientReleaseMs = 0;
            FrequencyEvidence acceptedFrequencyEvidence = {};
            FrequencyEvidence acceptedFrequencyEvidenceFull = {};
            unsigned long acceptedFrequencyProcessedAtMs = 0;
            FrequencyEvidence acceptedParityProbe64 = {};
            unsigned long acceptedParityProbe64ProcessedAtMs = 0;
            detection::PatternResult runtimePatternResult = {};
            detection::FieldState runtimeFieldState = {};
            bool runtimePatternCaptured = false;
            unsigned long duplicateTransientMs = 0;
            float duplicateTransientStrength = 0.0f;
            unsigned long duplicateTransientDurationMs = 0;
            uint64_t duplicateTransientOnsetSample = 0;
            uint64_t duplicateTransientPeakSample = 0;
            uint64_t duplicateTransientReleaseSample = 0;
            unsigned long duplicateTransientPeakMs = 0;
            unsigned long duplicateTransientReleaseMs = 0;
            FrequencyEvidence duplicateFrequencyEvidence = {};
            FrequencyEvidence duplicateFrequencyEvidenceFull = {};
            unsigned long duplicateFrequencyProcessedAtMs = 0;
            FrequencyEvidence duplicateParityProbe64 = {};
            unsigned long duplicateParityProbe64ProcessedAtMs = 0;
            long duplicateDeltaFromPrimaryMs = 0;
            bool duplicateOriginWindow = false;
            char duplicateReason[32] = "none";

            unsigned long rawCandidateCount = 0;
            unsigned long candidatePreWindowCount = 0;
            unsigned long candidateInWindowCount = 0;
            unsigned long candidatePostWindowCount = 0;
            CandidateSample candidates[kMaxTrialCandidates] = {};
            unsigned long candidateCount = 0;
            unsigned long candidateOverflowCount = 0;
            unsigned long firstCandidateMs = 0;
            bool bestCandidateValid = false;
            long bestCandidateDtFromTriggerMs = 0;
            unsigned long bestCandidateDurationMs = 0;
            float bestCandidateStrength = 0.0f;
            CandidateOrigin bestCandidateOrigin = CandidateOrigin::InWindow;

            unsigned long ambientBaselineSamples = 0;
            float ambientBaselineSum = 0.0f;
            float ambientBaselineMin = 0.0f;
            float ambientBaselineMax = 0.0f;
            int maxSignalLevel = 0;

            unsigned long transientRejectTooShortCount = 0;
            unsigned long transientRejectTooLongCount = 0;
            unsigned long transientRejectWeakCount = 0;
            AmpTransientDetector::TransientRejectReason strongestRejectReason = AmpTransientDetector::TransientRejectReason::None;
            long strongestRejectDtFromTriggerMs = -1;
            unsigned long strongestRejectDurationMs = 0;
            float strongestRejectStrength = 0.0f;

            unsigned long duplicateDts[kMaxDuplicateDts] = {};
            unsigned long duplicateDtCount = 0;

            unsigned long duplicateCount = 0;

            unsigned long onsetRejectBelowThreshold = 0;
            unsigned long onsetRejectPeakActive = 0;
            unsigned long onsetRejectCooldown = 0;
            unsigned long onsetRejectOther = 0;

            unsigned long firstOnsetRejectMs = 0;
            unsigned long lastOnsetRejectMs = 0;

            AmpTransientDetector::TransientRejectReason lastTransientRejectReason = AmpTransientDetector::TransientRejectReason::None;
            float lastRejectStrength = 0.0f;
            unsigned long lastRejectDurationMs = 0;
            bool peakActiveAtEnd = false;
        };

        // Sequence-test configuration and execution state.
        bool active = false;
        bool quiet = false;
        bool showDetails = true;
        bool externalEmitter = false;
        detection::DetectionProfileKind profileKind = detection::DetectionProfileKind::FreqAmp;
        bool progressLineStarted = false;
        unsigned long totalTrials = 100;
        unsigned long periodMs = 2500;
        unsigned long windowStartOffsetMs = 0;
        unsigned long windowEndOffsetMs = 2200;
        unsigned long toneHz = 3200;
        unsigned long durationMs = 100;
        char setupLabel[48] = TEST_SETUP_LABEL;

        bool sampleDumpEnabled = false;
        unsigned long sampleDumpFirstTrials = 2;
        unsigned long sampleDumpEveryNth = 0;
        unsigned long sampleDumpLeadMs = 50;
        unsigned long sampleDumpTailMs = 800;
        unsigned long sampleDumpStepMs = 1;
        unsigned long sampleDumpMaxRows = 5000;
        bool sampleDumpWarned = false;
        bool sampleDumpSelectedForTrial = false;
        bool sampleDumpCapturing = false;
        unsigned long sampleDumpCurrentTrial = 0;
        unsigned long sampleDumpTriggerMs = 0;
        unsigned long sampleDumpTriggerSampleMs = 0;
        unsigned long sampleDumpCaptureStartMs = 0;
        unsigned long sampleDumpCaptureEndMs = 0;
        unsigned long sampleDumpNextEmitMs = 0;
        static constexpr size_t kMaxSampleHistory = 256;
        static constexpr size_t kMaxSampleRows = 2048;
        static constexpr size_t kMaxTrialReports = 128;
        CurveSnapshot sampleHistory[kMaxSampleHistory] = {};
        size_t sampleHistoryStart = 0;
        size_t sampleHistoryCount = 0;
        unsigned long sampleHistoryLastMs = 0;
        bool sampleHistoryHasPending = false;
        CurveSnapshot sampleHistoryPending = {};
        CurveSnapshot sampleRows[kMaxSampleRows] = {};
        size_t sampleRowCount = 0;
        AnalyzerReport* trialReports = nullptr;
        mutable size_t trialReportCapacity = 0;
        mutable size_t trialReportCount = 0;

        // Trial scheduling and aggregate results.
        unsigned long startedAtMs = 0;
        unsigned long nextTriggerAtMs = 0;
        unsigned long currentTrial = 0;
        unsigned long currentTrialScheduledAtMs = 0;
        unsigned long currentTrialStartMs = 0;
        unsigned long currentTrialEndMs = 0;
        unsigned long currentTrialOnsetDetectedMs = 0;
        unsigned long currentTrialTransientDetectedMs = 0;
        bool currentTrialHit = false;
        bool currentTrialFinalized = false;
        unsigned long currentTrialUnexpected = 0;
        bool trialHadAudioOverflow = false;
        unsigned long trialOverflowCountAtStart = 0;
        unsigned long trialTransientRejectTooShortCountAtStart = 0;
        unsigned long trialTransientRejectTooLongCountAtStart = 0;
        unsigned long trialTransientRejectWeakCountAtStart = 0;
        TrialDiagnostics currentTrialDiagnostics;
        uint32_t logFlags = DEFAULT_ANALYZER_LOG_FLAGS;

        unsigned long hits = 0;
        unsigned long expectedHits = 0;
        unsigned long lateHits = 0;
        unsigned long misses = 0;
        unsigned long unexpected = 0;
        unsigned long duplicates = 0;
        unsigned long invalidAudio = 0;
        unsigned long samplesProcessed = 0;
        unsigned long maxSamplesPerLoop = 0;
        unsigned long emptySourceLoops = 0;
        unsigned long totalHitStrengthScaled = 0;
        unsigned long totalHitDurationMs = 0;
        unsigned long patternMatchedExpected = 0;
        unsigned long patternUnmatchedExpected = 0;
        unsigned long patternMatchedDuplicates = 0;
        unsigned long patternUnmatchedDuplicates = 0;
        unsigned long patternMatchedUnexpected = 0;
        unsigned long patternUnmatchedUnexpected = 0;
        unsigned long freqRejectScore = 0;
        unsigned long freqRejectContrast = 0;
        unsigned long freqRejectBoth = 0;
        unsigned long freqRejectNoEvidence = 0;
        unsigned long freqRejectInvalidWindow = 0;

    };

    struct PendingSequenceStart {
        bool active = false;
        unsigned long totalTrials = 0;
        unsigned long periodMs = 0;
        unsigned long windowEndOffsetMs = 0;
        unsigned long toneHz = 0;
        unsigned long durationMs = 0;
        bool quiet = false;
        bool showDetails = false;
        const char* setupLabel = nullptr;
        uint32_t logFlags = 0;
        bool sampleDumpEnabled = false;
        unsigned long sampleDumpFirstTrials = 0;
        unsigned long sampleDumpEveryNth = 0;
        unsigned long sampleDumpLeadMs = 0;
        unsigned long sampleDumpTailMs = 0;
        unsigned long sampleDumpStepMs = 0;
        unsigned long sampleDumpMaxRows = 0;
        detection::DetectionProfileKind profileKind = detection::DetectionProfileKind::FreqAmp;
        bool externalEmitter = false;
        char setupLabelStorage[96] = {};
    };

    // Setup, control, and detector configuration helpers.
    void configureParameters();
    void configureSharedParameters();
    void configureI2SParameters();
    void beginEmitterControl();
    void processPendingSequenceStart();
    void pollUsbConsole();
    void pollEmitterSerial();
    void handleUsbLine(const char* line);
    void sendEmitterCommand(const char* command);
    void resetDetectorState();

    // Detector state inspection and tuning helpers.
    bool detectorOnsetDetected() const;
    float detectorOnsetStrength() const;
    bool detectorTransientDetected() const;
    float detectorTransientStrength() const;
    unsigned long detectorTransientDurationMs() const;
    bool detectorTransientPeakActive() const;
    const char* detectorOnsetRejectReasonName() const;
    const char* detectorTransientRejectReasonName() const;
    unsigned long detectorTransientRejectedDurationMs() const;
    float detectorTransientRejectedStrength() const;
    float detectorOnsetDetectionThreshold() const;
    float detectorOnsetReleaseThreshold() const;
    unsigned long detectorCooldownAfterOnsetMs() const;
    unsigned long detectorMinTransientDurationMs() const;
    unsigned long detectorMaxTransientDurationMs() const;
    float detectorMinTransientPeakStrength() const;
    unsigned long detectorReleaseDebounceMs() const;
    void setDetectorOnsetDetectionThreshold(float value);
    void setDetectorOnsetReleaseThreshold(float value);
    void setDetectorCooldownAfterOnsetMs(unsigned long value);
    void setDetectorMinTransientDurationMs(unsigned long value);
    void setDetectorMaxTransientDurationMs(unsigned long value);
    void setDetectorMinTransientPeakStrength(float value);
    void setDetectorReleaseDebounceMs(unsigned long value);

    // Session lifecycle helpers.
    void startBaseSession(unsigned long durationMs, bool quiet = false);
    void stopBaseSession();
    void updateBaseSession(unsigned long now);
    void printBaseSummary() const;
    void printBaseHints() const;

    // Sequence-test workflows.
    void startSequenceTest(unsigned long totalTrials, unsigned long periodMs, unsigned long windowEndOffsetMs, unsigned long toneHz, unsigned long durationMs, bool quiet = false, bool showDetails = true, const char* setupLabel = nullptr, uint32_t logFlags = DEFAULT_ANALYZER_LOG_FLAGS, bool sampleDumpEnabled = false, unsigned long sampleDumpFirstTrials = 2, unsigned long sampleDumpEveryNth = 0, unsigned long sampleDumpLeadMs = 50, unsigned long sampleDumpTailMs = 800, unsigned long sampleDumpStepMs = 1, unsigned long sampleDumpMaxRows = 5000, detection::DetectionProfileKind profileKind = detection::DetectionProfileKind::FreqAmp, bool externalEmitter = false);
    void stopSequenceTest();
    void updateSequenceTest(unsigned long now);
    void handleSequenceTransient(unsigned long now);
    void finalizeSequenceTrial(unsigned long now);
    void printCaptureSummary() const;
    void startCaptureSession(unsigned long totalTrials, unsigned long periodMs, unsigned long windowEndOffsetMs, unsigned long toneHz, unsigned long durationMs, bool quiet = false);
    void stopCaptureSession();
    void updateCaptureSession(unsigned long now);
    void updateCaptureQuietStats(unsigned long now);
    void updateCaptureTrial(unsigned long now);
    void finalizeCaptureTrial(unsigned long now);
    void runRawTrigger(unsigned long toneHz, unsigned long durationMs, unsigned long postMs, unsigned long preMs, unsigned long decim, bool dumpChunks, bool dumpBinary);
    void printAudioSourceSummary() const;
    void printSignalSummary() const;
    void printCaptureHints() const;
    void printDetectionParameters() const;
    void printTransientAcceptedDebug(unsigned long now, float strength, unsigned long durationMs) const;
    void printTransientStatsDebug(unsigned long now) const;
    void printSequenceExplain(const AnalyzerReport& report) const;
    void printSequenceAmpWindow(const AnalyzerReport& report) const;
    void printSequenceTrialResult(unsigned long trialNumber, const char* result, long dtMs, long durMs, float strength, bool audioOverflow, unsigned long duplicateCount, const SequenceTest::TrialDiagnostics& diagnostics) const;
    void printSequenceTrialResult(const AnalyzerReport& report) const;
    void printSequenceFinalOutput() const;
    void printSequenceSummary() const;
    const char* activeAnalyzerProfileName() const;
    AnalyzerReport buildSequenceAnalyzerReport(unsigned long trialNumber, const char* result, long dtMs, long durMs, float strength, bool audioOverflow, unsigned long duplicateCount, const SequenceTest::TrialDiagnostics& diagnostics) const;
    void recordSequenceClassifierOutcome(const PatternResult& patternResult, bool duplicateCandidate, bool unexpectedCandidate);
    void handleSequenceCandidate(const PatternResult& patternResult, unsigned long queueDepthBeforeDrain, const FrequencyEvidence* liveFrequencyEvidence = nullptr);
    FrequencyEvidence scanSequenceFrequencyParity64(const PatternCandidate& patternCandidate, unsigned long observedAtMs) const;
    void updateSequenceAmbientStats();

    // Sequence sample capture helpers.
    void beginSequenceSampleDump(unsigned long trialNumber);
    void clearSequenceSampleDump();
    void recordSequenceSample(const CurveSnapshot& snapshot);
    void flushSequenceSampleHistory(unsigned long currentSampleMs);
    void printSequenceSampleDump(unsigned long trialNumber) const;
    bool sequenceSampleDumpSelected(unsigned long trialNumber) const;
    unsigned long sequenceSampleDumpEstimatedRows(unsigned long selectedTrials) const;
    static void sequenceCurveSampleCallback(const CurveSnapshot& snapshot, void* context);
    FrequencyEvidence captureFrequencyEvidence() const;
    void noteSequenceTransientReject(unsigned long eventMs);
    void noteSequenceTransientRejectReason(unsigned long eventMs, const char* reasonName, unsigned long durationMs, float strength);
    const char* sequenceTrialClassificationName(const char* result, long dtMs, long durMs, const SequenceTest::TrialDiagnostics& diagnostics) const;

    // Miscellaneous output helpers.
    void printValueFrame(unsigned long now) const;
    void printValueModeBanner() const;

    // Hardware and signal chain.
    int _inputPin;
    AudioSourceI2S _i2sSource;
    AudioSource& _audioSource;
    AudioSignal _audioSignal;
    AmpTransientDetector _audioOnsetDetector;
    detection::DetectionRuntime* _detection = nullptr;
    FreqBandStream _freqBandStream;
    detection::FeatureHistory* _sequenceFeatureHistory = nullptr;
    FrequencyEvidenceEvaluation::Values _frequencyEvidenceTuning = {};
    PendingSequenceStart _pendingSequenceStart = {};

    // Console and emitter control.
    unsigned long _controlBaudRate = 115200;
    int _controlRxPin = 16;
    int _controlTxPin = 17;
    char _usbLineBuffer[96];
    size_t _usbLineLength = 0;
    char _emitterLineBuffer[96];
    size_t _emitterLineLength = 0;

    // Value mode and detector latching.
    bool _valMode = false;
    mutable unsigned long _valOnsetLatchedUntilMs = 0;
    mutable unsigned long _valTransientLatchedUntilMs = 0;
    bool _controlClaimPending = false;
    bool _controlClaimSent = false;
    unsigned long _controlClaimAtMs = 0;

    // Session state.
    BaseSession _baseSession;
    SequenceTest _sequenceTest;
    CaptureSession _captureSession;
    unsigned long _rawCaptureSequenceId = 0;

    // Print throttling for the VAL view.
    mutable unsigned long _lastPrintMs = 0;
    static constexpr unsigned long kPrintIntervalMs = 100;
};
