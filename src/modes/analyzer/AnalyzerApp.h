#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../../hal/AudioSourceI2S.h"
#include "../../detection/DetectionProfile.h"
#include "../../io/AudioSignal.h"
#include "../../detection/DetectionRuntime.h"
#include "../../detection/features/FrequencyMatchEvaluation.h"
#include "../../detection/occurrences/InspectedOccurrence.h"
#include "../../detection/features/FreqBandStream.h"
#include "../../detection/patterns/PatternResult.h"
#include "../../detection/occurrences/Occurrence.h"
#include "../../hal/AudioSource.h"
#include "../../RuntimeDefaults.h"
#include "AnalyzerReporting.h"

/*
AnalyzerApp

Analyzer-mode coordinator for the Resonant project.
Wires audio input, DetectionRuntime, diagnostic probes, emitter control,
SEQ trials, RAW capture, and reporting.

Analyzer measures DetectionRuntime output against expected events.
It does not implement detection algorithms, PatternRules, Behavior, or output policy.
*/
class AnalyzerApp {
public:
    using FrequencyFeatureFrame = detection::FrequencyFeatureFrame;
    using PatternCandidate = detection::PatternCandidate;
    using PatternResult = detection::PatternResult;

    enum class SequenceDiagMode {
        Off,
        Miss,
        Trial,
    };

    enum class SeqOutputMode {
        Quiet,
        Compact,
        SignalCheck,
        Full,
        System,
        Source,
        Inspect,
        Pattern,
        Explain,
    };

    enum class SeqOutputWhen {
        Off,
        Miss,
        All,
    };

    struct SeqOutputConfig {
        SeqOutputMode mode = SeqOutputMode::Compact;
        SeqOutputWhen when = SeqOutputWhen::Miss;
        uint8_t verbosity = 0;
        unsigned long totalTrials = 100;
        detection::DetectionProfileKind profileKind = detection::DetectionProfileKind::TonalPulse;
        bool diagnosticsEnabled = true;
        bool frequencyBandEnabled = true;
        unsigned long frequencyComputeDecimation = 4;
    };

    AnalyzerApp(int inputPin = 34);

    void begin();
    void update();
    unsigned long loopDelayMs() const;

private:
    void updateSequenceAudioHealth(const AudioSignalFrame& frame);
    void printSystemHealth(const AnalyzerReport& report) const;
    unsigned long activeRunStartMs() const;
    unsigned long activeRunEndMs() const;
    void resetLoopHealthWindow();

    struct LoopHealthStats {
        uint32_t count = 0;
        uint64_t sumUs = 0;
        uint32_t maxUs = 0;
        uint32_t over5ms = 0;
        uint32_t over20ms = 0;

        void record(uint32_t loopUs) {
            ++count;
            sumUs += loopUs;
            if (loopUs > maxUs) {
                maxUs = loopUs;
            }
            if (loopUs > 5000UL) {
                ++over5ms;
            }
            if (loopUs > 20000UL) {
                ++over20ms;
            }
        }

