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
#include "AnalyzerSequenceTypes.h"

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

    using SeqOutputMode = analyzer_seq::SeqOutputMode;
    using SeqOutputWhen = analyzer_seq::SeqOutputWhen;
    using SeqOutputConfig = analyzer_seq::SeqOutputConfig;

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

    AnalyzerApp();

    void begin();
    void update();
    unsigned long loopDelayMs() const;
    static constexpr size_t debugSequenceTestSize();
    static constexpr size_t debugSequenceTestSampleHistoryCapacity();
    static constexpr size_t debugSequenceTestSampleRowsCapacity();

private:
    void updateSequenceAudioHealth(const AudioSamplePacket& audioSamplePacket);
    void printSystemHealth(const AnalyzerReport& report);  // non-const: it resets the loop-health window it printed
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

    // SEQ run state, by lifetime (AnalyzerSequenceTypes.h, step 11g C4).
    using RunConfig = analyzer_seq::RunConfig;
    using TrialState = analyzer_seq::TrialState;
    using TrialDiagnostics = analyzer_seq::TrialDiagnostics;
    using RunTotals = analyzer_seq::RunTotals;
    using SampleDumpState = analyzer_seq::SampleDumpState;
    using SequenceTest = analyzer_seq::SequenceTest;
    using PendingSequenceStart = analyzer_seq::PendingSequenceStart;

    // Setup, control, and detector configuration helpers.
    void configureParameters();
    void configureI2SParameters();
    void beginEmitterControl();
    void processPendingSequenceStart();
    void pollUsbConsole();
    void pollEmitterSerial();
    void handleUsbLine(const char* line);
    // The console's handlers, one per group (AnalyzerCommands.cpp; analyzer_commands::groupOf routes). The SEQ and
    // EMIT ones return false for a sub-command they do not know.
    struct SeqStartSeen {
        bool totalTrials = false;
        bool window = false;
        bool profile = false;
    };
    void printUsbHelp();
    void handleRawCommand(const char* line);
    bool handleSeqCommand(const char* line);
    bool handleSeqOutputSetting(const char* verb, char*& savePtr);
    bool handleSeqRunCommand(const char* verb, char*& savePtr);
    void handleSeqStart(const char* verb, char*& savePtr);
    bool applySeqStartToken(const char* token, RunConfig& request, SeqOutputConfig& outputConfig, SeqStartSeen& seen);
    bool handleEmitCommand(const char* line);
    void sendClaimedEmitterCommand(const char* line, const char* name);
    // SELFTEST (NODE-015, AnalyzerSelfTest.cpp): board, mic, amp; blocking.
    void handleSelfTestCommand(const char* line);
    void restoreAfterSelfTest();
    void printSequenceHelp();
    // SEQ output sub-commands: the next start's settings, and the running copy during a run.
    template <typename Change>
    void changeSeqOutputConfig(Change change);
    void sendEmitterCommand(const char* command);
    void resetAudioSignalState();

    // Sequence-test workflows.
    void startSequenceTest(const RunConfig& requested);
    void initializeSequenceTest(const RunConfig& requested);
    void configureSequenceDetection();
    void resetSequenceTestRunState();
    void stopSequenceTest();
    void updateSequenceTest(unsigned long now);
    void finalizeSequenceTrial(unsigned long now);
    void refreshCapturedDetectorReportAggregates();
    bool runRawTrigger(unsigned long toneHz, unsigned long durationMs, unsigned long postMs, unsigned long preMs,
                       unsigned long decim, RawCaptureMode mode, const RawTriggerShape& shape);
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
    void buildSequenceAnalyzerReport(AnalyzerReport& report, unsigned long trialNumber, bool rejectedInWindow,
                                     long dtMs, long durMs, float strength, bool bufferOverrun,
                                     unsigned long duplicateCount, const TrialDiagnostics& diagnostics) const;
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
    detection::FrequencyBandMeasurementPacket captureFrequencyMeasurementPacket(
        const AudioSamplePacket& audioSamplePacket) const;

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
    detection::DetectionRuntime _detection; // Canonical detector runtime and feature history.
    FreqBandStream _freqBandStream; // Live frequency-band stream used by analyzer diagnostics.
    // SEQ tuning (PAR-003, step 11e item 9): the TonalPulseScalar profile's
    // detection.scalar_* values on the registry with the Node's paths; a SEQ
    // start copies them into the profile (effectiveSequenceProfile).
    detection::ScalarTransientConfig _scalarTuning = {};
    param::ParamRegistry _paramRegistry;
    // The next start's output settings; a run copies them once at activation into
    // _sequenceTest.config.outputConfig (SEQ sub-commands: changeSeqOutputConfig).
    SeqOutputConfig _seqOutputConfig = {};
    PendingSequenceStart _pendingSequenceStart = {}; // Deferred sequence start request.

    // Console and emitter control.
    unsigned long _controlBaudRate = EMITTER_UART_BAUD; // Serial2 baud rate to the emitter.
    int _controlRxPin = EMITTER_UART_RX_PIN; // UART RX pin for emitter control.
    int _controlTxPin = EMITTER_UART_TX_PIN; // UART TX pin for emitter control.
    serial_line::LineReader<serial_line::kCommandLineMax>
        _usbLine;      // the USB console (app/SerialLine.h: long lines refused)
    char _commandScratch[serial_line::kCommandLineMax + 1];  // mutable scratch for strtok_r over a finished line
    serial_line::LineReader<serial_line::kCommandLineMax> _emitterLine;  // the Emitter's replies on Serial2

    bool _controlClaimPending = false;  // Waiting to request emitter control.
    bool _controlClaimSent = false;  // Control claim has already been sent.
    bool _controlClaimAcked = false;  // The Emitter answered the boot claim (ANA-005).
    unsigned long _controlClaimAtMs = 0;  // When the claim was queued.

    // Session state.
    SequenceTest _sequenceTest;  // Current sequence-test execution state.
    unsigned long _rawCaptureSequenceId = 0;  // Monotonic RAW capture identifier.
    AnalyzerReport _sequenceReportScratch = {};  // Reusable scratch report buffer.
    // The one mutable: effectiveSequenceProfile() const builds the tuned profile here, a
    // scratch for const printers (printDetectionParameters) rather than a stack copy.
    mutable detection::DetectionProfile _sequenceProfileScratch = {};
    detection::FrequencyBandMeasurementPacket _runtimeFrequencyMeasurementPacketScratch =
        {};  // Reusable per-sample frequency packet scratch.
    detection::DetectionPipelineEvent _runtimePipelineEventScratch =
        {};  // Reusable pipeline event scratch for the loop task.

    uint32_t _loopLastUs = 0;  // Previous loop timestamp for timing diagnostics.
    unsigned long _loopMaxSinceBootUs = 0;  // Worst loop duration since boot.
    LoopHealthStats _loopHealth;  // Rolling loop health statistics.
};

using AnalyzerModeApp = AnalyzerApp;

constexpr size_t AnalyzerApp::debugSequenceTestSize() {
    return sizeof(SequenceTest);
}

constexpr size_t AnalyzerApp::debugSequenceTestSampleHistoryCapacity() {
    return SampleDumpState::kMaxSampleHistory;
}

constexpr size_t AnalyzerApp::debugSequenceTestSampleRowsCapacity() {
    return SampleDumpState::kMaxSampleRows;
}
