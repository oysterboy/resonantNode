#include "../AnalyzerModeApp.h"
#include "../../../audio/AudioPcm.h"
#include "../../../app/TextParse.h"
#include "../../../app/SerialLine.h"

#include <Arduino.h>
#include <math.h>
#include <esp_heap_caps.h>
#include <stdlib.h>
#include <string.h>
#include "../../../app/TimeBase.h"
#include "../../../app/TimingUtils.h"
#include "../../../app/SerialOut.h"

#include "../../../app/PrintKeyValue.h"
#include "AnalyzerEmitterControl.h"

using text_parse::startsWithIgnoreCase;

namespace {

constexpr unsigned long kRawCaptureFlushSamples = 256;
constexpr unsigned long kRawCaptureTimeoutSlackMs = 2000;

struct RawCapturePcmSample {
    uint16_t timeOffsetMs = 0;
    int32_t pcm = 0;
    float baseline = 0.0f;
};

struct RawCaptureFeatureSample {
    uint16_t timeOffsetMs = 0;
    uint16_t ampStrength = 0;
    uint16_t envStrength = 0;
    uint16_t frequencyScore = 0;
    uint8_t targetFresh : 1;
};

// mode=both (step 7b): PCM plus the features the shipped gates decide on, so
// the host replay can compare its frequency path with the board's: amp
// (Strength16), freq (the target-band FrequencyScore16 the scalar detector
// observes), contrast (the band contrast the contrast inspector reads) and
// fresh (a new Goertzel packet this sample). 16 bytes per row.
struct RawCaptureBothSample {
    uint16_t timeOffsetMs = 0;
    uint16_t amp = 0;
    int32_t pcm = 0;
    uint16_t freq = 0;
    uint8_t fresh = 0;
    float contrast = 0.0f;
};

const char* rawCaptureModeName(AnalyzerApp::RawCaptureMode mode) {
    switch (mode) {
        case AnalyzerApp::RawCaptureMode::Features:
            return "feat";
        case AnalyzerApp::RawCaptureMode::Both:
            return "both";
        case AnalyzerApp::RawCaptureMode::I2s:
            return "i2s";
        case AnalyzerApp::RawCaptureMode::Pcm:
        default:
            return "pcm";
    }
}

bool rawCaptureUsesFeatureStream(AnalyzerApp::RawCaptureMode mode) {
    return mode == AnalyzerApp::RawCaptureMode::Features || mode == AnalyzerApp::RawCaptureMode::Both;
}

bool rawCaptureUsesSignalView(AnalyzerApp::RawCaptureMode mode) {
    return mode == AnalyzerApp::RawCaptureMode::Pcm || rawCaptureUsesFeatureStream(mode);
}

uint16_t rawCaptureOffsetMs(unsigned long sampleMs, unsigned long baseMs) {
    if (timing::atOrBefore(sampleMs, baseMs)) {
        return 0;
    }

    const unsigned long deltaMs = sampleMs - baseMs;
    return deltaMs > 0xFFFFUL ? 0xFFFFU : static_cast<uint16_t>(deltaMs);
}

void printRawMemoryState(const char* label) {
    serialOut().print("RAW_MEM label=");
    serialOut().print(label);
    printKeyValue("free_heap", ESP.getFreeHeap());
    printKeyValue("min_heap", ESP.getMinFreeHeap());
    printKeyValue("largest_8bit", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    printKeyValue("free_8bit", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    serialOut().println();
}

unsigned long rawCaptureChunkSize(unsigned long sampleRateHz, unsigned long decim) {
    const unsigned long baseChunk = sampleRateHz / 20UL;
    const unsigned long decimatedChunk = decim > 0 ? baseChunk / decim : baseChunk;
    return decimatedChunk > 0 ? decimatedChunk : 1UL;
}

bool rawCaptureUsesPcmRows(AnalyzerApp::RawCaptureMode mode) {
    return mode == AnalyzerApp::RawCaptureMode::Pcm || mode == AnalyzerApp::RawCaptureMode::I2s;
}

// The rows of one row type: the pre-emit ring and the stored window (the pre window is copied in front of the post
// rows). Freed on every way out of runRawTrigger (step 12.4.5; it was six manual malloc / free pairs).
template <typename Row>
struct RawRows {
    Row* preRing = nullptr;
    Row* rows = nullptr;

    RawRows() = default;
    RawRows(const RawRows&) = delete;
    RawRows& operator=(const RawRows&) = delete;
    ~RawRows() {
        free(preRing);
        free(rows);
    }

    // The capture rows first: the larger block, taken while the largest free block is whole. The pre ring first
    // split it, and mode=both with pre= could not get its 6,000 rows (step 12.4.10).
    bool allocate(unsigned long preRows, unsigned long captureRows) {
        rows = static_cast<Row*>(malloc(captureRows * sizeof(Row)));
        if (rows == nullptr) {
            serialOut().println("RAW_ERR memory=raw_buffer_alloc_failed");
            printRawMemoryState("raw_buffer");
            return false;
        }
        if (preRows > 0) {
            preRing = static_cast<Row*>(malloc(preRows * sizeof(Row)));
            if (preRing == nullptr) {
                free(rows);  // before the memory report
                rows = nullptr;
                serialOut().println("RAW_ERR memory=pre_ring_alloc_failed");
                printRawMemoryState("pre_ring");
                return false;
            }
        }
        return true;
    }
};

// Where the capture stands: the emitter's start and done, the pre ring's fill, the post rows stored.
struct RawCaptureProgress {
    unsigned long preWanted = 0;  // the pre ring's size
    unsigned long postWanted = 0;
    bool emitStarted = false;
    bool emitDone = false;
    bool preWindowCopied = false;
    unsigned long emitStartMs = 0;
    unsigned long emitDoneMs = 0;
    unsigned long preCapturedTotal = 0;
    unsigned long preWindowSamples = 0;
    unsigned long preWriteIndex = 0;
    unsigned long postCaptured = 0;
};

// One sample: into the pre ring until the emitter starts, then into the post rows until they are full.
template <typename Row>
void storeRawRow(RawRows<Row>& buffers, RawCaptureProgress& progress, const Row& row) {
    if (!progress.emitStarted) {
        if (buffers.preRing != nullptr) {
            buffers.preRing[progress.preWriteIndex] = row;
            progress.preWriteIndex = (progress.preWriteIndex + 1UL) % progress.preWanted;
        }
        ++progress.preCapturedTotal;
    } else if (progress.postCaptured < progress.postWanted) {
        if (buffers.rows != nullptr) {
            buffers.rows[progress.preWindowSamples + progress.postCaptured] = row;
        }
        ++progress.postCaptured;
    }
}

// The pre ring, oldest first, in front of the post rows; once, when the emitter starts or the capture ends.
template <typename Row>
void copyRawPreWindow(RawRows<Row>& buffers, RawCaptureProgress& progress) {
    if (progress.preWindowCopied || progress.preWanted == 0) {
        progress.preWindowCopied = true;
        return;
    }
    const bool ringFull = progress.preCapturedTotal >= progress.preWanted;
    progress.preWindowSamples = ringFull ? progress.preWanted : progress.preCapturedTotal;
    const unsigned long startIndex = ringFull ? progress.preWriteIndex : 0UL;
    if (buffers.preRing != nullptr && buffers.rows != nullptr) {
        for (unsigned long i = 0; i < progress.preWindowSamples; ++i) {
            buffers.rows[i] = buffers.preRing[(startIndex + i) % progress.preWanted];
        }
    }
    progress.preWindowCopied = true;
}

// The rows of the capture's mode (pcm and i2s share one row type).
struct RawCaptureBuffers {
    AnalyzerApp::RawCaptureMode mode = AnalyzerApp::RawCaptureMode::Pcm;
    RawRows<RawCapturePcmSample> pcm;
    RawRows<RawCaptureFeatureSample> features;
    RawRows<RawCaptureBothSample> both;

    size_t rowSize() const {
        if (rawCaptureUsesPcmRows(mode)) {
            return sizeof(RawCapturePcmSample);
        }
        return mode == AnalyzerApp::RawCaptureMode::Features ? sizeof(RawCaptureFeatureSample)
                                                             : sizeof(RawCaptureBothSample);
    }
    bool allocate(unsigned long preRows, unsigned long captureRows) {
        if (rawCaptureUsesPcmRows(mode)) {
            return pcm.allocate(preRows, captureRows);
        }
        return mode == AnalyzerApp::RawCaptureMode::Features ? features.allocate(preRows, captureRows)
                                                             : both.allocate(preRows, captureRows);
    }
    void copyPreWindow(RawCaptureProgress& progress) {
        if (rawCaptureUsesPcmRows(mode)) {
            copyRawPreWindow(pcm, progress);
        } else if (mode == AnalyzerApp::RawCaptureMode::Features) {
            copyRawPreWindow(features, progress);
        } else {
            copyRawPreWindow(both, progress);
        }
    }
};

// Every sample read, stored or not (pre=0 drops the pre-emit ones): the mean divides by this count, not by the
// stored rows (issue #31).
struct RawWindowStats {
    int64_t sum = 0;
    uint64_t absSum = 0;
    unsigned long count = 0;
    int32_t min = INT32_MAX;
    int32_t max = INT32_MIN;

    void add(int32_t sample) {
        sum += static_cast<int64_t>(sample);
        ++count;
        absSum += static_cast<uint64_t>(sample < 0 ? -static_cast<int64_t>(sample) : static_cast<int64_t>(sample));
        if (sample < min) {
            min = sample;
        }
        if (sample > max) {
            max = sample;
        }
    }
};

// One CHIRP, or the same PULSES line SEQ start sends (trial=0: no trial bookkeeping on the Emitter side);
// EMIT_START / EMIT_DONE bracket either.
void formatRawEmitterCommand(char* out, size_t size, unsigned long toneHz, unsigned long durationMs,
                             const AnalyzerApp::RawTriggerShape& shape) {
    if (shape.pulseCount > 0) {
        int written = snprintf(out, size, "PULSES %lu %lu %lu trial=0 freq=%lu", shape.pulseCount, durationMs,
                               shape.pulseGapMs, toneHz);
        if (shape.hop[0] != '\0' && written > 0 && static_cast<size_t>(written) < size) {
            snprintf(out + written, size - written, " hop=%s", shape.hop);
        }
    } else if (shape.sweepToHz != 0) {
        snprintf(out, size, "CHIRP freq=%lu dur=%lu sweep=%lu", toneHz, durationMs, shape.sweepToHz);
    } else {
        snprintf(out, size, "CHIRP freq=%lu dur=%lu", toneHz, durationMs);
    }
}

} // namespace

// One RAW capture: what was asked, the plan, the rows, where it stands.
struct AnalyzerApp::RawCaptureRun {
    RawCaptureMode mode = RawCaptureMode::Pcm;
    const RawTriggerShape* shape = nullptr;
    unsigned long toneHz = 0;
    unsigned long durationMs = 0;
    unsigned long preMs = 0;
    unsigned long postMs = 0;
    unsigned long decim = 0;
    bool useSignalView = false;
    // mode=i2s with decim=N keeps every Nth word, so the fixed row budget covers N times longer (e.g. a clap test
    // over seconds). Other modes keep using decim for output chunking only.
    unsigned long i2sKeepEvery = 1;
    uint32_t storedRateHz = 0;
    unsigned long maxSamples = 6000UL;
    unsigned long captureRows = 0;
    unsigned long captureId = 0;
    unsigned long commandMs = 0;  // also the rows' time base
    RawCaptureBuffers buffers;
    RawCaptureProgress progress;
    RawWindowStats stats;
    unsigned long i2sDecimCounter = 0;
    timebase::MsFromUs64 msFromUs;
    serial_line::LineReader<serial_line::kEmitterLineMax> emitterLine;
};

bool AnalyzerApp::runRawTrigger(unsigned long toneHz, unsigned long durationMs, unsigned long postMs,
                                unsigned long preMs, unsigned long decim, RawCaptureMode mode,
                                const RawTriggerShape& shape) {
    stopSequenceTest();
    // band= (step 7b): set for this capture, restored on every way out.
    struct BandRestore {
        FreqBandStream& band;
        unsigned long low;
        unsigned long high;
        ~BandRestore() { band.setBand(low, high); }
    } bandRestore{_freqBandStream, _freqBandStream.bandLowHz(), _freqBandStream.bandHighHz()};
    if (shape.bandSet) {
        _freqBandStream.setBand(shape.bandLowHz, shape.bandHighHz);
    }
    // Start the band bank from rest, as the host replay does, so its round-
    // robin bin updates run in the same phase and the freq/contrast columns
    // compare sample for sample (step 7b). The pre window fills the 64-sample
    // Goertzel window long before the trigger.
    _freqBandStream.resetState();
    resetAudioSignalState();
    _audioSource.resetStats();

    RawCaptureRun run;
    run.mode = mode;
    run.buffers.mode = mode;
    run.shape = &shape;
    run.toneHz = toneHz == 0 ? 1 : toneHz;
    run.durationMs = durationMs == 0 ? 1 : durationMs;
    run.postMs = postMs == 0 ? 1 : postMs;
    run.preMs = preMs;
    run.decim = decim == 0 ? 1 : decim;
    run.useSignalView = rawCaptureUsesSignalView(mode);
    serialOut().print("RAW_INFO mode=");
    serialOut().print(rawCaptureModeName(mode));
    serialOut().println(" output=csv");
    serialOut().print("RAW_INFO row_size=");
    serialOut().print(run.buffers.rowSize());
    serialOut().println();

    planRawCapture(run);
    if (!run.buffers.allocate(run.progress.preWanted, run.captureRows)) {
        return false;
    }
    serialOut().print("RAW_ALLOC capture_rows=");
    serialOut().print(run.captureRows);
    printKeyValue("pre_ring_rows", run.progress.preWanted);
    printKeyValue("row_size", static_cast<unsigned long>(run.buffers.rowSize()));
    printKeyValue("bytes",
                  static_cast<unsigned long>((run.captureRows + run.progress.preWanted) * run.buffers.rowSize()));
    printKeyValue("max_rows", run.maxSamples);
    printKeyValue("largest_free_after", static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    serialOut().println();

    unsigned long flushedSamples = 0;
    int discardedSample = 0;
    uint64_t discardedSampleTimeUs = 0;
    while (flushedSamples < kRawCaptureFlushSamples &&
           (run.useSignalView ? _audioSource.readSample(discardedSample, discardedSampleTimeUs)
                              : _audioSource.readRawSample(discardedSample, discardedSampleTimeUs))) {
        ++flushedSamples;
    }

    sendEmitterCommand("MODE REMOTE");
    if (!waitForEmitterAck("OK MODE REMOTE", kEmitterModeAckTimeoutMs)) {
        serialOut().println("RAW_ERR emitter_remote_claim_timeout");
        printRawMemoryState("emitter_timeout");
        return false;
    }

    run.captureId = ++_rawCaptureSequenceId;
    run.commandMs = timebase::nowMs();
    char emitterCommand[128];
    formatRawEmitterCommand(emitterCommand, sizeof(emitterCommand), run.toneHz, run.durationMs, shape);
    sendEmitterCommand(emitterCommand);
    Serial2.flush();

    captureRawWindow(run);
    printRawCapture(run);
    return true;
}

// The row budget: pre and post windows in stored samples, together at most maxSamples rows.
void AnalyzerApp::planRawCapture(RawCaptureRun& run) const {
    const uint32_t sampleRateHz =
        _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : runtime::kDefaultAudioI2SSampleRateHz;
    run.i2sKeepEvery = (run.mode == RawCaptureMode::I2s && run.decim > 1) ? run.decim : 1UL;
    run.storedRateHz = sampleRateHz / run.i2sKeepEvery;
    const unsigned long maxSamples = run.maxSamples;
    unsigned long preWanted = static_cast<unsigned long>(
        (static_cast<uint64_t>(run.preMs) * static_cast<uint64_t>(run.storedRateHz)) / 1000ULL);
    unsigned long postWanted = static_cast<unsigned long>(
        (static_cast<uint64_t>(run.postMs) * static_cast<uint64_t>(run.storedRateHz)) / 1000ULL);
    if (preWanted > maxSamples) {
        preWanted = maxSamples;
    }
    if (postWanted > maxSamples) {
        postWanted = maxSamples;
    }
    if (preWanted + postWanted > maxSamples) {
        if (preWanted >= maxSamples) {
            preWanted = maxSamples;
            postWanted = 0;
        } else {
            postWanted = maxSamples - preWanted;
        }
    }
    if (preWanted == 0 && postWanted == 0) {
        postWanted = 1;
    }
    run.progress.preWanted = preWanted;
    run.progress.postWanted = postWanted;
    // ANA-008 (step 7a): the capture buffer was always maxSamples rows,
    // whatever was asked; size it to the request (the pre window is copied
    // in front of the post samples, so pre + post rows are enough).
    run.captureRows = preWanted + postWanted;
}

// Read samples until the post rows are full (or the deadline), then wait for the emitter's done line.
void AnalyzerApp::captureRawWindow(RawCaptureRun& run) {
    RawCaptureProgress& progress = run.progress;
    const unsigned long overallDeadlineMs = run.commandMs + run.preMs + run.postMs + kRawCaptureTimeoutSlackMs;
    while (timing::atOrBefore(timebase::nowMs(), overallDeadlineMs)) {
        pumpRawEmitterSerial(run);

        if (progress.emitStarted && progress.preWindowCopied && progress.postCaptured >= progress.postWanted) {
            break;
        }

        if (!captureRawSample(run)) {
            delay(1);
        }
    }

    pumpRawEmitterSerial(run);
    if (!progress.emitStarted) {
        run.buffers.copyPreWindow(progress);
    }
    if (progress.emitStarted && !progress.emitDone) {
        const unsigned long emitDoneDeadlineMs = progress.emitStartMs + run.postMs + kRawCaptureTimeoutSlackMs;
        while (timing::atOrBefore(timebase::nowMs(), emitDoneDeadlineMs) && !progress.emitDone) {
            pumpRawEmitterSerial(run);
            if (!progress.emitDone) {
                delay(1);
            }
        }
        pumpRawEmitterSerial(run);
    }
}

void AnalyzerApp::pumpRawEmitterSerial(RawCaptureRun& run) {
    while (Serial2.available() > 0) {
        if (run.emitterLine.feed(static_cast<char>(Serial2.read())) && run.emitterLine.length() > 0) {
            noteRawEmitterLine(run, run.emitterLine.line());
        }
    }
}

void AnalyzerApp::noteRawEmitterLine(RawCaptureRun& run, const char* line) {
    if (line == nullptr || *line == '\0') {
        return;
    }

    RawCaptureProgress& progress = run.progress;
    const bool startLine = startsWithIgnoreCase(line, "EMIT_START") || startsWithIgnoreCase(line, "EMIT_DRIVE_ON");
    const bool doneLine = startsWithIgnoreCase(line, "EMIT_DONE") || startsWithIgnoreCase(line, "EMIT_DRIVE_OFF");
    const char* label = nullptr;
    unsigned long atMs = 0;
    if (startLine && !progress.emitStarted) {
        progress.emitStarted = true;
        progress.emitStartMs = timebase::nowMs();
        run.buffers.copyPreWindow(progress);
        label = "RAW_EMIT_START id=";
        atMs = progress.emitStartMs;
    } else if (doneLine && !progress.emitDone) {
        progress.emitDone = true;
        progress.emitDoneMs = timebase::nowMs();
        label = "RAW_EMIT_DONE id=";
        atMs = progress.emitDoneMs;
    }
    if (label != nullptr && !rawCaptureUsesPcmRows(run.mode)) {
        serialOut().print(label);
        serialOut().print(run.captureId);
        printKeyValue("t_ms", atMs);
        printKeyValue("line", line);
        serialOut().println();
    }
}

// One sample from the source into the mode's rows; false when none was waiting.
bool AnalyzerApp::captureRawSample(RawCaptureRun& run) {
    int sourceSample = 0;
    uint64_t sampleTimeUs64 = 0;
    if (!(run.useSignalView ? _audioSource.readSample(sourceSample, sampleTimeUs64)
                            : _audioSource.readRawSample(sourceSample, sampleTimeUs64))) {
        return false;
    }
    if (run.i2sKeepEvery > 1 && (run.i2sDecimCounter++ % run.i2sKeepEvery) != 0) {
        return true;
    }

    const uint16_t offsetMs = rawCaptureOffsetMs(run.msFromUs(sampleTimeUs64), run.commandMs);
    run.stats.add(static_cast<int32_t>(sourceSample));
    if (!run.useSignalView) {
        // mode=i2s: the raw PCM, before AudioSignal.
        RawCapturePcmSample row;
        row.timeOffsetMs = offsetMs;
        row.pcm = static_cast<int32_t>(sourceSample);
        storeRawRow(run.buffers.pcm, run.progress, row);
        return true;
    }

    AudioSamplePacket audioSamplePacket = {};
    _audioSignal.update(sourceSample, sampleTimeUs64, audioSamplePacket);
    if (run.mode == RawCaptureMode::Pcm) {
        RawCapturePcmSample row;
        row.timeOffsetMs = offsetMs;
        row.pcm = audioSamplePacket.rawAudioValue;
        row.baseline = audioSamplePacket.baseline;
        storeRawRow(run.buffers.pcm, run.progress, row);
        return true;
    }

    _freqBandStream.observeCenteredSample(audioSamplePacket.baselineCorrectedValue, audioSamplePacket.timeMs);
    const detection::FrequencyBandMeasurementPacket frequencyEvidence =
        captureFrequencyMeasurementPacket(audioSamplePacket);
    _detection.observeFrame(audioSamplePacket, frequencyEvidence, audioSamplePacket.timeMs);
    if (run.mode == RawCaptureMode::Features) {
        RawCaptureFeatureSample row;
        row.timeOffsetMs = offsetMs;
        row.ampStrength = audioSamplePacket.audioMagnitudeValue;
        row.envStrength = audioSamplePacket.smoothedLevel;
        row.frequencyScore = frequencyEvidence.targetBandValue;
        row.targetFresh = frequencyEvidence.fresh ? 1U : 0U;
        storeRawRow(run.buffers.features, run.progress, row);
    } else {
        RawCaptureBothSample row;
        row.timeOffsetMs = offsetMs;
        row.pcm = static_cast<int32_t>(sourceSample);
        row.amp = audioSamplePacket.audioMagnitudeValue;
        row.freq = frequencyEvidence.targetBandValue;
        row.fresh = frequencyEvidence.fresh ? 1U : 0U;
        row.contrast = frequencyEvidence.targetBandContrastValue;
        storeRawRow(run.buffers.both, run.progress, row);
    }
    return true;
}

// RAW_BEGIN, the CSV rows, RAW_SUMMARY.
void AnalyzerApp::printRawCapture(const RawCaptureRun& run) const {
    const unsigned long capturedSamples = run.progress.preWindowSamples + run.progress.postCaptured;
    printRawBegin(run);
    printRawRows(run, capturedSamples);
    printRawSummary(run, capturedSamples);
}

void AnalyzerApp::printRawBegin(const RawCaptureRun& run) const {
    const RawCaptureProgress& progress = run.progress;
    const RawTriggerShape& shape = *run.shape;
    serialOut().print("RAW_BEGIN id=");
    serialOut().print(run.captureId);
    printKeyValue("sr", run.storedRateHz);
    printKeyValue("trigger_ms", progress.emitStarted ? progress.emitStartMs : run.commandMs);
    printKeyValue("command_ms", run.commandMs);
    printKeyValue("emit_seen", progress.emitStarted ? 1 : 0);
    printKeyValue("emit_start_ms", progress.emitStartMs);
    printKeyValue("emit_done_ms", progress.emitDoneMs);
    printKeyValue("f", run.toneHz);
    printKeyValue("dur", run.durationMs);
    if (shape.pulseCount > 0) {
        printKeyValue("pulses", shape.pulseCount);
        printKeyValue("gap", shape.pulseGapMs);
        if (shape.hop[0] != '\0') {
            printKeyValue("hop", shape.hop);
        }
    } else if (shape.sweepToHz != 0) {
        printKeyValue("sweep", shape.sweepToHz);
    }
    printKeyValue("pre_ms", run.preMs);
    printKeyValue("post_ms", run.postMs);
    serialOut().print(" band=");
    if (_freqBandStream.bandLowHz() == 0) {
        serialOut().print("off");
    } else {
        serialOut().print(_freqBandStream.bandLowHz());
        serialOut().print('-');
        serialOut().print(_freqBandStream.bandHighHz());
    }
    // How many samples the band stream saw (since its reset at the start of
    // this capture) before the first stored row; a replay that feeds that
    // many mod frequencyUpdateEverySamples first runs in the same update phase.
    printKeyValue("band_lead",
                  progress.preCapturedTotal > progress.preWindowSamples
                      ? progress.preCapturedTotal - progress.preWindowSamples
                      : 0UL);
    printKeyValue("band_update_every", _freqBandStream.frequencyUpdateEverySamples());
    // Which PCM conditioning produced the stored stream (mode=i2s stores the
    // raw PCM, every other mode what AudioSignal received), so a transcript
    // used as a host fixture says which chain it represents.
    printKeyValue("preprocess",
                  runtime::kPcmPreprocessMode == runtime::PcmPreprocessMode::DcBlocker             ? "dc"
                      : runtime::kPcmPreprocessMode == runtime::PcmPreprocessMode::FirstDifference ? "fd"
                                                                                                   : "none");
    printKeyValue("decim", run.decim);
    printKeyValue("pre_samples", progress.preWindowSamples);
    printKeyValue("post_samples", progress.postCaptured);
    printKeyValue("mode", rawCaptureModeName(run.mode));
    serialOut().print(" output=csv");
    if (run.mode == RawCaptureMode::Pcm) {
        serialOut().print(" fields=ms,pcm,baseline,centered");
    } else if (run.mode == RawCaptureMode::I2s) {
        serialOut().print(" fields=ms,pcm");
    } else if (run.mode == RawCaptureMode::Features) {
        serialOut().print(" fields=ms,amp_strength,env_strength,freq_score,freq_fresh");
    } else {
        serialOut().print(" fields=ms,amp,pcm,freq,contrast,fresh");
    }
    serialOut().println();
}

void AnalyzerApp::printRawRows(const RawCaptureRun& run, unsigned long capturedSamples) const {
    const unsigned long baseMs = run.commandMs;
    if (run.mode == RawCaptureMode::Pcm) {
        serialOut().println("ms,pcm,baseline,centered");
        for (unsigned long i = 0; i < capturedSamples; ++i) {
            const RawCapturePcmSample& sample = run.buffers.pcm.rows[i];
            serialOut().print(static_cast<unsigned long>(sample.timeOffsetMs) + baseMs);
            serialOut().print(",");
            serialOut().print(sample.pcm);
            serialOut().print(",");
            serialOut().print(sample.baseline, 1);
            serialOut().print(",");
            serialOut().println(sample.pcm - static_cast<int32_t>(sample.baseline));
        }
    } else if (run.mode == RawCaptureMode::I2s) {
        serialOut().println("ms,pcm");
        for (unsigned long i = 0; i < capturedSamples; ++i) {
            const RawCapturePcmSample& sample = run.buffers.pcm.rows[i];
            serialOut().print(static_cast<unsigned long>(sample.timeOffsetMs) + baseMs);
            serialOut().print(",");
            serialOut().println(sample.pcm);
        }
    } else if (run.mode == RawCaptureMode::Features) {
        serialOut().println("ms,amp_strength,env_strength,freq_score,freq_fresh");
        for (unsigned long i = 0; i < capturedSamples; ++i) {
            const RawCaptureFeatureSample& sample = run.buffers.features.rows[i];
            serialOut().print(static_cast<unsigned long>(sample.timeOffsetMs) + baseMs);
            serialOut().print(",");
            serialOut().print(sample.ampStrength);
            serialOut().print(",");
            serialOut().print(sample.envStrength);
            serialOut().print(",");
            serialOut().print(sample.frequencyScore);
            serialOut().print(",");
            serialOut().println(sample.targetFresh ? 1 : 0);
        }
    } else {
        serialOut().println("ms,amp,pcm,freq,contrast,fresh");
        for (unsigned long i = 0; i < capturedSamples; ++i) {
            const RawCaptureBothSample& sample = run.buffers.both.rows[i];
            serialOut().print(static_cast<unsigned long>(sample.timeOffsetMs) + baseMs);
            serialOut().print(",");
            serialOut().print(sample.amp);
            serialOut().print(",");
            serialOut().print(sample.pcm);
            serialOut().print(",");
            serialOut().print(sample.freq);
            serialOut().print(",");
            serialOut().print(sample.contrast, 1);
            serialOut().print(",");
            serialOut().println(sample.fresh);
        }
    }
}

void AnalyzerApp::printRawSummary(const RawCaptureRun& run, unsigned long capturedSamples) const {
    uint16_t maxAmp = 0;
    int32_t maxPcmAbs = 0;
    for (unsigned long i = 0; i < capturedSamples; ++i) {
        int32_t pcm = 0;
        if (rawCaptureUsesPcmRows(run.mode)) {
            pcm = run.buffers.pcm.rows[i].pcm;
        } else if (run.mode == RawCaptureMode::Features) {
            maxAmp =
                run.buffers.features.rows[i].ampStrength > maxAmp ? run.buffers.features.rows[i].ampStrength : maxAmp;
            continue;
        } else {
            pcm = run.buffers.both.rows[i].pcm;
            maxAmp =
                run.buffers.both.rows[i].amp > maxAmp ? static_cast<uint16_t>(run.buffers.both.rows[i].amp) : maxAmp;
        }
        const int32_t absPcm = pcm < 0 ? -pcm : pcm;
        if (absPcm > maxPcmAbs) {
            maxPcmAbs = absPcm;
        }
    }

    const RawWindowStats& stats = run.stats;
    const unsigned long requestedSamples = run.progress.preWanted + run.progress.postWanted;
    serialOut().print("RAW_SUMMARY id=");
    serialOut().print(run.captureId);
    printKeyValue("requested", requestedSamples);
    printKeyValue("captured", capturedSamples);
    printKeyValue("unused_capacity", run.captureRows > capturedSamples ? run.captureRows - capturedSamples : 0);
    printKeyValue("max_pcm_abs", maxPcmAbs);
    if (rawCaptureUsesFeatureStream(run.mode)) {
        printKeyValue("max_amp", maxAmp);
    }
    printKeyValue("raw_mean", stats.count > 0 ? static_cast<float>(stats.sum) / static_cast<float>(stats.count) : 0.0f,
                  1);
    printKeyValue("raw_abs_mean",
                  stats.count > 0 ? static_cast<float>(stats.absSum) / static_cast<float>(stats.count) : 0.0f, 1);
    printKeyValue("raw_min", stats.count > 0 ? stats.min : 0);
    printKeyValue("raw_max", stats.count > 0 ? stats.max : 0);
    printKeyValue("baseline", _audioSignal.baseline(), 1);
    serialOut().println();
}
