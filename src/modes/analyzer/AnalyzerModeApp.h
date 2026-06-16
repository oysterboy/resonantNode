#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../../audio/AudioSourceI2S.h"
#include "../../detection/DetectionProfile.h"
#include "../../audio/AudioSignal.h"
#include "../../detection/DetectionRuntime.h"
#include "../../detection/detectors/frequency/FrequencyMatchCriteria.h"
#include "../../detection/occurrences/InspectedOccurrence.h"
#include "../../detection/features/FreqBandStream.h"
#include "../../detection/patterns/PatternResult.h"
#include "../../detection/occurrences/Occurrence.h"
#include "../../audio/AudioSource.h"
#include "../../app/RuntimeDefaults.h"
#include "../../detection/analyzer/AnalyzerReportTypes.h"

struct DetectorInputSample {
    // Bounded detector-input trace used only by the neutral SAMPLES dump.
    unsigned long sampleMs = 0;
    float value = 0.0f;
};

/*
AnalyzerApp

Analyzer-mode coordinator for the Resonant project.
Wires audio input, DetectionRuntime, diagnostic probes, emitter control,
SEQ trials, RAW capture, and reporting.

Analyzer measures DetectionRuntime output against expected events.
 It does not implement detection algorithms, pattern matching, Behavior, or output policy.
*/
class AnalyzerApp {
public:
    using PatternResult = detection::PatternResult;

    enum class SequenceDiagMode {
        Off,
        Miss,
        Trial,
    };

    enum class SeqOutputMode {
        Quiet,
        Trial,
        System,
        Source,
        Inspect,
        Detail,
    };

    enum class SeqOutputWhen {
        Off,
        Miss,
        All,
    };

    struct SeqOutputConfig {
        SeqOutputMode mode = SeqOutputMode::Trial;
        SeqOutputWhen when = SeqOutputWhen::Miss;
        uint8_t verbosity = 0;
        unsigned long totalTrials = 100;
        detection::DetectionProfileKind profileKind = detection::DetectionProfileKind::TonalPulseScalar;
        bool diagnosticsEnabled = true;
        bool frequencyBandEnabled = true;
        unsigned long frequencyUpdateEverySamples = 4;
    };

    //PARAM TUNING TEMPORARY
    struct AnalyzerTuning {
        FrequencyMatchCriteria::Values frequencyMatch = {};
        detection::ScalarTransientConfig scalarTransient = {};
    };

    AnalyzerApp(int inputPin = 34);

    void begin();
    void update();
    unsigned long loopDelayMs() const;
    static constexpr size_t debugSequenceTestSize();
    static constexpr size_t debugSequenceTestSampleHistoryCapacity();
    static constexpr size_t debugSequenceTestSampleRowsCapacity();

private:
    void updateSequenceAudioHealth(const AudioSamplePacket& audioSamplePacket);
    void printSystemHealth(const AnalyzerReport& report) const;
    uint32_t analyzerBootCount() const;
    const char* currentResetReasonName() const;
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

    struct SequenceTest {
        static constexpr size_t kMaxTrialPending = 5;
        static constexpr size_t kMaxDuplicateDts = 8;
        enum class PendingOrigin {
            PreWindow,
            InWindow,
            PostWindow,
        };

        // Per-trial pending snapshots and reports.
        struct PendingSample {
            unsigned long pendingMs = 0;
            long dtFromTriggerMs = 0;
            long dtFromTrialStartMs = 0;
            unsigned long durationMs = 0;
            float strength = 0.0f;
            PendingOrigin origin = PendingOrigin::InWindow;
            unsigned long peakMs = 0;
            long endDtMs = -1;
            bool patternValid = false;
            bool patternAccepted = false;
            bool patternMatched = false;
            bool supportMatched = false;
            bool behaviorEligible = false;
            bool duplicatePending = false;
            uint8_t pendingClass = 0;
            detection::PatternType patternType = detection::PatternType::None;
            detection::PatternReasonCode reasonCode = detection::PatternReasonCode::None;
            detection::PatternRejectReason rejectReasonCode = detection::PatternRejectReason::None;
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
            unsigned long acceptedPatternPeakMs = 0;
            unsigned long acceptedPatternReleaseMs = 0;
            bool runtimePatternCaptured = false;

            bool emitSeen = false;
            bool emitStartSeen = false;
            bool emitDoneSeen = false;
            bool emitDriveSeen = false;
            unsigned long emitStartDtMs = 0;
            unsigned long emitDoneDtMs = 0;
            unsigned long emitDurationMs = 0;
            unsigned long emitTrialId = 0;
            unsigned long emitStartTrialId = 0;
            unsigned long emitDoneTrialId = 0;

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
            uint8_t rawZeroCrossings = 0;
            uint8_t rawSameValueCount = 0;
            uint8_t rawSameValueRun = 0;
            uint8_t rawSameValueMaxRun = 0;
            int16_t rawLastSample = 0;
            uint8_t rawBlockHashRepeatCount = 0;
            uint8_t rawLastBlockHash = 0;

