#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../../hal/AudioSourceAnalog.h"
#include "../../hal/AudioSourceI2S.h"
#include "../../io/AudioFrequencyDetector.h"
#include "../../io/AudioOnsetDetector.h"
#include "../../io/AudioSignal.h"
#include "../../detection/DetectionPipeline.h"
#include "../../hal/AudioSource.h"

class AnalyzerApp {
public:
    enum AnalyzerLogFlags : uint32_t {
        ANALYZER_LOG_NONE = 0,
        ANALYZER_LOG_SUMMARY = 1u << 0,
        ANALYZER_LOG_TRIAL = 1u << 1,
        ANALYZER_LOG_CANDIDATE = 1u << 2,
        ANALYZER_LOG_FREQ_CLASS = 1u << 3,
        ANALYZER_LOG_RAW_DEBUG = 1u << 4,
    };

    static constexpr uint32_t DEFAULT_ANALYZER_LOG_FLAGS =
        ANALYZER_LOG_SUMMARY |
        ANALYZER_LOG_TRIAL;

    enum class AudioSourceKind {
        Analog,
        I2S
    };

    AnalyzerApp(int inputPin = 34, AudioSourceKind sourceKind = AudioSourceKind::I2S);

    void begin();
    void update();
    unsigned long loopDelayMs() const;

private:
    // Per-session state bundles keep the long-running modes readable and isolated.
    struct SequenceTest {
        static constexpr size_t kMaxTrialCandidates = 16;
        static constexpr size_t kMaxDuplicateDts = 8;
        enum class CandidateOrigin {
            PreWindow,
            InWindow,
            PostWindow,
        };

        struct CandidateSample {
            unsigned long candidateMs = 0;
            long dtFromTriggerMs = 0;
            long dtFromTrialStartMs = 0;
            unsigned long durationMs = 0;
            float strength = 0.0f;
            CandidateOrigin origin = CandidateOrigin::InWindow;
        };

        struct TrialReport {
            unsigned long trialNumber = 0;
            unsigned long startMs = 0;
            unsigned long endMs = 0;
            long dtMs = -1;
            long durMs = -1;
            float strength = 0.0f;
            unsigned long duplicates = 0;
            bool onsetSeen = false;
            unsigned long maxEnv = 0;
            float maxStrengthEst = 0.0f;
            unsigned long transientRejectTooShortCount = 0;
            unsigned long transientRejectTooLongCount = 0;
            unsigned long transientRejectWeakCount = 0;
            unsigned long onsetRejectPeakActiveCount = 0;
            unsigned long onsetRejectCooldownCount = 0;
            unsigned long onsetRejectOtherCount = 0;

            bool bestCandidateValid = false;
            unsigned long bestCandidateDtFromTriggerMs = 0;
            unsigned long bestCandidateDurationMs = 0;
            float bestCandidateStrength = 0.0f;
            CandidateOrigin bestCandidateOrigin = CandidateOrigin::InWindow;
            unsigned long candidateCount = 0;
            unsigned long candidateOverflowCount = 0;
            unsigned long candidatePreWindowCount = 0;
            unsigned long candidateInWindowCount = 0;
            unsigned long candidatePostWindowCount = 0;

            DetectionPipeline::FrequencyEvidence freqEarly = {};
            DetectionPipeline::FrequencyEvidence freqFull = {};

            char result[16] = {};
        };

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
            DetectionPipeline::FrequencyEvidence acceptedFrequencyEvidence = {};
            DetectionPipeline::FrequencyEvidence acceptedFrequencyEvidenceFull = {};
            unsigned long acceptedFrequencyProcessedAtMs = 0;
            unsigned long duplicateTransientMs = 0;
            float duplicateTransientStrength = 0.0f;
            unsigned long duplicateTransientDurationMs = 0;
            DetectionPipeline::FrequencyEvidence duplicateFrequencyEvidence = {};
            DetectionPipeline::FrequencyEvidence duplicateFrequencyEvidenceFull = {};
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
            AudioOnsetDetector::TransientRejectReason strongestRejectReason = AudioOnsetDetector::TransientRejectReason::None;
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

