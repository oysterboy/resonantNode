#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../../hal/AudioSourceI2S.h"
#if !defined(BOARD_PIEZO)
#include "../../hal/I2sToneOutput.h"
#endif
#include "../../detection/DetectionProfile.h"
#include "../../audio/AudioSignal.h"
#include "../../detection/DetectionRuntime.h"
#include "../../detection/occurrences/InspectedOccurrence.h"
#include "../../detection/features/FreqBandStream.h"
#include "../../detection/evaluation/OccurrenceVerdict.h"
#include "../../detection/occurrences/Occurrence.h"
#include "../../audio/AudioSource.h"
#include <ParamRegistry.h>
#include "../../app/RuntimeDefaults.h"
#include "../../app/SerialLine.h"
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
    using OccurrenceVerdict = detection::OccurrenceVerdict;

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

    enum class RawCaptureMode {
        Pcm,      // Analyzer-facing PCM: raw sample plus current baseline and centered value.
        I2s,      // Transport decode only: raw I2S PCM for offset/lane debugging.
        Features, // Detector view only: centered/normalized values from the live pipeline.
        Both,     // Raw PCM plus detector view in one dump.
    };

    // What the Emitter plays during a RAW capture: one CHIRP (pulseCount 0) or a
    // PULSES train with an optional per-pulse frequency list, the same options
    // SEQ start forwards (pulses= gap= hop=). Lets the capture corpus hold the
    // hop shapes the gate battery uses (tools/bench/rawcorpus.py).
    struct RawTriggerShape {
        unsigned long pulseCount = 0;
        unsigned long pulseGapMs = 0;
        char hop[40] = {};
        unsigned long sweepToHz = 0;  // CHIRP sweep=: glide from f= to this over dur=
        // band=LO-HI|off (step 7b): the detector's band for this capture only,
        // so a transcript carries the frequency path the Node runs (the
        // network band) and not the Analyzer's idle setting.
        bool bandSet = false;
        unsigned long bandLowHz = 0;
        unsigned long bandHighHz = 0;
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

    AnalyzerApp();

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
        // Live per-trial diagnostics and rejection bookkeeping.
        struct TrialDiagnostics {
            bool accepted = false;

            unsigned long firstOnsetMs = 0;
            unsigned long acceptedPatternMs = 0;
            unsigned long acceptedPatternDurationMs = 0;
            float acceptedAmbientBaseline = 0.0f;
            bool runtimePatternCaptured = false;

            bool emitStartSeen = false;
            unsigned long emitStartDtMs = 0;

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

            unsigned long ambientBaselineSamples = 0;
            float ambientBaselineMin = 0.0f;
            float ambientBaselineMax = 0.0f;
            int maxSignalLevel = 0;

            unsigned long duplicateCount = 0;

        };

        // Sequence-test configuration and execution state.
        bool active = false;
        bool quiet = false;
        bool showDetails = true;
        SeqOutputConfig outputConfig = {};
        bool externalEmitter = false;
        detection::DetectionProfileKind profileKind = detection::DetectionProfileKind::TonalPulseScalar;
        unsigned long totalTrials = 100;
        unsigned long periodMs = 2500;
        unsigned long windowEndOffsetMs = 2200;
        unsigned long toneHz = runtime::kDefaultChirpFrequencyHz;
        unsigned long durationMs = 100;
        // pulses > 0: each trial is a PULSES train of durationMs pulses
        // (DET-011); 0 = one CHIRP. pulseRampMs only sent when set.
        unsigned long pulseCount = 0;
        unsigned long pulseGapMs = 0;
        bool pulseRampSet = false;
        unsigned long pulseRampMs = 0;
        char pulseHop[40] = {};  // hop=F1,F2,... forwarded to PULSES; empty = none
        unsigned long sweepToHz = 0;  // sweep=HZ forwarded to CHIRP (glide from toneHz over durationMs); 0 = steady
        unsigned long bandLowHz = 0;   // band=LOW-HIGH: FrequencyTarget = strongest bin in the band; 0 = off
        unsigned long bandHighHz = 0;
        unsigned long startupDelayMs = 1000;
        unsigned long reportSettleMs = 500;
        char setupLabel[48] = TEST_SETUP_LABEL;

        // Sample-dump tooling state, separate from clean analyzer truth.
        bool sampleDumpEnabled = false;
        unsigned long sampleDumpFirstTrials = 2;
        unsigned long sampleDumpEveryNth = 0;
        unsigned long sampleDumpLeadMs = 50;
        unsigned long sampleDumpTailMs = 800;
        unsigned long sampleDumpStepMs = 4; // 50 + 800 ms at 4 ms = 213 rows, fits kMaxSampleRows (issue #31 B12)
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
        detection::DetectorSelection sampleDumpDetectorSelection = detection::DetectorSelection::ScalarTransient;
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
        // ANA-007 (step 7a): when this trial's CHIRP/PULSES command left for
        // the Emitter; the dt anchor (the EMIT_START marker is read after the
        // sample loop, 21-37 ms late and jittery).
        unsigned long currentTrialCommandSentMs = 0;
        bool currentTrialCommandSent = false;
        unsigned long currentTrialStartMs = 0;
        unsigned long currentTrialEndMs = 0;
        unsigned long currentTrialOnsetDetectedMs = 0;
        unsigned long currentTrialPatternDetectedMs = 0;
        unsigned long sourceCandidateCount = 0;
        unsigned long sourceAcceptedCount = 0;
        unsigned long sourceRejectedCount = 0;
        unsigned long inspectedOccurrenceCount = 0;
        unsigned long verdictCount = 0;
        bool primaryValidPatternCaptured = false;
        detection::OccurrenceVerdict primaryValidPattern = {};
        detection::InspectedOccurrence primaryValidInspectedOccurrence = {};
        detection::DetectorReport primaryValidDetectorReport = {};
        bool primaryAcceptedOccurrenceCaptured = false;
        detection::InspectedOccurrence primaryAcceptedInspectedOccurrence = {};
        detection::DetectorReport primaryAcceptedDetectorReport = {};
        detection::SourceDiagnosticRecord primaryAcceptedSourceRecord = {};
        unsigned long rejectedInWindowCount = 0;
        bool bestRejectedPatternCaptured = false;
        detection::OccurrenceVerdict bestRejectedInWindow = {};
        detection::InspectedOccurrence bestRejectedInspectedOccurrence = {};
        detection::DetectorReport bestRejectedDetectorReport = {};
        bool selectedSourceRejectCaptured = false;
        detection::SourceDiagnosticRecord selectedSourceReject = {};
        unsigned long consumedSourceRejectEventId = 0;
        unsigned long consumedSourceRejectReportGeneration = 0;
        bool currentTrialFinalized = false;
        unsigned long currentTrialUnexpected = 0;
        unsigned long currentTrialRejected = 0;
        bool bufferOverrun = false;
        unsigned long trialOverflowCountAtStart = 0;
        unsigned long trialPipelineOverflowCountAtStart = 0;
        TrialDiagnostics currentTrialDiagnostics;

        unsigned long samplesProcessed = 0;
        unsigned long currentTrialSamplesProcessed = 0;
        unsigned long maxSamplesPerLoop = 0;
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
        unsigned long pulseCount = 0;
        unsigned long pulseGapMs = 0;
        bool pulseRampSet = false;
        unsigned long pulseRampMs = 0;
        char pulseHop[40] = {};  // hop=F1,F2,... forwarded to PULSES; empty = none
        unsigned long sweepToHz = 0;  // sweep=HZ forwarded to CHIRP (glide from toneHz over durationMs); 0 = steady
        unsigned long bandLowHz = 0;   // band=LOW-HIGH: FrequencyTarget = strongest bin in the band; 0 = off
        unsigned long bandHighHz = 0;
        unsigned long reportSettleMs = 500;
        bool quiet = false;
        bool showDetails = false;
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
    // SELFTEST (NODE-015, AnalyzerSelfTest.cpp): board, mic, amp; blocking.
    void handleSelfTestCommand(const char* line);
    void restoreAfterSelfTest();
    void printSequenceHelp();
    void sendEmitterCommand(const char* command);
    void resetAudioSignalState();

    // Sequence-test workflows.
    void startSequenceTest(const PendingSequenceStart& pending);
    void initializeSequenceTest(const PendingSequenceStart& pending);
    void configureSequenceDetection();
    void resetSequenceTestRunState();
    void stopSequenceTest();
    void updateSequenceTest(unsigned long now);
    void finalizeSequenceTrial(unsigned long now);
    void refreshCapturedDetectorReportAggregates();
    bool runRawTrigger(unsigned long toneHz, unsigned long durationMs, unsigned long postMs, unsigned long preMs, unsigned long decim, RawCaptureMode mode, const RawTriggerShape& shape);
    void printAudioSourceSummary() const;
    void printAudioRunSummary() const;
    void printFeatureHistoryRecordCounts() const;
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
    const detection::DetectionProfile& effectiveSequenceProfile() const;
    AnalyzerReport* sequenceReportScratch();
    void buildSequenceAnalyzerReport(AnalyzerReport& report, unsigned long trialNumber, AnalyzerResult result, long dtMs, long durMs, float strength, bool bufferOverrun, unsigned long duplicateCount, const SequenceTest::TrialDiagnostics& diagnostics) const;
        struct SequenceTrialSelection {
        enum class Kind {
            None,
            ValidPattern,
            AcceptedOccurrence,
            RejectedPattern,
            RejectedSourceCandidate,
            Unexpected,
            Miss,
        };

        Kind kind = Kind::None;
        const detection::OccurrenceVerdict* verdict = nullptr;
        const detection::InspectedOccurrence* inspectedOccurrence = nullptr;
        const detection::DetectorReport* detectorReport = nullptr;
        unsigned long occurrenceId = 0;
        unsigned long candidateId = 0;
        bool reportMatched = false;
        unsigned long sourceEventId = 0;
        unsigned long sourceReportGeneration = 0;
        unsigned long sourceEventTrialAttribution = 0;
        AnalyzerResult result = AnalyzerResult::Unknown;
        long dtMs = -1;
        unsigned long durationMs = 0;
        float strength = 0.0f;
    };

    unsigned long sequenceTrialOnsetAnchorMs() const;
    SequenceTrialSelection selectSequenceTrialSelection(unsigned long trialOnsetAnchorMs) const;
    void handleSequencePending(const detection::DetectionPipelineEvent& event);
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

    // Miscellaneous output helpers.
    bool shouldPrintSequenceTrial() const;
    bool shouldPrintSequenceSource(const AnalyzerReport& report) const;
    bool shouldPrintSequenceInspect(const AnalyzerReport& report) const;
    bool shouldPrintSequenceSystem(const AnalyzerReport& report) const;
    bool shouldPrintSequenceDetail(const AnalyzerReport& report) const;
    bool shouldPrintHardwareDiagnostics() const;
    static const char* sequenceOutputModeName(SeqOutputMode mode);
    static const char* sequenceOutputWhenName(SeqOutputWhen value);
    static bool sequenceOutputWhenEnabled(SeqOutputWhen configured, AnalyzerResult result);
    static SeqOutputMode sequenceOutputModeFromToken(const char* token, bool* valid = nullptr);
    static SeqOutputWhen sequenceOutputWhenFromToken(const char* token, bool* valid = nullptr);

    // Hardware and occurrence chain.
    AudioSourceI2S _i2sSource; // Concrete audio transport for analyzer mode.
    AudioSource& _audioSource; // Common audio-source view used by shared helpers.
#if !defined(BOARD_PIEZO)
    // SELFTEST amp check only: the D-AMP tone on this port's TX side. Its
    // writer task runs only during a SELFTEST (begin() ... end()).
    I2sToneOutput _selfTestTone;
#endif
    AudioSignal _audioSignal; // Audio framing for detector input packets (the same class the Node runs).
    mutable detection::DetectionRuntime _detection; // Canonical detector runtime and feature history.
    FreqBandStream _freqBandStream; // Live frequency-band stream used by analyzer diagnostics.
    // SEQ tuning (PAR-003, step 11e item 9): the TonalPulseScalar profile's
    // detection.scalar_* values on the registry with the Node's paths; a SEQ
    // start copies them into the profile (effectiveSequenceProfile).
    detection::ScalarTransientConfig _scalarTuning = {};
    param::ParamRegistry _paramRegistry;
    SeqOutputConfig _seqOutputConfig = {}; // Sequence-report output policy.
    PendingSequenceStart _pendingSequenceStart = {}; // Deferred sequence start request.

    // Console and emitter control.
    unsigned long _controlBaudRate = EMITTER_UART_BAUD; // Serial2 baud rate to the emitter.
    int _controlRxPin = EMITTER_UART_RX_PIN; // UART RX pin for emitter control.
    int _controlTxPin = EMITTER_UART_TX_PIN; // UART TX pin for emitter control.
    serial_line::LineReader<serial_line::kCommandLineMax> _usbLine;      // the USB console (app/SerialLine.h: long lines refused)
    char _commandScratch[serial_line::kCommandLineMax + 1];                   // mutable scratch for strtok_r over a finished line
    serial_line::LineReader<serial_line::kCommandLineMax> _emitterLine;  // the Emitter's replies on Serial2

    bool _controlClaimPending = false; // Waiting to request emitter control.
    bool _controlClaimSent = false; // Control claim has already been sent.
    bool _controlClaimAcked = false; // The Emitter answered the boot claim (ANA-005).
    unsigned long _controlClaimAtMs = 0; // When the claim was queued.

    // Session state.
    SequenceTest _sequenceTest; // Current sequence-test execution state.
    unsigned long _rawCaptureSequenceId = 0; // Monotonic RAW capture identifier.
    AnalyzerReport _sequenceReportScratch = {}; // Reusable scratch report buffer.
    mutable detection::DetectionProfile _sequenceProfileScratch = {}; // Reusable tuned profile scratch buffer.
    mutable detection::FrequencyBandMeasurementPacket _runtimeFrequencyMeasurementPacketScratch = {}; // Reusable per-sample frequency packet scratch.
    mutable detection::DetectionPipelineEvent _runtimePipelineEventScratch = {}; // Reusable pipeline event scratch for the loop task.

    uint32_t _loopLastUs = 0; // Previous loop timestamp for timing diagnostics.
    unsigned long _loopMaxSinceBootUs = 0; // Worst loop duration since boot.
    mutable LoopHealthStats _loopHealth; // Rolling loop health statistics.
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

