#include "Replay.h"

#include <Arduino.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <filesystem>

#include "../app/TimeBase.h"
#include "../audio/AudioSignal.h"
#include "../audio/AudioSource.h"
#include "../audio/PcmPreprocessor.h"
#include "../detection/DetectionRuntime.h"
#include "../detection/evaluation/VerdictNames.h"
#include "../detection/features/FreqBandStream.h"
#include "../detection/features/FrequencyMeasurementPacketBuilder.h"
#include "../detection/inspection/InspectionNames.h"

namespace hostreplay {

namespace {

bool endsWith(const std::string& s, const char* suffix) {
    const size_t n = strlen(suffix);
    return s.size() >= n && strcmp(s.c_str() + s.size() - n, suffix) == 0;
}

uint32_t le32(const unsigned char* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
uint16_t le16(const unsigned char* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

// The capture plays back through an AudioSource so AudioSignal is wired
// exactly as on the board (it only reads samples through update()).
class VectorAudioSource : public AudioSource {
public:
    VectorAudioSource(const std::vector<int32_t>& pcm, uint32_t rate) : _pcm(pcm), _rate(rate) {}
    void begin() override {}
    bool available() override { return _index < _pcm.size(); }
    bool readSample(int& sample, uint64_t& sampleTimeUs64) override {
        if (_index >= _pcm.size()) {
            return false;
        }
        sample = _pcm[_index];
        sampleTimeUs64 = timeUs(_index);
        ++_index;
        return true;
    }
    uint32_t sampleRateHz() const override { return _rate; }
    uint64_t timeUs(size_t index) const { return (static_cast<uint64_t>(index) * 1000000ULL) / _rate; }

private:
    const std::vector<int32_t>& _pcm;
    uint32_t _rate;
    size_t _index = 0;
};

}  // namespace

bool loadWav(const std::string& path, Capture& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (f == nullptr) {
        return false;
    }
    std::vector<unsigned char> bytes;
    unsigned char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        bytes.insert(bytes.end(), buf, buf + n);
    }
    fclose(f);
    if (bytes.size() < 44 || memcmp(bytes.data(), "RIFF", 4) != 0 || memcmp(bytes.data() + 8, "WAVE", 4) != 0) {
        return false;
    }
    size_t pos = 12;
    uint16_t channels = 1;
    uint16_t bits = 16;
    uint32_t rate = 16000;
    while (pos + 8 <= bytes.size()) {
        const uint32_t size = le32(bytes.data() + pos + 4);
        const unsigned char* body = bytes.data() + pos + 8;
        if (memcmp(bytes.data() + pos, "fmt ", 4) == 0 && size >= 16) {
            channels = le16(body + 2);
            rate = le32(body + 4);
            bits = le16(body + 14);
        } else if (memcmp(bytes.data() + pos, "data", 4) == 0) {
            const size_t bytesPerSample = bits / 8;
            const size_t frameBytes = bytesPerSample * channels;
            const size_t frames = frameBytes > 0 ? size / frameBytes : 0;
            out.pcm.reserve(frames);
            for (size_t i = 0; i < frames && pos + 8 + (i + 1) * frameBytes <= bytes.size(); ++i) {
                const unsigned char* s = body + i * frameBytes;  // first channel
                int32_t v = 0;
                if (bits == 16) {
                    v = static_cast<int32_t>(static_cast<int16_t>(le16(s))) << 8;  // scale to the 24-bit canonical range
                } else if (bits == 24) {
                    v = static_cast<int32_t>(static_cast<uint32_t>(s[0] | (s[1] << 8) | (s[2] << 16)) << 8) >> 8;
                } else if (bits == 32) {
                    v = static_cast<int32_t>(le32(s)) >> 8;
                }
                out.pcm.push_back(v);
            }
            break;
        }
        pos += 8 + size + (size & 1);
    }
    out.sampleRateHz = rate;
    return !out.pcm.empty();
}

bool loadRawCsv(const std::string& path, Capture& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (f == nullptr) {
        return false;
    }
    char line[512];
    int pcmCol = -1;
    int ampCol = -1;
    while (fgets(line, sizeof(line), f) != nullptr) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (strncmp(line, "RAW_BEGIN ", 10) == 0) {
            const char* p;
            if ((p = strstr(line, " sr=")) != nullptr) {
                out.sampleRateHz = static_cast<uint32_t>(strtoul(p + 4, nullptr, 10));
            }
            if ((p = strstr(line, " emit_start_ms=")) != nullptr) {
                out.emitStartMs = strtol(p + 15, nullptr, 10);
            }
            if ((p = strstr(line, " emit_done_ms=")) != nullptr) {
                out.emitDoneMs = strtol(p + 14, nullptr, 10);
            }
            if ((p = strstr(line, " mode=")) != nullptr) {
                const char* end = strchr(p + 6, ' ');
                out.mode.assign(p + 6, end != nullptr ? static_cast<size_t>(end - p - 6) : strlen(p + 6));
            }
            if ((p = strstr(line, " preprocess=")) != nullptr) {
                const char* end = strchr(p + 12, ' ');
                out.preprocess.assign(p + 12, end != nullptr ? static_cast<size_t>(end - p - 12) : strlen(p + 12));
            }
            if ((p = strstr(line, " f=")) != nullptr) {
                const char* end = strstr(p, " pre_ms=");
                out.shape.assign(p + 1, end != nullptr ? static_cast<size_t>(end - p - 1) : strlen(p + 1));
            }
            continue;
        }
        if (strncmp(line, "RAW_SUMMARY ", 12) == 0) {
            const char* p = strstr(line, " baseline=");
            if (p != nullptr) {
                out.boardBaseline = static_cast<float>(atof(p + 10));
                out.hasBoardBaseline = true;
            }
            continue;
        }
        if (strncmp(line, "ms,", 3) == 0) {
            pcmCol = ampCol = -1;
            int field = 0;
            char* save = nullptr;
            for (char* tok = strtok_r(line, ",", &save); tok != nullptr; tok = strtok_r(nullptr, ",", &save), ++field) {
                if (strcmp(tok, "pcm") == 0) {
                    pcmCol = field;
                }
                if (strcmp(tok, "amp") == 0) {
                    ampCol = field;
                }
            }
            continue;
        }
        if (pcmCol < 0 || !(line[0] >= '0' && line[0] <= '9')) {
            continue;
        }
        char* save = nullptr;
        int col = 0;
        int32_t pcm = 0;
        int32_t amp = 0;
        bool havePcm = false;
        for (char* tok = strtok_r(line, ",", &save); tok != nullptr; tok = strtok_r(nullptr, ",", &save), ++col) {
            if (col == pcmCol) {
                pcm = static_cast<int32_t>(strtol(tok, nullptr, 10));
                havePcm = true;
            } else if (col == ampCol) {
                amp = static_cast<int32_t>(strtol(tok, nullptr, 10));
            }
        }
        if (havePcm) {
            out.pcm.push_back(pcm);
            if (ampCol >= 0) {
                out.boardAmp.push_back(amp);
            }
        }
    }
    fclose(f);
    return !out.pcm.empty();
}

bool loadCapture(const std::string& path, Capture& out) {
    out.name = std::filesystem::path(path).filename().string();
    return endsWith(path, ".csv") ? loadRawCsv(path, out) : loadWav(path, out);
}

bool applyProfileSet(detection::DetectionProfile& profile, const std::string& key, double value) {
    auto& sc = profile.scalarTransient;
    const unsigned long u = static_cast<unsigned long>(value);
    const float fv = static_cast<float>(value);
    if (key == "scalar_onset_threshold") {
        sc.onsetDetectionThreshold = fv;
    } else if (key == "scalar_release_threshold") {
        sc.onsetReleaseThreshold = fv;
    } else if (key == "scalar_min_duration_ms") {
        sc.minTransientDurationMs = u;
    } else if (key == "scalar_max_duration_ms") {
        sc.maxTransientDurationMs = u;
    } else if (key == "scalar_min_peak_strength") {
        sc.minTransientPeakStrength = fv;
    } else if (key == "scalar_release_debounce_ms") {
        sc.releaseDebounceMs = u;
    } else if (key == "scalar_cooldown_ms") {
        sc.cooldownAfterOnsetMs = u;
    } else if (key == "scalar_min_release_coverage_ms") {
        sc.minCoverageAboveReleaseMs = u;
    } else if (key == "scalar_min_longest_island_ms") {
        sc.minLongestIslandMs = u;
    } else if (key == "scalar_max_gap_ms") {
        sc.maxGapMs = u;
    } else if (key == "contrast_medium") {
        profile.inspectionPlan.modules[0].magnitude.supportStrength.mediumPeakThreshold = fv;
    } else if (key == "amp_medium") {
        profile.inspectionPlan.modules[1].magnitude.supportStrength.mediumPeakThreshold = fv;
    } else {
        return false;
    }
    return true;
}

void printVerdict(const detection::OccurrenceVerdict& v) {
    printf("    verdict type=%s reason=%s reject=%s accepted=%d valid=%d start_ms=%lu dur_ms=%lu strength=%.0f",
           detection::verdictTypeName(v.type), detection::verdictReasonName(v.reasonCode),
           detection::verdictRejectReasonName(v.rejectReason), v.accepted ? 1 : 0, v.valid ? 1 : 0,
           v.primaryStartMs, v.primaryDurationMs, static_cast<double>(v.primaryStrength));
    if (!v.accepted && v.firstFailedRequirementIndex != 255) {
        printf(" failed_target=%s observed=%s required=%s", detection::inspectionTargetName(v.firstFailedRequirementTarget),
               detection::strengthClassName(v.firstFailedObservedStrength),
               detection::strengthClassName(v.firstFailedRequiredStrength));
    }
    printf("\n");
}

Result replay(const Capture& cap, const Options& opt, const detection::DetectionProfile& profile) {
    Result r;
    VectorAudioSource source(cap.pcm, cap.sampleRateHz);
    AudioSignal audioSignal(source);
    FreqBandStream band;
    detection::DetectionRuntime runtime;
    runtime::PcmPreprocessMode preprocessMode = opt.preprocess;
    if (!opt.preprocessForced) {
        preprocessMode = cap.rawPcm() ? runtime::kPcmPreprocessMode : runtime::PcmPreprocessMode::None;
    }
    audio::PcmPreprocessor preprocessor(preprocessMode, cap.sampleRateHz);

    timebase::host::setNowUs64(0);
    audioSignal.begin(false);
    // The conditioning the Node and the Analyzer apply at boot. The baseline
    // tracker is the one state a capture does not carry per sample: a RAW
    // transcript has the board's value in RAW_SUMMARY (seeded here), a .wav
    // starts from the mean of the first 200 samples (rebase(), as the Node's
    // begin(true) does; the read moves the source's cursor only, the loop
    // below feeds every sample).
    audioSignal.setSmoothingFactor(0.5f);
    audioSignal.setBaselineUpdateFactor(0.005f);
    audioSignal.setBaselineTrackingQuietThreshold(20);
    if (cap.hasBoardBaseline) {
        audioSignal.seedBaseline(cap.boardBaseline);
    } else {
        audioSignal.rebase();
    }

    band.resetState();
    band.setSampleRateHz(cap.sampleRateHz);
    band.setTargetFrequencyHz(opt.targetHz);
    band.setBand(opt.bandLow, opt.bandHigh);

    runtime.resetState();
    runtime.setDetectorSelection(profile.detectorSelection);
    runtime.setFrequencyMatchConfig(profile.frequencyMatch);
    runtime.setScalarTransientConfig(profile.scalarTransient);
    runtime.setInspectionPlan(profile.inspectionPlan);
    runtime.setFieldStateConfig(profile.fieldStateConfig);
    runtime.setProfileName(detection::detectionProfileName(profile.kind));
    runtime.setVerdictQueueEnabled(true);

    const size_t tailSamples = static_cast<size_t>((static_cast<uint64_t>(opt.tailMs) * cap.sampleRateHz) / 1000ULL);
    const size_t total = cap.pcm.size() + tailSamples;
    double ampDiffSum = 0.0;
    for (size_t i = 0; i < total; ++i) {
        const int32_t raw = i < cap.pcm.size() ? cap.pcm[i] : 0;
        const int sample = static_cast<int>(preprocessor.process(raw));
        const uint64_t us = source.timeUs(i);
        timebase::host::setNowUs64(us);
        AudioSamplePacket packet;
        audioSignal.update(sample, us, packet, false);
        band.observeCenteredSample(packet.baselineCorrectedValue, packet.timeMs);
        const detection::FrequencyBandMeasurementPacket evidence = detection::buildFrequencyMeasurementPacket(band, packet);
        runtime.observeFrame(packet, evidence, packet.timeMs);
        if (opt.compare && i < cap.boardAmp.size()) {
            const double d = fabs(static_cast<double>(packet.audioMagnitudeValue) - static_cast<double>(cap.boardAmp[i]));
            ampDiffSum += d;
            if (d > r.ampMaxAbsDiff) {
                r.ampMaxAbsDiff = d;
            }
            ++r.ampCompared;
        }
        detection::OccurrenceVerdict verdict;
        while (runtime.popOccurrenceVerdict(verdict)) {
            if (verdict.primaryStartMs < opt.skipMs) {
                ++r.skipped;
                continue;
            }
            if (verdict.accepted) {
                ++r.accepted;
            } else {
                ++r.rejected;
            }
            r.verdicts.push_back(verdict);
            if (opt.verbose) {
                printVerdict(verdict);
            }
        }
    }
    r.samples = cap.pcm.size();
    if (r.ampCompared > 0) {
        r.ampMeanAbsDiff = ampDiffSum / static_cast<double>(r.ampCompared);
    }
    return r;
}

}  // namespace hostreplay