            AudioOnsetDetector::TransientRejectReason lastTransientRejectReason = AudioOnsetDetector::TransientRejectReason::None;
            float lastRejectStrength = 0.0f;
            unsigned long lastRejectDurationMs = 0;
            bool peakActiveAtEnd = false;
        };

        bool active = false;
        bool quiet = false;
        bool showDetails = true;
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
        mutable TrialReport* trialReports = nullptr;
        mutable size_t trialReportCapacity = 0;
        mutable size_t trialReportCount = 0;

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

    // Setup and control helpers.
    void configureParameters();
    void configureSharedParameters();
    void configureAnalogParameters();
    void configureI2SParameters();
    void beginEmitterControl();
    void pollUsbConsole();
    void pollEmitterSerial();
    void handleUsbLine(const char* line);
    void sendEmitterCommand(const char* command);
    void resetDetectorState();
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
    void startBaseSession(unsigned long durationMs, bool quiet = false);
    void stopBaseSession();
    void updateBaseSession(unsigned long now);
    void printBaseSummary() const;
    void printBaseHints() const;
    void startSequenceTest(unsigned long totalTrials, unsigned long periodMs, unsigned long windowEndOffsetMs, unsigned long toneHz, unsigned long durationMs, bool quiet = false, bool showDetails = true, const char* setupLabel = nullptr, uint32_t logFlags = DEFAULT_ANALYZER_LOG_FLAGS, bool sampleDumpEnabled = false, unsigned long sampleDumpFirstTrials = 2, unsigned long sampleDumpEveryNth = 0, unsigned long sampleDumpLeadMs = 50, unsigned long sampleDumpTailMs = 800, unsigned long sampleDumpStepMs = 1, unsigned long sampleDumpMaxRows = 5000);
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
    void printSequenceTrialDebug(unsigned long trialNumber, const char* result, const SequenceTest::TrialDiagnostics& diagnostics) const;
    void printSequenceTrialReports() const;
    void printSequenceTrialResult(unsigned long trialNumber, const char* result, long dtMs, long durMs, float strength, bool audioOverflow, unsigned long duplicateCount, const SequenceTest::TrialDiagnostics& diagnostics) const;
    void printSequenceFinalOutput() const;
    void printSequenceSummary() const;
    void handleSequenceCandidate(const DetectionPipeline::PatternResult& patternResult, unsigned long queueDepthBeforeDrain, const DetectionPipeline::FrequencyEvidence* liveFrequencyEvidence = nullptr);
    void updateSequenceAmbientStats();
    void beginSequenceSampleDump(unsigned long trialNumber);
    void clearSequenceSampleDump();
    void recordSequenceSample(const CurveSnapshot& snapshot);
    void flushSequenceSampleHistory(unsigned long currentSampleMs);
    void printSequenceSampleDump(unsigned long trialNumber) const;
    bool sequenceSampleDumpSelected(unsigned long trialNumber) const;
    unsigned long sequenceSampleDumpEstimatedRows(unsigned long selectedTrials) const;
    static void sequenceCurveSampleCallback(const CurveSnapshot& snapshot, void* context);
    DetectionPipeline::FrequencyEvidence captureFrequencyEvidence() const;
    void noteSequenceTransientReject(unsigned long eventMs);
    void noteSequenceTransientRejectReason(unsigned long eventMs, const char* reasonName, unsigned long durationMs, float strength);
    const char* sequenceTrialClassificationName(const char* result, long dtMs, long durMs, const SequenceTest::TrialDiagnostics& diagnostics) const;
    void printValueFrame(unsigned long now) const;
    void printValueModeBanner() const;

    // Hardware and signal chain.
    int _inputPin;
    AudioSourceAnalog _analogSource;
    AudioSourceI2S _i2sSource;
    AudioSource& _audioSource;
    AudioSourceKind _sourceKind;
    AudioSignal _audioSignal;
    AudioFrequencyDetector _audioFrequencyDetector;
    AudioOnsetDetector _audioOnsetDetector;

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
