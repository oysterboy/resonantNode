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
    uint32_t sampleRateHz = 16000;
    long emitStartMs = -1;
    long emitDoneMs = -1;
    bool hasBoardBaseline = false;
    float boardBaseline = 0.0f;  // RAW_SUMMARY baseline= (csv only): the board's AudioSignal baseline
    std::string mode;   // RAW_BEGIN mode= (csv only)
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
    unsigned long bandLow = 2800;
    unsigned long bandHigh = 3800;
    unsigned long targetHz = 3200;
    bool preprocessForced = false;
    runtime::PcmPreprocessMode preprocess = runtime::PcmPreprocessMode::None;
    unsigned long tailMs = 300;  // silence after the capture so the last occurrence can close
    unsigned long skipMs = 0;    // ignore verdicts whose occurrence starts earlier (marker tone)
    bool compare = false;        // amp feature vs the board's column (mode=both transcripts)
    bool verbose = false;        // one line per verdict
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
};

// Profile override by the Analyzer PARAM key names plus contrast_medium / amp_medium.
bool applyProfileSet(detection::DetectionProfile& profile, const std::string& key, double value);

Result replay(const Capture& capture, const Options& options, const detection::DetectionProfile& profile);

void printVerdict(const detection::OccurrenceVerdict& verdict);

}  // namespace hostreplay
