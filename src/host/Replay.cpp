#include <algorithm>
#include "Replay.h"

#include <Arduino.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <filesystem>

#include <ParamRegistry.h>
#include "../app/ParamBindings.h"
#include "../app/ParamIds.h"
#include "../app/TimeBase.h"
#include "../audio/AudioSignal.h"
#include "../audio/AudioSource.h"
#include "../audio/PcmPreprocessor.h"
#include "../detection/DetectionRuntime.h"
#include "../detection/evaluation/VerdictNames.h"
#include "../detection/features/FreqBandStream.h"
#include "../detection/features/FrequencyMeasurementPacketBuilder.h"
#include "../detection/inspection/BinShape.h"
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
    int freqCol = -1;
    int contrastCol = -1;
    int freshCol = -1;
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
            if ((p = strstr(line, " band=")) != nullptr) {
                out.hasBoardBand = true;
                if (strncmp(p + 6, "off", 3) == 0) {
                    out.boardBandLow = out.boardBandHigh = 0;
                } else {
                    char* dash = nullptr;
                    out.boardBandLow = strtoul(p + 6, &dash, 10);
                    out.boardBandHigh = (dash != nullptr && *dash == '-') ? strtoul(dash + 1, nullptr, 10) : 0UL;
                }
            }
            if ((p = strstr(line, " band_lead=")) != nullptr) {
                out.hasBandLead = true;
                out.bandLead = strtoul(p + 11, nullptr, 10);
            }
            if ((p = strstr(line, " band_update_every=")) != nullptr) {
                out.bandUpdateEvery = strtoul(p + 19, nullptr, 10);
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
            pcmCol = ampCol = freqCol = contrastCol = freshCol = -1;
            int field = 0;
            char* save = nullptr;
            for (char* tok = strtok_r(line, ",", &save); tok != nullptr; tok = strtok_r(nullptr, ",", &save), ++field) {
                if (strcmp(tok, "pcm") == 0) {
                    pcmCol = field;
                }
                if (strcmp(tok, "amp") == 0) {
                    ampCol = field;
                }
                if (strcmp(tok, "freq") == 0) {
                    freqCol = field;
                }
                if (strcmp(tok, "contrast") == 0) {
                    contrastCol = field;
                }
                if (strcmp(tok, "fresh") == 0) {
                    freshCol = field;
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
        int32_t freq = 0;
        float contrast = 0.0f;
        int fresh = 0;
        bool havePcm = false;
        for (char* tok = strtok_r(line, ",", &save); tok != nullptr; tok = strtok_r(nullptr, ",", &save), ++col) {
            if (col == pcmCol) {
                pcm = static_cast<int32_t>(strtol(tok, nullptr, 10));
                havePcm = true;
            } else if (col == ampCol) {
                amp = static_cast<int32_t>(strtol(tok, nullptr, 10));
            } else if (col == freqCol) {
                freq = static_cast<int32_t>(strtol(tok, nullptr, 10));
            } else if (col == contrastCol) {
                contrast = static_cast<float>(atof(tok));
            } else if (col == freshCol) {
                fresh = static_cast<int>(strtol(tok, nullptr, 10));
            }
        }
        if (havePcm) {
            out.pcm.push_back(pcm);
            if (ampCol >= 0) {
                out.boardAmp.push_back(amp);
            }
            if (freqCol >= 0 && contrastCol >= 0) {
                out.boardFreq.push_back(freq);
                out.boardContrast.push_back(contrast);
                out.boardFresh.push_back(static_cast<uint8_t>(freshCol >= 0 ? fresh : 1));
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

const char* applyProfileSet(detection::DetectionProfile& profile, const std::string& path, const std::string& valueText) {
    param::ParamRegistry registry;
    registry.setGroupNamer(param::moduleNameForGroup);
    param::bindScalarTransientParams(registry, profile.scalarTransient);
    // The inspectors' medium limits, bound straight into the plan's module for
    // the target (the Node keeps its own copies, ResonantNodeParams.cpp).
    const size_t moduleCount = profile.inspectionPlan.count > detection::kMaxInspectionModules
                                   ? detection::kMaxInspectionModules
                                   : profile.inspectionPlan.count;
    for (size_t i = 0; i < moduleCount; ++i) {
        detection::InspectionModuleConfig& module = profile.inspectionPlan.modules[i];
        float* medium = &module.magnitude.supportStrength.mediumPeakThreshold;
        if (module.target == detection::InspectionTarget::Contrast) {
            registry.addFloat(param::ParamId::Detection_ContrastMedium, param::ModuleId::Detection,
                              "detection.contrast_medium", medium, 0.0f, 32767.0f);
        } else if (module.target == detection::InspectionTarget::Amp) {
            registry.addFloat(param::ParamId::Detection_AmpMedium, param::ModuleId::Detection,
                              "detection.amp_medium", medium, 0.0f, 32767.0f);
        } else if (module.kind == detection::InspectionModuleKind::BinShape) {
            param::bindShapeParams(registry, module.shape);
        }
    }
    const param::ParamBinding* binding = registry.find(path.c_str());
    if (binding == nullptr) {
        return param::paramSetStatusName(param::ParamSetStatus::UnknownParam);
    }
    return param::paramSetStatusName(registry.applyValue(*binding, valueText.c_str()));
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

namespace {

// The strongest band bin per millisecond over the whole capture (0 = quiet or not in band), with its target value
// and the broadband energy for the dips. A whole-capture log, not the runtime's 512 ms BinTrack: --shape also
// prints a capture's active stretch after the end. It is read through the same at() as BinTrack, by the same rule
// (BinShape.h; review D2.3, 2026-10-26: this file had its own copy of the rule, and it had drifted).
struct ReplayBinLog {
    std::vector<uint32_t> peakHz;   // per ms: the bin with the largest target value in that ms
    std::vector<float> value;       // per ms: that largest target value
    std::vector<float> energy;      // per ms: the largest total window energy (broadband)

    void note(unsigned long ms, uint32_t hz, float v, float e) {
        if (ms >= peakHz.size()) {
            peakHz.resize(ms + 1, 0);
            value.resize(ms + 1, 0.0f);
            energy.resize(ms + 1, 0.0f);
        }
        energy[ms] = std::max(energy[ms], e);
        if (v > value[ms]) {
            value[ms] = v;
            peakHz[ms] = hz;
        }
    }

    // BinTrack's reading: the bin and its level (a FrequencyScore16, stored as uint16 like the runtime's track).
    bool at(uint32_t ms, uint32_t& binHz, uint16_t& level) const {
        if (ms >= peakHz.size() || peakHz[ms] == 0) {
            return false;
        }
        binHz = peakHz[ms];
        level = static_cast<uint16_t>(std::min(value[ms], 65535.0f));
        return true;
    }
};

// The plan's shape settings (the BinShape module's), or the defaults when the plan has none.
detection::ShapeInspectionConfig shapeConfigOf(const detection::InspectionPlan& plan) {
    for (size_t i = 0; i < plan.count && i < detection::kMaxInspectionModules; ++i) {
        if (plan.modules[i].kind == detection::InspectionModuleKind::BinShape) {
            return plan.modules[i].shape;
        }
    }
    return detection::ShapeInspectionConfig{};
}

void printShapeWindow(const char* label, uint32_t start, uint32_t span, const ReplayBinLog& t, float minLevel,
                      const detection::ShapeInspectionConfig& config);

// One line per verdict, over the window the inspector reads: [start - preMs, start + duration + tailMs].
void printShape(const detection::OccurrenceVerdict& v, const ReplayBinLog& t, float minLevel,
                const detection::ShapeInspectionConfig& config) {
    printShapeWindow(v.accepted ? "accepted" : detection::verdictRejectReasonName(v.rejectReason),
                     static_cast<uint32_t>(v.primaryStartMs) - config.preMs,
                     config.preMs + static_cast<uint32_t>(v.primaryDurationMs) + config.tailMs, t, minLevel, config);
}

// The whole capture's active stretch (first to last ms above the level), for a capture with no verdict.
void printCaptureShape(const ReplayBinLog& t, float minLevel, const detection::ShapeInspectionConfig& config) {
    unsigned long first = 0;
    unsigned long last = 0;
    bool any = false;
    for (unsigned long ms = 0; ms < t.peakHz.size(); ++ms) {
        if (t.value[ms] >= minLevel && t.peakHz[ms] != 0) {
            if (!any) {
                first = ms;
            }
            last = ms;
            any = true;
        }
    }
    if (any) {
        printShapeWindow("capture", static_cast<uint32_t>(first), static_cast<uint32_t>(last - first), t, minLevel, config);
    }
}

void printShapeWindow(const char* label, uint32_t start, uint32_t span, const ReplayBinLog& t, float minLevel,
                      const detection::ShapeInspectionConfig& config) {
    const detection::BinShapeMeasure m = detection::measureBinShape(t, start, span, minLevel, config);
    const detection::BinRun* kept = m.runs;
    // The dip at each bin change: the lowest target value from the end of one run to the start of the next,
    // as a share of the smaller of the two runs' peaks. A hop train's gaps dip even when the room fills them;
    // a sweep glides from bin to bin without one. Replay-only facts (E002), not part of the rule.
    std::vector<int> dips;
    std::vector<int> edips;
    for (size_t k = 1; k < m.runCount; ++k) {
        float peakA = 0.0f;
        float peakB = 0.0f;
        for (unsigned long ms = kept[k - 1].startMs; ms < kept[k - 1].startMs + kept[k - 1].lengthMs && ms < t.value.size(); ++ms) {
            peakA = std::max(peakA, t.value[ms]);
        }
        for (unsigned long ms = kept[k].startMs; ms < kept[k].startMs + kept[k].lengthMs && ms < t.value.size(); ++ms) {
            peakB = std::max(peakB, t.value[ms]);
        }
        // the lowest point within 6 ms either side of the boundary
        const unsigned long boundary = kept[k].startMs;
        float low = peakA;
        for (unsigned long ms = boundary > 6 ? boundary - 6 : 0; ms <= boundary + 6 && ms < t.value.size(); ++ms) {
            low = std::min(low, t.value[ms]);
        }
        const float ref = std::min(peakA, peakB);
        dips.push_back(ref > 0.0f ? static_cast<int>(100.0f * low / ref + 0.5f) : -1);
        // the same on the broadband energy: a gap in the sound, not a bin change
        float eA = 0.0f;
        float eB = 0.0f;
        for (unsigned long ms = kept[k - 1].startMs; ms < kept[k - 1].startMs + kept[k - 1].lengthMs && ms < t.energy.size(); ++ms) {
            eA = std::max(eA, t.energy[ms]);
        }
        for (unsigned long ms = kept[k].startMs; ms < kept[k].startMs + kept[k].lengthMs && ms < t.energy.size(); ++ms) {
            eB = std::max(eB, t.energy[ms]);
        }
        float eLow = eA;
        for (unsigned long ms = boundary > 6 ? boundary - 6 : 0; ms <= boundary + 6 && ms < t.energy.size(); ++ms) {
            eLow = std::min(eLow, t.energy[ms]);
        }
        const float eRef = std::min(eA, eB);
        edips.push_back(eRef > 0.0f ? static_cast<int>(100.0f * eLow / eRef + 0.5f) : -1);
    }
    printf("    shape %s start_ms=%lu dur_ms=%lu active_ms=%lu runs=%zu distinct=%u up_steps=%u other_steps=%u class=%s dips_pct=",
           label, static_cast<unsigned long>(start), static_cast<unsigned long>(span),
           static_cast<unsigned long>(m.activeMs), m.runCount, static_cast<unsigned>(m.shape.distinctBins),
           static_cast<unsigned>(m.shape.risingSteps), static_cast<unsigned>(m.shape.otherSteps),
           detection::strengthClassName(m.shape.shapeClass));
    for (size_t k = 0; k < dips.size(); ++k) {
        printf("%s%d", k > 0 ? "," : "", dips[k]);
    }
    printf(" edips_pct=");
    for (size_t k = 0; k < edips.size(); ++k) {
        printf("%s%d", k > 0 ? "," : "", edips[k]);
    }
    printf(" seq=");
    for (size_t k = 0; k < m.runCount; ++k) {
        printf("%s%ux%lu", k > 0 ? "," : "", static_cast<unsigned>(kept[k].hz), static_cast<unsigned long>(kept[k].lengthMs));
        if (k + 1 < m.runCount) {
            printf("+%lu", static_cast<unsigned long>(kept[k + 1].startMs - (kept[k].startMs + kept[k].lengthMs)));
        }
    }
    printf("\n");
}

}  // namespace

Result replay(const Capture& cap, const Options& opt, const detection::DetectionProfile& profile) {
    Result r;
    ReplayBinLog track;
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
    // Same band-update phase as the board (RAW band_lead=, step 7b). The pad
    // zeros sit in the 64-sample window for its first pass, so --compare
    // skips the first window of samples.
    if (cap.hasBandLead && cap.bandUpdateEvery > 0) {
        for (unsigned long k = 0; k < cap.bandLead % cap.bandUpdateEvery; ++k) {
            band.observeCenteredSample(0, 0);
        }
    }

    runtime.resetState();
    runtime.setDetectorSelection(profile.detectorSelection);
    runtime.setScalarTransientConfig(profile.scalarTransient);
    runtime.setInspectionPlan(profile.inspectionPlan);
    runtime.setFieldStateConfig(profile.fieldStateConfig);
    runtime.setVerdictQueueEnabled(true);

    const size_t tailSamples = static_cast<size_t>((static_cast<uint64_t>(opt.tailMs) * cap.sampleRateHz) / 1000ULL);
    const size_t total = cap.pcm.size() + tailSamples;
    double ampDiffSum = 0.0;
    double freqDiffSum = 0.0;
    FILE* dump = opt.dumpFreqPath.empty() ? nullptr : fopen(opt.dumpFreqPath.c_str(), "w");
    if (dump != nullptr) {
        fprintf(dump, "i,ms,host_freq,board_freq,host_contrast,board_contrast,host_fresh,board_fresh\n");
    }
    double contrastRelSum = 0.0;
    size_t contrastCompared = 0;
    std::vector<detection::OccurrenceVerdict> pendingShape;  // printed after the loop, when the track is complete
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
        if (opt.shape && evidence.present) {
            track.note(packet.timeMs, static_cast<uint32_t>(band.lastPeakBinHz()), evidence.targetBandValue, evidence.totalEnergyValue);
        }
        if (opt.compare && i < cap.boardAmp.size()) {
            const double d = fabs(static_cast<double>(packet.audioMagnitudeValue) - static_cast<double>(cap.boardAmp[i]));
            ampDiffSum += d;
            if (d > r.ampMaxAbsDiff) {
                r.ampMaxAbsDiff = d;
            }
            ++r.ampCompared;
        }
        if (dump != nullptr && i < cap.pcm.size()) {
            fprintf(dump, "%zu,%lu,%d,%d,%.1f,%.1f,%d,%d\n", i, static_cast<unsigned long>(packet.timeMs), static_cast<int>(evidence.targetBandValue),
                    i < cap.boardFreq.size() ? cap.boardFreq[i] : -1, static_cast<double>(evidence.targetBandContrastValue),
                    i < cap.boardContrast.size() ? static_cast<double>(cap.boardContrast[i]) : -1.0, evidence.fresh ? 1 : 0,
                    i < cap.boardFresh.size() ? cap.boardFresh[i] : -1);
        }
        if (opt.compare && i >= runtime::kFreqBandWindowSamples && i < cap.boardFreq.size() && cap.boardFresh[i] != 0 && evidence.present) {
            const double fd = fabs(static_cast<double>(evidence.targetBandValue) - static_cast<double>(cap.boardFreq[i]));
            const double boardC = static_cast<double>(cap.boardContrast[i]);
            freqDiffSum += fd;
            if (fd > r.freqMaxAbsDiff) {
                r.freqMaxAbsDiff = fd;
            }
            ++r.freqCompared;
            if (fabs(boardC) >= 1000.0) {
                const double cd = fabs(static_cast<double>(evidence.targetBandContrastValue) - boardC) / fabs(boardC);
                contrastRelSum += cd;
                ++contrastCompared;
                if (cd > r.contrastMaxRelDiff) {
                    r.contrastMaxRelDiff = cd;
                }
            }
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
            if (opt.shape) {
                pendingShape.push_back(verdict);
            }
        }
    }
    r.samples = cap.pcm.size();
    for (const auto& v : pendingShape) {
        printShape(v, track, profile.scalarTransient.onsetReleaseThreshold, shapeConfigOf(profile.inspectionPlan));
    }
    if (opt.shape && pendingShape.empty()) {
        printCaptureShape(track, profile.scalarTransient.onsetReleaseThreshold, shapeConfigOf(profile.inspectionPlan));
    }
    if (r.ampCompared > 0) {
        r.ampMeanAbsDiff = ampDiffSum / static_cast<double>(r.ampCompared);
    }
    if (dump != nullptr) {
        fclose(dump);
    }
    if (r.freqCompared > 0) {
        r.freqMeanAbsDiff = freqDiffSum / static_cast<double>(r.freqCompared);
        r.contrastMeanRelDiff = contrastCompared > 0 ? contrastRelSum / static_cast<double>(contrastCompared) : 0.0;
    }
    return r;
}

}  // namespace hostreplay