            unsigned long duplicatePatternMs = 0;
            float duplicatePatternStrength = 0.0f;
            unsigned long duplicatePatternDurationMs = 0;
            unsigned long duplicatePatternPeakMs = 0;
            unsigned long duplicatePatternReleaseMs = 0;
            long duplicateDeltaFromPrimaryMs = 0;
            bool duplicateOriginWindow = false;
            char duplicateReason[32] = "none";

            unsigned long rawPendingCount = 0;
            unsigned long pendingPreWindowCount = 0;
            unsigned long pendingInWindowCount = 0;
            unsigned long pendingPostWindowCount = 0;
            PendingSample pendingSamples[kMaxTrialPending] = {};
            unsigned long pendingCount = 0;
            unsigned long pendingOverflowCount = 0;
            unsigned long firstPendingMs = 0;
            bool bestPendingAccepted = false;
            long bestPendingDtFromTriggerMs = 0;
            unsigned long bestPendingDurationMs = 0;
            float bestPendingStrength = 0.0f;
            PendingOrigin bestPendingOrigin = PendingOrigin::InWindow;

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
        };

        // Sequence-test configuration and execution state.
        bool active = false;
        bool quiet = false;
        bool showDetails = true;
        SequenceDiagMode diagMode = SequenceDiagMode::Off;
        SeqOutputConfig outputConfig = {};
        bool externalEmitter = false;
        detection::DetectionProfileKind profileKind = detection::DetectionProfileKind::TonalPulseScalar;
        bool progressLineStarted = false;
        unsigned long totalTrials = 100;
        unsigned long periodMs = 2500;
        unsigned long windowStartOffsetMs = 0;
        unsigned long windowEndOffsetMs = 2200;
        unsigned long toneHz = runtime::kDefaultChirpFrequencyHz;
        unsigned long durationMs = 100;
        unsigned long startupDelayMs = 1000;
        unsigned long reportSettleMs = 500;
        char setupLabel[48] = TEST_SETUP_LABEL;

        // Sample-dump tooling state, separate from clean analyzer truth.
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
        detection::DetectorSelection sampleDumpDetectorSelection = detection::DetectorSelection::FrequencyMatch;
        detection::FeatureStreamId sampleDumpObservedStream = detection::FeatureStreamId::Unknown;
        static constexpr size_t kMaxSampleHistory = 128;
        static constexpr size_t kMaxSampleRows = 256;
        DetectorInputSample sampleHistory[kMaxSampleHistory] = {};
        size_t sampleHistoryStart = 0;
        size_t sampleHistoryCount = 0;
        unsigned long sampleHistoryLastMs = 0;
        bool sampleHistoryHasPending = false;
        DetectorInputSample sampleHistoryPending = {};
        DetectorInputSample sampleRows[kMaxSampleRows] = {};
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
        detection::InspectedOccurrence primaryValidInspectedOccurrence = {};
        long primaryValidPatternDtMs = -1;
        bool primaryAcceptedOccurrenceCaptured = false;
        detection::InspectedOccurrence primaryAcceptedInspectedOccurrence = {};
        long primaryAcceptedOccurrenceDtMs = -1;
        unsigned long rejectedInWindowCount = 0;
        bool bestRejectedPatternCaptured = false;
        detection::PatternResult bestRejectedInWindow = {};
        detection::InspectedOccurrence bestRejectedInspectedOccurrence = {};
        bool currentTrialFinalized = false;
        unsigned long currentTrialUnexpected = 0;
        unsigned long currentTrialRejected = 0;
        bool bufferOverrun = false;
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
        unsigned long completedTrials = 0;
        AnalyzerCleanSummary cleanSummary = {};

        };

    struct PendingSequenceStart {
        bool active = false;
        unsigned long totalTrials = 0;
        unsigned long periodMs = 0;
        unsigned long windowEndOffsetMs = 0;
        unsigned long toneHz = 0;
        unsigned long durationMs = 0;
        unsigned long reportSettleMs = 500;
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
        detection::DetectionProfileKind profileKind = detection::DetectionProfileKind::TonalPulseScalar;
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

