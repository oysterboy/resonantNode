#pragma once

// The Analyzer's SEQ / OBS run state, split by lifetime (step 11g C4, review A8):
//   RunConfig   what SEQ start parses, copied once when the run activates;
//   TrialState  one trial, reset to TrialState{} (resetToDefaults) at each trial start;
//   RunTotals   the run's tallies (SEQ_SUMMARY, the run-health lines), reset once at start.
// SeqOutputConfig has two roles in AnalyzerApp: the next start's settings and the
// running run's copy (RunConfig::outputConfig), copied at activation.

#include <new>
#include <stddef.h>
#include <stdint.h>
#include <type_traits>

#include "../../app/RuntimeDefaults.h"
#include "../../detection/DetectionProfile.h"
#include "../../detection/DetectionDiagnostics.h"
#include "../../detection/detectors/DetectorReport.h"
#include "../../detection/evaluation/OccurrenceVerdict.h"
#include "../../detection/features/FeatureStream.h"
#include "../../detection/occurrences/InspectedOccurrence.h"
#include "../../detection/analyzer/AnalyzerReportTypes.h"

struct DetectorInputSample {
    // Bounded detector-input trace used only by the neutral SAMPLES dump.
    unsigned long sampleMs = 0;
    float value = 0.0f;
};

namespace analyzer_seq {

// `value = T{}` in place. On the ESP32 `trial = TrialState{}` builds the ~4 KB
// default object as a stack temporary first (updateSequenceTest's frame went
// from 0.9 to 4.3 KB); constructing in place resets the same fields to the same
// defaults without it. Only for the trivially destructible state structs below.
template <typename T>
inline void resetToDefaults(T& value) {
    static_assert(std::is_trivially_destructible<T>::value, "resetToDefaults: state structs only");
    value.~T();
    new (&value) T();
}

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
    unsigned long frequencyUpdateEverySamples = 4;
};

// What SEQ start / SEQ OBS parses (the defaults are the start's defaults) and the
// run then reads unchanged. A start is parsed into a local copy and handed to the
// pending start only when every token was valid (issue #31). At activation the
// run's copy is clamped once (initializeSequenceTest) and gets outputConfig, the
// next-start output settings as they are then; quiet becomes quiet || mode=quiet.
struct RunConfig {
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
    char pulseLevel[8] = {};  // level=FS forwarded to PULSES (step 13); empty = the Emitter's level
    char pulseAtt[24] = {};   // att=DB1,DB2,... forwarded to PULSES (step 13); empty = none
    unsigned long sweepToHz = 0;  // sweep=HZ forwarded to CHIRP (glide from toneHz over durationMs); 0 = steady
    unsigned long bandLowHz = 0;   // band=LOW-HIGH: FrequencyTarget = strongest bin in the band; 0 = off
    unsigned long bandHighHz = 0;
    unsigned long startupDelayMs = 1000;
    unsigned long reportSettleMs = 500;
    char setupLabel[48] = TEST_SETUP_LABEL;

    // SAMPLES dump request (tooling, separate from clean analyzer truth).
    bool sampleDumpEnabled = false;
    unsigned long sampleDumpFirstTrials = 2;
    unsigned long sampleDumpEveryNth = 0;
    unsigned long sampleDumpLeadMs = 50;
    unsigned long sampleDumpTailMs = 800;
    // 4 ms: the default 850 ms window in 213 rows, inside the 256-row
    // per-trial buffer; at 1 ms (851 rows) every default dump was
    // refused (decision 2026-10-13, issue #31 B12).
    unsigned long sampleDumpStepMs = 4;
    unsigned long sampleDumpMaxRows = 5000;
};

// Live per-trial diagnostics (audio health, raw block checks, ambient stats).
struct TrialDiagnostics {
    float acceptedAmbientBaseline = 0.0f;

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

    uint32_t rawFrames = 0;  // 32 / 64 bits: the samples are 24-bit (deep review L5; 16-bit fields truncated)
    int32_t rawMin = 0;
    int32_t rawMax = 0;
    int64_t rawSum = 0;
    uint64_t rawAbsSum = 0;
    uint8_t rawZeroCrossings = 0;
    uint8_t rawSameValueCount = 0;
    uint8_t rawSameValueRun = 0;
    uint8_t rawSameValueMaxRun = 0;
    int32_t rawLastSample = 0;
    uint8_t rawBlockHashRepeatCount = 0;
    uint8_t rawLastBlockHash = 0;

    unsigned long ambientBaselineSamples = 0;
    float ambientBaselineMin = 0.0f;
    float ambientBaselineMax = 0.0f;
    int maxSignalLevel = 0;

    unsigned long duplicateCount = 0;
};