        void reset() {
            count = 0;
            sumUs = 0;
            maxUs = 0;
            over5ms = 0;
            over20ms = 0;
        }
    };

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
        unsigned long toneHz = runtime::kDefaultChirpFrequencyHz;
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
        static constexpr size_t kMaxTrialCandidates = 12;
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
            uint64_t onsetSample = 0;
            uint64_t peakSample = 0;
            uint64_t releaseSample = 0;
            unsigned long peakMs = 0;
            long endDtMs = -1;
            unsigned long processedAtMs = 0;
            long processLagMs = -1;
            bool transientPresent = false;
            bool freqPresent = false;
            bool freqMatched = false;
            float freqScore = 0.0f;
            float freqContrast = 0.0f;
            bool patternValid = false;
            bool candidateAccepted = false;
            bool patternMatched = false;
            bool supportMatched = false;
            bool behaviorEligible = false;
            bool duplicateCandidate = false;
            const char* candidateClass = "unknown";
            const char* patternType = "none";
            const char* reason = "none";
            const char* rejectReason = "none";
        };

        // Live per-trial diagnostics and rejection bookkeeping.
        struct TrialDiagnostics {
            bool onsetSeen = false;
            bool patternAccepted = false;

            unsigned long firstOnsetMs = 0;
            unsigned long lastOnsetMs = 0;
            unsigned long acceptedPatternMs = 0;
            float acceptedPatternStrength = 0.0f;
            unsigned long acceptedPatternDurationMs = 0;
            float acceptedPatternOnsetStrength = 0.0f;
            float acceptedPatternReleaseStrength = 0.0f;
            float acceptedAmbientBaseline = 0.0f;
            uint64_t acceptedPatternOnsetSample = 0;
            uint64_t acceptedPatternPeakSample = 0;
            uint64_t acceptedPatternReleaseSample = 0;
            unsigned long acceptedPatternPeakMs = 0;
            unsigned long acceptedPatternReleaseMs = 0;
            unsigned long acceptedFrequencyProcessedAtMs = 0;
            detection::PatternResult runtimePatternResult = {};
            detection::FieldState runtimeFieldState = {};
            bool runtimePatternCaptured = false;

            unsigned long audioFrames = 0;
            unsigned long audioZeroishFrames = 0;
            unsigned long audioFlatlineFrames = 0;
            unsigned long audioLargeJumpFrames = 0;
            unsigned long audioRmsTooLowFrames = 0;
            unsigned long audioRmsTooHighFrames = 0;
            unsigned long audioMaxAbsDelta = 0;
            unsigned long audioFlatlineRunFrames = 0;
            long audioLastCenteredSample = 0;
            bool audioHasLastCenteredSample = false;
            uint64_t audioSumSquares = 0;
            float audioRms = 0.0f;
            const char* audioHealth = "unknown";

            uint16_t rawFrames = 0;
            int16_t rawMin = 0;
            int16_t rawMax = 0;
            int32_t rawSum = 0;
            uint32_t rawAbsSum = 0;
            uint16_t rawMaxAbs = 0;

            unsigned long duplicatePatternMs = 0;
            float duplicatePatternStrength = 0.0f;
            unsigned long duplicatePatternDurationMs = 0;
            uint64_t duplicatePatternOnsetSample = 0;
            uint64_t duplicatePatternPeakSample = 0;
            uint64_t duplicatePatternReleaseSample = 0;
            unsigned long duplicatePatternPeakMs = 0;
            unsigned long duplicatePatternReleaseMs = 0;
            unsigned long duplicateFrequencyProcessedAtMs = 0;
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
            bool bestCandidateAccepted = false;
            long bestCandidateDtFromTriggerMs = 0;
            unsigned long bestCandidateDurationMs = 0;
            float bestCandidateStrength = 0.0f;
            CandidateOrigin bestCandidateOrigin = CandidateOrigin::InWindow;

            unsigned long ambientBaselineSamples = 0;
            float ambientBaselineSum = 0.0f;
            float ambientBaselineMin = 0.0f;
            float ambientBaselineMax = 0.0f;
            int maxSignalLevel = 0;
            unsigned long ampPeakMs = 0;

            unsigned long duplicateDts[kMaxDuplicateDts] = {};
            unsigned long duplicateDtCount = 0;

            unsigned long duplicateCount = 0;

            unsigned long onsetRejectBelowThreshold = 0;
            unsigned long onsetRejectPeakActive = 0;
            unsigned long onsetRejectCooldown = 0;
            unsigned long onsetRejectOther = 0;

            unsigned long firstOnsetRejectMs = 0;
            unsigned long lastOnsetRejectMs = 0;

            float lastRejectStrength = 0.0f;
            unsigned long lastRejectDurationMs = 0;
            bool peakActiveAtEnd = false;
            AnalyzerFrequencyDiagnostic frequency = {};
            AnalyzerScalarDiagnostic scalar = {};
        };

        // Sequence-test configuration and execution state.
        bool active = false;
        bool quiet = false;
        bool showDetails = true;
        SequenceDiagMode diagMode = SequenceDiagMode::Off;
        SeqOutputConfig outputConfig = {};
        bool externalEmitter = false;
        detection::DetectionProfileKind profileKind = detection::DetectionProfileKind::TonalPulse;
        bool progressLineStarted = false;
        unsigned long totalTrials = 100;
        unsigned long periodMs = 2500;
        unsigned long windowStartOffsetMs = 0;
        unsigned long windowEndOffsetMs = 2200;
        unsigned long toneHz = runtime::kDefaultChirpFrequencyHz;
        unsigned long durationMs = 100;
        unsigned long startupDelayMs = 1000;
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
        CurveSnapshot sampleHistory[kMaxSampleHistory] = {};
        size_t sampleHistoryStart = 0;
        size_t sampleHistoryCount = 0;
        unsigned long sampleHistoryLastMs = 0;
        bool sampleHistoryHasPending = false;
        CurveSnapshot sampleHistoryPending = {};
        CurveSnapshot sampleRows[kMaxSampleRows] = {};
        size_t sampleRowCount = 0;

        // Trial scheduling and aggregate results.
        unsigned long startedAtMs = 0;
        unsigned long nextTriggerAtMs = 0;
        unsigned long currentTrial = 0;
        unsigned long currentTrialScheduledAtMs = 0;
        unsigned long currentTrialStartMs = 0;
        unsigned long currentTrialEndMs = 0;
        unsigned long currentTrialOnsetDetectedMs = 0;
        unsigned long currentTrialPatternDetectedMs = 0;
        bool primaryValidPatternCaptured = false;
        detection::PatternResult primaryValidPattern = {};
        long primaryValidPatternDtMs = -1;
        unsigned long rejectedInWindowCount = 0;
        detection::PatternResult firstRejectedInWindow = {};
        bool currentTrialFinalized = false;
        unsigned long currentTrialUnexpected = 0;
        unsigned long currentTrialRejected = 0;
        bool trialHadAudioOverflow = false;
        unsigned long trialOverflowCountAtStart = 0;
        unsigned long trialTransientRejectTooShortCountAtStart = 0;
        unsigned long trialTransientRejectTooLongCountAtStart = 0;
        unsigned long trialTransientRejectWeakCountAtStart = 0;
        TrialDiagnostics currentTrialDiagnostics;

        unsigned long hits = 0;
        unsigned long expectedHits = 0;
        unsigned long lateHits = 0;
        unsigned long misses = 0;
        unsigned long unexpected = 0;
        unsigned long duplicates = 0;
        unsigned long invalidAudio = 0;
        unsigned long startupArtifacts = 0;
        unsigned long samplesProcessed = 0;
        unsigned long currentTrialSamplesProcessed = 0;
        unsigned long maxSamplesPerLoop = 0;
        unsigned long emptySourceLoops = 0;
        uint64_t availableBytesSum = 0;
        unsigned long availableBytesSamples = 0;
        unsigned long maxAvailableBytes = 0;
        unsigned long maxBlockAgeMs = 0;
        unsigned long maxUpdateLoopUs = 0;
        uint64_t totalUpdateLoopUs = 0;
        unsigned long updateLoopCount = 0;
        unsigned long currentTrialUpdateLoopMaxUs = 0;
        unsigned long maxSampleWorkUs = 0;
        unsigned long maxFinalizeTrialUs = 0;
        unsigned long maxProcessingLagMs = 0;
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
            unsigned long totalPatternDtMs = 0;
            unsigned long totalPatternDurationMs = 0;
        float totalPatternConfidence = 0.0f;
        unsigned long patternDtCount = 0;
        unsigned long patternDurationCount = 0;
        unsigned long completedTrials = 0;
        unsigned long missReasonCounts[static_cast<size_t>(AnalyzerReason::Unknown) + 1U] = {};
        unsigned long rejectReasonCounts[static_cast<size_t>(AnalyzerReason::Unknown) + 1U] = {};
        unsigned long freqEvidenceClassCounts[5] = {};
        unsigned long currentMissStreak = 0;
            unsigned long longestMissStreak = 0;
            unsigned long firstMissTrial = 0;

            AnalyzerScalarDiagnostic scalar = {};
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
        SequenceDiagMode diagMode = SequenceDiagMode::Off;
        const char* setupLabel = nullptr;
        bool sampleDumpEnabled = false;
        unsigned long sampleDumpFirstTrials = 0;
        unsigned long sampleDumpEveryNth = 0;
        unsigned long sampleDumpLeadMs = 0;
        unsigned long sampleDumpTailMs = 0;
        unsigned long sampleDumpStepMs = 0;
        unsigned long sampleDumpMaxRows = 0;
        unsigned long startupDelayMs = 1000;
        detection::DetectionProfileKind profileKind = detection::DetectionProfileKind::TonalPulse;
        bool externalEmitter = false;
        char setupLabelStorage[96] = {};
    };

    // Setup, control, and detector configuration helpers.
    void configureParameters();
    void configureI2SParameters();
    void beginEmitterControl();
    void processPendingSequenceStart();
    void pollUsbConsole();
    void pollEmitterSerial();
    void handleUsbLine(const char* line);
    void printSequenceHelp();
    void sendEmitterCommand(const char* command);
    void resetAudioSignalState();

    // Session lifecycle helpers.
    void startBaseSession(unsigned long durationMs, bool quiet = false);
    void stopBaseSession();
    void updateBaseSession(unsigned long now);
    void printBaseSummary() const;
    void printBaseHints() const;

    // Sequence-test workflows.
    void startSequenceTest(unsigned long totalTrials, unsigned long periodMs, unsigned long windowEndOffsetMs, unsigned long toneHz, unsigned long durationMs, bool quiet = false, bool showDetails = true, SequenceDiagMode diagMode = SequenceDiagMode::Off, const char* setupLabel = nullptr, bool sampleDumpEnabled = false, unsigned long sampleDumpFirstTrials = 2, unsigned long sampleDumpEveryNth = 0, unsigned long sampleDumpLeadMs = 50, unsigned long sampleDumpTailMs = 800, unsigned long sampleDumpStepMs = 1, unsigned long sampleDumpMaxRows = 5000, unsigned long startupDelayMs = 1000, detection::DetectionProfileKind profileKind = detection::DetectionProfileKind::TonalPulse, bool externalEmitter = false);
    void stopSequenceTest();
    void updateSequenceTest(unsigned long now);
    void finalizeSequenceTrial(unsigned long now);
    void printCaptureSummary() const;
    void startCaptureSession(unsigned long totalTrials, unsigned long periodMs, unsigned long windowEndOffsetMs, unsigned long toneHz, unsigned long durationMs, bool quiet = false);
    void stopCaptureSession();
    void updateCaptureSession(unsigned long now);
    void updateCaptureQuietStats(unsigned long now);
    void updateCaptureTrial(unsigned long now);
    void finalizeCaptureTrial(unsigned long now);
    void runRawTrigger(unsigned long toneHz, unsigned long durationMs, unsigned long postMs, unsigned long preMs, unsigned long decim, bool dumpChunks, bool dumpBinary);
    void runRawBandTrigger(unsigned long toneHz, unsigned long durationMs, unsigned long postMs, unsigned long preMs, unsigned long decim);
    void printAudioSourceSummary() const;
    void printAudioRunSummary() const;
    void printOccurrenceSummary() const;
    void printCaptureHints() const;
    void printDetectionParameters() const;
    void printTransientAcceptedDebug(unsigned long now, float strength, unsigned long durationMs) const;
    void printTransientStatsDebug(unsigned long now) const;
    void printSequenceExplain(const AnalyzerReport& report) const;
    void printSequenceDiagnostics(const AnalyzerReport& report) const;
    void printSequenceScalarDiagnostics(const AnalyzerReport& report) const;
    void printSequenceInspect(const AnalyzerReport& report) const;
    void printSequencePattern(const AnalyzerReport& report) const;
    void printSequenceStatus() const;
    void printSignalCheck() const;
    void printSequenceTrialHeader(unsigned long trialNumber) const;
    void printSequenceCandidateLogs(unsigned long trialNumber, const SequenceTest::TrialDiagnostics& diagnostics) const;
    void printSequenceTrialResult(const AnalyzerReport& report) const;
    void printSequenceFinalOutput() const;
    void printSequenceSummary() const;
    const char* activeAnalyzerProfileName() const;
    AnalyzerReport* sequenceReportScratch();
    void buildSequenceAnalyzerReport(AnalyzerReport& report, unsigned long trialNumber, AnalyzerResult result, long dtMs, long durMs, float strength, bool audioOverflow, unsigned long duplicateCount, const SequenceTest::TrialDiagnostics& diagnostics) const;
    void recordSequenceClassifierOutcome(const PatternResult& patternResult, bool duplicateCandidate, bool unexpectedCandidate);
    void handleSequenceCandidate(const PatternResult& patternResult, const FrequencyFeatureFrame* liveFrequencyFrame = nullptr);
    void updateSequenceAmbientStats(unsigned long nowMs);

    // Sequence sample capture helpers.
    void beginSequenceSampleDump(unsigned long trialNumber);
    void clearSequenceSampleDump();
    void recordSequenceSample(const CurveSnapshot& snapshot);
    void flushSequenceSampleHistory(unsigned long currentSampleMs);
    void printSequenceSampleDump(unsigned long trialNumber) const;
    bool sequenceSampleDumpSelected(unsigned long trialNumber) const;
    unsigned long sequenceSampleDumpEstimatedRows(unsigned long selectedTrials) const;
    static void sequenceCurveSampleCallback(const CurveSnapshot& snapshot, void* context);
    FrequencyFeatureFrame captureFrequencyFeatureFrame(unsigned long observedAtMs) const;
    const char* sequenceTrialClassificationName(const char* result, long dtMs, long durMs, const SequenceTest::TrialDiagnostics& diagnostics) const;

    // Miscellaneous output helpers.
    void printValueFrame(unsigned long now) const;
    void printValueModeBanner() const;
    static const char* sequenceOutputModeName(SeqOutputMode mode);
    static const char* sequenceOutputWhenName(SeqOutputWhen value);
    static bool sequenceOutputModeEnabled(SeqOutputMode configured, SeqOutputMode requested);
    static bool sequenceOutputWhenEnabled(SeqOutputWhen configured, AnalyzerResult result);
    static SeqOutputMode sequenceOutputModeFromToken(const char* token, bool* valid = nullptr);
    static SeqOutputWhen sequenceOutputWhenFromToken(const char* token, bool* valid = nullptr);
    static SequenceDiagMode sequenceDiagModeFromOutputWhen(SeqOutputWhen when);

    // Hardware and occurrence chain.
    int _inputPin;
    AudioSourceI2S _i2sSource;
    AudioSource& _audioSource;
    AudioSignal _audioSignal;
    detection::DetectionRuntime* _detection = nullptr;
    FreqBandStream _freqBandStream;
    FrequencyMatchEvaluation::Values _frequencyEvidenceTuning = {};
    SeqOutputConfig _seqOutputConfig = {};
    PendingSequenceStart _pendingSequenceStart = {};

    // Console and emitter control.
    unsigned long _controlBaudRate = 115200;
    int _controlRxPin = 16;
    int _controlTxPin = 17;
    char _usbLineBuffer[96];
    size_t _usbLineLength = 0;
    char _commandScratch[128];
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
    AnalyzerReport* _sequenceReportScratch = nullptr;

    // Print throttling for the VAL view.
    mutable unsigned long _lastPrintMs = 0;
    uint32_t _loopLastUs = 0;
    unsigned long _loopMaxSinceBootUs = 0;
    mutable LoopHealthStats _loopHealth;
    static constexpr unsigned long kPrintIntervalMs = 100;
};

