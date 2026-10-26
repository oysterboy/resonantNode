#pragma once

/*
Host replay (NODE-006): the real detection chain on the PC, fed with RAW captures.

Chain per sample, the same calls ResonantNodeApp / AnalyzerModeApp make:
    [PcmPreprocessor] -> AudioSignal::update -> FreqBandStream::observeCenteredSample
    -> buildFrequencyMeasurementPacket -> DetectionRuntime::observeFrame -> popOccurrenceVerdict
The host clock (app/TimeBase.h, host branch) is set to each sample's time, so
debounces and inspection windows run on the capture's own axis.

Captures: a .wav from tools/bench/rawcorpus.py (24-bit mono) or a RAW serial
transcript (.csv). A transcript in mode=both/feat/pcm holds the stream
AudioSignal saw on the board (after the I2S preprocessor), one in mode=i2s the
raw PCM; `preprocess` follows that unless forced. A .wav carries no mode: it
is replayed as the preprocessed stream unless --preprocess says otherwise
(the 2026-10-14 rawscan/rawtone sessions were mode=i2s: use --preprocess dc).

ReplayMain.cpp is the command line; test/test_replay_corpus uses the same
functions on the fixtures under test/fixtures.
*/
#include <stdint.h>

#include <string>
#include <vector>

#include "../app/RuntimeDefaults.h"
#include "../detection/DetectionProfile.h"
#include "../detection/evaluation/OccurrenceVerdict.h"

namespace hostreplay {

struct Capture {
    std::string name;
    std::vector<int32_t> pcm;
    std::vector<int32_t> boardAmp;  // mode=both transcripts: the board's audioMagnitudeValue per sample
    // mode=both transcripts since step 7b: the board's frequency path per
    // sample (the target-band FrequencyScore16, the band contrast, and
    // whether a fresh Goertzel packet came with this sample).
    std::vector<int32_t> boardFreq;
    std::vector<float> boardContrast;
    std::vector<uint8_t> boardFresh;
    // RAW_BEGIN band= (Analyzer builds after 2026-10-22): the detector band the
    // board ran during the capture; 0/0 = single target. Absent in older
    // transcripts, which ran on the Analyzer's idle setting.
    bool hasBoardBand = false;
    unsigned long boardBandLow = 0;
    unsigned long boardBandHigh = 0;
    // RAW_BEGIN band_lead= / band_update_every=: samples the board's band
    // stream saw before the first stored row; the replay pads lead %
    // update_every zeros into its band stream so both update in phase.
    bool hasBandLead = false;
    unsigned long bandLead = 0;
    unsigned long bandUpdateEvery = 4;
    uint32_t sampleRateHz = 16000;
    long emitStartMs = -1;
    long emitDoneMs = -1;
    bool hasBoardBaseline = false;
    float boardBaseline = 0.0f;  // RAW_SUMMARY baseline= (csv only): the board's AudioSignal baseline
    std::string mode;   // RAW_BEGIN mode= (csv only)
    std::string preprocess;  // RAW_BEGIN preprocess= (csv, builds after 2026-10-16): dc | fd | none; empty if absent
    std::string shape;  // "f=3200 dur=100" or "... pulses=5 gap=10 hop=..." from RAW_BEGIN
    // Only mode=i2s stores the untouched I2S PCM (readRawSample); pcm, feat
    // and both store what AudioSignal received (after the preprocessor).
    bool rawPcm() const { return mode == "i2s"; }
};

bool loadWav(const std::string& path, Capture& out);
bool loadRawCsv(const std::string& path, Capture& out);
// By extension: .csv -> loadRawCsv, else loadWav. Sets name from the path.
bool loadCapture(const std::string& path, Capture& out);

struct Options {
    unsigned long bandLow = runtime::kDefaultNetworkBandLowHz;   // the network band (decisions/2026-10-16-network-band-2950-3950.md)
    unsigned long bandHigh = runtime::kDefaultNetworkBandHighHz;
    unsigned long targetHz = runtime::kDefaultChirpFrequencyHz;
    bool preprocessForced = false;
    runtime::PcmPreprocessMode preprocess = runtime::PcmPreprocessMode::None;
    unsigned long tailMs = 300;  // silence after the capture so the last occurrence can close
    unsigned long skipMs = 0;    // ignore verdicts whose occurrence starts earlier (marker tone)
    bool compare = false;        // amp feature vs the board's column (mode=both transcripts)
    bool verbose = false;        // one line per verdict
    std::string dumpFreqPath;    // if set: per-sample host vs board frequency path as CSV (step 7b)
    // DET-015 verification (step 11b, owner: verify that shape separates the signals before choosing a data
    // path): one `shape` line per verdict with the strongest band bin per ms over the occurrence's window,
    // as runs. Host-only probe; reads FreqBandStream::lastPeakBinHz(), which the board does not pass on.
    bool shape = false;
};

struct Result {
    size_t samples = 0;
    unsigned accepted = 0;
    unsigned rejected = 0;
    unsigned skipped = 0;
    std::vector<detection::OccurrenceVerdict> verdicts;  // accepted and rejected, in order (skipped excluded)
    size_t ampCompared = 0;
    double ampMeanAbsDiff = 0.0;
    double ampMaxAbsDiff = 0.0;
    // Frequency path vs the board (step 7b), compared on samples where the
    // board had a fresh packet: target FrequencyScore16 absolute, contrast
    // relative to the board's value, on samples where |board| >= 1000 (the
    // gate's range; the medium limit is 30,500).
    size_t freqCompared = 0;
    double freqMeanAbsDiff = 0.0;
    double freqMaxAbsDiff = 0.0;
    double contrastMeanRelDiff = 0.0;
    double contrastMaxRelDiff = 0.0;
};

// --set <path>=<value>: a registered param path (app/ParamBindings.h, the
// detection.scalar_* params as on the board, plus detection.contrast_medium /
// detection.amp_medium on the plan's modules), parsed and range-checked by
// ParamRegistry. Returns the status name, "ok" on success.
const char* applyProfileSet(detection::DetectionProfile& profile, const std::string& path, const std::string& valueText);

Result replay(const Capture& capture, const Options& options, const detection::DetectionProfile& profile);

void printVerdict(const detection::OccurrenceVerdict& verdict);

}  // namespace hostreplay