// One trial: its window, the evidence captured in it, its loop timing. Reset to
// its defaults as a whole (resetToDefaults) at each trial start and at run start; the trial-local counts feed
// classifySequenceTrial (issue #31: left cumulative, one rejected candidate made
// every later miss OccurrenceSeenButRejected).
struct TrialState {
    unsigned long number = 0;  // 1-based; 0 = no trial started in this run
    unsigned long scheduledAtMs = 0;
    // ANA-007 (step 7a): when this trial's CHIRP/PULSES command left for
    // the Emitter; the dt anchor (the EMIT_START marker is read after the
    // sample loop, 21-37 ms late and jittery).
    unsigned long commandSentMs = 0;
    bool commandSent = false;
    unsigned long startMs = 0;
    unsigned long endMs = 0;
    bool finalized = false;

    unsigned long sourceCandidateCount = 0;
    unsigned long sourceAcceptedCount = 0;
    unsigned long sourceRejectedCount = 0;
    unsigned long inspectedOccurrenceCount = 0;
    unsigned long verdictCount = 0;
    bool primaryValidPatternCaptured = false;
    detection::OccurrenceVerdict primaryValidPattern = {};
    detection::InspectedOccurrence primaryValidInspectedOccurrence = {};
    detection::DetectorReport primaryValidDetectorReport = {};
    // The valid verdict's own source record (review 12.0 L12: it was the first accepted event's).
    bool primaryValidSourceRecordCaptured = false;
    detection::SourceDiagnosticRecord primaryValidSourceRecord = {};
    bool primaryAcceptedOccurrenceCaptured = false;
    detection::InspectedOccurrence primaryAcceptedInspectedOccurrence = {};
    detection::DetectorReport primaryAcceptedDetectorReport = {};
    detection::SourceDiagnosticRecord primaryAcceptedSourceRecord = {};
    // The accepted occurrence's own verdict (review 12.0 M5: it was looked up through bestRejectedInWindow).
    bool primaryAcceptedVerdictCaptured = false;
    detection::OccurrenceVerdict primaryAcceptedVerdict = {};
    unsigned long rejectedInWindowCount = 0;
    bool bestRejectedPatternCaptured = false;
    detection::OccurrenceVerdict bestRejectedInWindow = {};
    detection::InspectedOccurrence bestRejectedInspectedOccurrence = {};
    detection::DetectorReport bestRejectedDetectorReport = {};
    bool selectedSourceRejectCaptured = false;
    detection::SourceDiagnosticRecord selectedSourceReject = {};
    unsigned long unexpected = 0;
    bool bufferOverrun = false;
    unsigned long overflowCountAtStart = 0;
    unsigned long pipelineOverflowCountAtStart = 0;
    TrialDiagnostics diagnostics;

    unsigned long samplesProcessed = 0;
    uint64_t totalUpdateLoopUs = 0;
    unsigned long updateLoopCount = 0;
    unsigned long updateLoopMaxUs = 0;
};

// The run's tallies, reset once at SEQ start and read by SEQ_SUMMARY and the
// run-health lines; the source-reject identity is carried across trials so one
// reject event is counted once.
struct RunTotals {
    unsigned long consumedSourceRejectEventId = 0;
    unsigned long consumedSourceRejectReportGeneration = 0;

    unsigned long samplesProcessed = 0;
    unsigned long maxSamplesPerLoop = 0;
    uint64_t availableBytesSum = 0;
    unsigned long availableBytesSamples = 0;
    unsigned long maxAvailableBytes = 0;
    unsigned long maxBlockAgeMs = 0;
    unsigned long maxUpdateLoopUs = 0;
    unsigned long maxSampleWorkUs = 0;
    unsigned long maxFinalizeTrialUs = 0;
    unsigned long maxProcessingLagMs = 0;
    unsigned long completedTrials = 0;
    AnalyzerCleanSummary cleanSummary = {};
};

// SAMPLES dump capture state (tooling, separate from clean analyzer truth).
struct SampleDumpState {
    static constexpr size_t kMaxSampleHistory = 128;
    static constexpr size_t kMaxSampleRows = 256;

    bool warned = false;
    bool selectedForTrial = false;
    bool capturing = false;
    unsigned long trialNumber = 0;
    unsigned long triggerMs = 0;
    unsigned long triggerSampleMs = 0;
    unsigned long captureStartMs = 0;
    unsigned long captureEndMs = 0;
    unsigned long nextEmitMs = 0;
    detection::DetectorSelection detectorSelection = detection::DetectorSelection::ScalarTransient;
    detection::FeatureStreamId observedStream = detection::FeatureStreamId::Unknown;
    DetectorInputSample history[kMaxSampleHistory] = {};
    size_t historyStart = 0;
    size_t historyCount = 0;
    unsigned long historyLastMs = 0;
    bool historyHasPending = false;
    DetectorInputSample historyPending = {};
    DetectorInputSample rows[kMaxSampleRows] = {};
    size_t rowCount = 0;
};

struct SequenceTest {
    bool active = false;
    unsigned long startedAtMs = 0;
    unsigned long nextTriggerAtMs = 0;
    RunConfig config;
    TrialState trial;
    RunTotals totals;
    SampleDumpState sampleDump;
};

// A SEQ start accepted by the parser, activated at the top of the next update().
struct PendingSequenceStart {
    bool active = false;
    RunConfig config;
};

}  // namespace analyzer_seq