    // Sequence-test workflows.
    void startSequenceTest(const PendingSequenceStart& pending);
    void stopSequenceTest();
    void updateSequenceTest(unsigned long now);
    void finalizeSequenceTrial(unsigned long now);
    void runRawTrigger(unsigned long toneHz, unsigned long durationMs, unsigned long postMs, unsigned long preMs, unsigned long decim, bool dumpChunks, bool dumpBinary, bool dumpCsv);
    void runRawBandTrigger(unsigned long toneHz, unsigned long durationMs, unsigned long postMs, unsigned long preMs, unsigned long decim);
    void printAudioSourceSummary() const;
    void printAudioRunSummary() const;
    void printOccurrenceSummary() const;
    void printDetectionParameters() const;
    void printSequenceDetailCanonical(const AnalyzerReport& report) const;
    void printSequenceInspectCanonical(const AnalyzerReport& report) const;
    void printSequenceSourceCanonical(const AnalyzerReport& report) const;
    void printSequenceSourceCoreCanonical(const AnalyzerReport& report) const;
    void printSequenceSourceSpecCanonical(const AnalyzerReport& report) const;
    void printSequenceStatus() const;
    void printSequenceTrialHeader(unsigned long trialNumber) const;
    void printSequenceTrial(const AnalyzerReport& report) const;
    void printSequenceSummaryClean() const;
    void printSequenceReport() const;
    const char* activeAnalyzerProfileName() const;
    void printParamStatus() const;
    detection::DetectionProfile effectiveSequenceProfile() const;
    AnalyzerReport* sequenceReportScratch();
    void buildSequenceAnalyzerReport(AnalyzerReport& report, unsigned long trialNumber, AnalyzerResult result, long dtMs, long durMs, float strength, bool bufferOverrun, unsigned long duplicateCount, const SequenceTest::TrialDiagnostics& diagnostics) const;
    struct SequenceTrialSelection {
        enum class Kind {
            None,
            ValidPattern,
            AcceptedOccurrence,
            RejectedPattern,
            Unexpected,
            Miss,
        };

        Kind kind = Kind::None;
        const detection::PatternResult* patternResult = nullptr;
        const detection::InspectedOccurrence* inspectedOccurrence = nullptr;
        AnalyzerResult result = AnalyzerResult::Unknown;
        long dtMs = -1;
        unsigned long durationMs = 0;
        float strength = 0.0f;
    };

    unsigned long sequenceTrialOnsetAnchorMs() const;
    SequenceTrialSelection selectSequenceTrialSelection(unsigned long trialOnsetAnchorMs) const;
    void handleSequencePending(
        const PatternResult& patternResult,
        const detection::InspectedOccurrence* selectedInspectedOccurrence = nullptr,
        const detection::FrequencyBandMeasurementPacket* liveFrequencyMeasurementPacket = nullptr
    );
    void updateSequenceAmbientStats(unsigned long nowMs);
    void updateCleanSequenceSummary(const AnalyzerReport& report);

    // Sequence sample capture helpers.
    void beginSequenceSampleDump(unsigned long trialNumber);
    void clearSequenceSampleDump();
    void recordSequenceDetectorInputSample(unsigned long sampleMs, float value);
    void flushSequenceSampleHistory(unsigned long currentSampleMs);
    void printSequenceSampleReport(unsigned long trialNumber) const;
    bool sequenceSampleDumpSelected(unsigned long trialNumber) const;
    unsigned long sequenceSampleDumpEstimatedRows(unsigned long selectedTrials) const;
    detection::FrequencyBandMeasurementPacket captureFrequencyMeasurementPacket(const AudioSamplePacket& audioSamplePacket) const;
    const char* sequenceTrialClassificationName(const char* result, long dtMs, long durMs, const SequenceTest::TrialDiagnostics& diagnostics) const;

    // Miscellaneous output helpers.
    bool shouldPrintSequenceTrial() const;
    bool shouldPrintSequenceSource(const AnalyzerReport& report) const;
    bool shouldPrintSequenceInspect(const AnalyzerReport& report) const;
    bool shouldPrintSequenceSystem(const AnalyzerReport& report) const;
    bool shouldPrintSequenceDetail(const AnalyzerReport& report) const;
    bool shouldPrintHardwareDiagnostics() const;
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
    mutable detection::DetectionRuntime _detection;
    FreqBandStream _freqBandStream;
    AnalyzerTuning _analyzerTuning = {};
    SeqOutputConfig _seqOutputConfig = {};
    PendingSequenceStart _pendingSequenceStart = {};

    // Console and emitter control.
    unsigned long _controlBaudRate = 115200;
    int _controlRxPin = 16;
    int _controlTxPin = 17;
    char _usbLineBuffer[256];
    size_t _usbLineLength = 0;
    char _commandScratch[256];
    char _emitterLineBuffer[256];
    size_t _emitterLineLength = 0;

    bool _controlClaimPending = false;
    bool _controlClaimSent = false;
    unsigned long _controlClaimAtMs = 0;

    // Session state.
    SequenceTest _sequenceTest;
    unsigned long _rawCaptureSequenceId = 0;
    AnalyzerReport _sequenceReportScratch = {};

    uint32_t _loopLastUs = 0;
    unsigned long _loopMaxSinceBootUs = 0;
    mutable LoopHealthStats _loopHealth;
    static constexpr unsigned long kPrintIntervalMs = 100;
};

using AnalyzerModeApp = AnalyzerApp;

constexpr size_t AnalyzerApp::debugSequenceTestSize() {
    return sizeof(SequenceTest);
}

constexpr size_t AnalyzerApp::debugSequenceTestSampleHistoryCapacity() {
    return SequenceTest::kMaxSampleHistory;
}

constexpr size_t AnalyzerApp::debugSequenceTestSampleRowsCapacity() {
    return SequenceTest::kMaxSampleRows;
}

