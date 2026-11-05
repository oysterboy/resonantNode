// native-echoreplay (step 20): the echo modes' detection chain on WAV files, the board's classes on the PC.
//
// A WAV (PCM 16 / 24 / 32-bit, any rate, mono or mixed to mono) is resampled to --rate, scaled to canonical PCM
// (24-bit; a 16-bit sample << 8, then --level-db), and fed per 128-sample block through EnvelopeFollower and
// EnvelopeRuntime with the echo room's plan (--plan clap, as EchoApp runs them) or Whispernet's (--plan whisper: a
// phrase's level window and syllable modulation, ECHO-002). One line per judged occurrence with --verbose, a summary
// per file. The levels a WAV reaches are not what a board's mic hears: a board's own capture (RAW mode=pcm,
// tools/bench/rawcorpus.py) gives real levels; --level-db moves a file toward them.
//
//   program [--plan clap|whisper] [--rate 48000|24000] [--level-db DB] [--onset S16] [--release S16] [--floor S16]
//           [--peak S16] [--attack MS] [--loud S16] [--mod INDEX] [--flat-min F] [--flat-max F] [--band-level]
//           [--low-max DB] [--room FILE.wav] [--out-dir DIR] [--loop-gain G] [--verbose] <file.wav>...
//
// The whisper plan's band checks (2026-11-05, as Whispernet's whisper.level_band and whisper.timbre_check run them):
// --band-level makes the level follow 1.5-4.5 kHz; --low-max DB makes the timbre module read the 200-700 Hz band's
// excess over the room's (a phrase passes at most DB) in place of the flatness range; --room FILE.wav is played before
// each file (the room tone the low band is learned on, 'silence.wav' of a labelrec session).
//
// With --plan whisper and --out-dir, each file also goes through Whispernet's delay line (GatedDelayLine, 2 s) under
// the gate, and what it would play is written as DIR/<name>.gated.wav. --loop-gain G feeds that output back into
// the input at G (the howl path: G > 1 grows by 20 log G dB per round of D), with 20 s of silence after the file to
// see whether the loop dies down; the summary gives the output's loudest second and its last 5 s.
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "../../audio/EnvelopeFollower.h"
#include "../../audio/BandBalance.h"
#include "../../audio/BandFlatness.h"
#include "../../audio/RoomLevel.h"
#include "../../audio/GatedDelayLine.h"
#include "../../behavior/WhisperGate.h"
#include "../../detection/EnvelopeRuntime.h"

namespace {

struct Wav {
    uint32_t rate = 0;
    std::vector<float> samples;  // mono, -1..1
};

uint32_t le32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
        (static_cast<uint32_t>(p[3]) << 24);
}
uint16_t le16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

bool readWav(const char* path, Wav& out) {
    FILE* f = fopen(path, "rb");
    if (f == nullptr) {
        return false;
    }
    std::vector<uint8_t> b;
    uint8_t chunk[65536];
    size_t n = 0;
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        b.insert(b.end(), chunk, chunk + n);
    }
    fclose(f);
    if (b.size() < 12 || memcmp(b.data(), "RIFF", 4) != 0 || memcmp(b.data() + 8, "WAVE", 4) != 0) {
        return false;
    }
    uint16_t format = 0, channels = 0, bits = 0;
    size_t pos = 12;
    while (pos + 8 <= b.size()) {
        const uint32_t size = le32(&b[pos + 4]);
        const size_t body = pos + 8;
        if (memcmp(&b[pos], "fmt ", 4) == 0 && body + 16 <= b.size()) {
            format = le16(&b[body]);
            channels = le16(&b[body + 2]);
            out.rate = le32(&b[body + 4]);
            bits = le16(&b[body + 14]);
        } else if (memcmp(&b[pos], "data", 4) == 0) {
            const size_t frameBytes = static_cast<size_t>(channels) * (bits / 8);
            if ((format != 1 && format != 0xFFFE) || frameBytes == 0 || (bits != 16 && bits != 24 && bits != 32)) {
                return false;
            }
            const size_t end = body + size <= b.size() ? body + size : b.size();
            for (size_t p = body; p + frameBytes <= end; p += frameBytes) {
                double sum = 0.0;
                for (uint16_t c = 0; c < channels; ++c) {
                    const uint8_t* s = &b[p + c * (bits / 8)];
                    int32_t v = 0;
                    if (bits == 16) {
                        v = static_cast<int16_t>(le16(s));
                        sum += v / 32768.0;
                    } else if (bits == 24) {
                        v = static_cast<int32_t>((static_cast<uint32_t>(s[0]) << 8) |
                                                 (static_cast<uint32_t>(s[1]) << 16) |
                                                 (static_cast<uint32_t>(s[2]) << 24)) >>
                            8;
                        sum += v / 8388608.0;
                    } else {
                        v = static_cast<int32_t>(le32(s));
                        sum += v / 2147483648.0;
                    }
                }
                out.samples.push_back(static_cast<float>(sum / channels));
            }
            return out.rate > 0;
        }
        pos = body + size + (size & 1U);
    }
    return false;
}

// 16-bit mono PCM.
bool writeWav(const char* path, uint32_t rate, const std::vector<int16_t>& samples) {
    FILE* f = fopen(path, "wb");
    if (f == nullptr) {
        return false;
    }
    const uint32_t dataBytes = static_cast<uint32_t>(samples.size() * 2U);
    uint8_t h[44] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0};
    const auto put32 = [&](size_t at, uint32_t v) {
        for (int k = 0; k < 4; ++k) {
            h[at + k] = static_cast<uint8_t>(v >> (8 * k));
        }
    };
    put32(4, 36U + dataBytes);
    put32(24, rate);
    put32(28, rate * 2U);
    h[32] = 2;
    h[34] = 16;
    memcpy(&h[36], "data", 4);
    put32(40, dataBytes);
    bool ok = fwrite(h, 1, sizeof(h), f) == sizeof(h);
    for (int16_t s : samples) {  // little-endian on every host this runs on, written byte by byte all the same
        const uint8_t b[2] = {static_cast<uint8_t>(s & 0xFF), static_cast<uint8_t>((s >> 8) & 0xFF)};
        ok = ok && fwrite(b, 1, 2, f) == 2;
    }
    fclose(f);
    return ok;
}

// Linear resampling: enough for an envelope detector (the clip quality is the board's, not this tool's).
std::vector<float> resample(const std::vector<float>& in, uint32_t from, uint32_t to) {
    if (from == to || in.empty()) {
        return in;
    }
    const size_t n = static_cast<size_t>(static_cast<double>(in.size()) * to / from);
    std::vector<float> out(n);
    for (size_t i = 0; i < n; ++i) {
        const double x = static_cast<double>(i) * from / to;
        const size_t k = static_cast<size_t>(x);
        const double f = x - k;
        const float a = in[k < in.size() ? k : in.size() - 1];
        const float c = in[k + 1 < in.size() ? k + 1 : in.size() - 1];
        out[i] = static_cast<float>(a + (c - a) * f);
    }
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    uint32_t rate = 48000;
    double levelDb = 0.0;
    float onset = 1000.0f, release = 700.0f, floorLevel = 300.0f, minPeak = 1200.0f;
    uint32_t attackMs = 10;
    bool whisper = false;
    float loudBound = 20000.0f, modIndex = 0.3f, flatMax = 0.0f;  // flatMax 0: no timbre check
    float flatMin = 0.0f, lowMaxDb = -1.0f;
    bool bandLevel = false;
    std::string roomFile;
    bool verbose = false;
    double loopGain = 0.0;
    std::string outDir;  // --plan whisper: the delay line's output per file, <name>.gated.wav
    std::vector<std::string> files;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s needs a value\n", name);
                exit(2);
            }
            return argv[++i];
        };
        if (a == "--plan") {
            whisper = std::string(next("--plan")) == "whisper";
        } else if (a == "--loud") {
            loudBound = static_cast<float>(atof(next("--loud")));
        } else if (a == "--flat-max") {
            flatMax = static_cast<float>(atof(next("--flat-max")));
        } else if (a == "--flat-min") {
            flatMin = static_cast<float>(atof(next("--flat-min")));
        } else if (a == "--band-level") {
            bandLevel = true;
        } else if (a == "--low-max") {
            lowMaxDb = static_cast<float>(atof(next("--low-max")));
        } else if (a == "--room") {
            roomFile = next("--room");
        } else if (a == "--mod") {
            modIndex = static_cast<float>(atof(next("--mod")));
        } else if (a == "--out-dir") {
            outDir = next("--out-dir");
        } else if (a == "--loop-gain") {
            loopGain = atof(next("--loop-gain"));
        } else if (a == "--rate") {
            rate = static_cast<uint32_t>(strtoul(next("--rate"), nullptr, 10));
        } else if (a == "--level-db") {
            levelDb = atof(next("--level-db"));
        } else if (a == "--onset") {
            onset = static_cast<float>(atof(next("--onset")));
        } else if (a == "--release") {
            release = static_cast<float>(atof(next("--release")));
        } else if (a == "--floor") {
            floorLevel = static_cast<float>(atof(next("--floor")));
        } else if (a == "--peak") {
            minPeak = static_cast<float>(atof(next("--peak")));
        } else if (a == "--attack") {
            attackMs = static_cast<uint32_t>(strtoul(next("--attack"), nullptr, 10));
        } else if (a == "--verbose") {
            verbose = true;
        } else if (a == "--help" || a[0] == '-') {
            fprintf(
                stderr,
                "usage: %s [--plan clap|whisper] [--rate 48000|24000] [--level-db DB] [--onset S16] "
                "[--release S16] [--floor S16] [--peak S16] [--attack MS] [--loud S16] [--mod INDEX] [--flat-max F] "
                "[--out-dir DIR] [--loop-gain G] [--verbose] <file.wav>...\n",
                argv[0]);
            return 2;
        } else {
            files.push_back(a);
        }
    }

    // The echo room's chain, as EchoApp::configureEcho sets it.
    detection::ScalarTransientConfig d;
    d.observedStream = detection::FeatureStreamId::AmpEnvelope;
    d.onsetDetectionThreshold = onset;
    d.onsetReleaseThreshold = release;
    d.cooldownAfterOnsetMs = 50;
    d.maxTransientDurationMs = 300;
    d.minTransientPeakStrength = minPeak;
    d.releaseDebounceMs = 20;
    detection::InspectionPlan plan = detection::InspectionPlan{};
    detection::InspectionModuleConfig& m = plan.modules[0];
    m.kind = detection::InspectionModuleKind::AttackRise;
    m.target = detection::InspectionTarget::Attack;
    m.enabled = true;
    m.minimumStrength = detection::StrengthClass::Medium;
    m.attack.floorLevel = floorLevel;
    m.attack.mediumMaxMs = attackMs;
    m.attack.strongMaxMs = attackMs / 2;
    m.attack.weakMaxMs = attackMs * 3;
    plan.count = 1;
    if (whisper) {
        // A phrase is one occurrence: syllable gaps bridged by the release debounce, no length limit (the provisional
        // inspection of an open candidate comes later). Defaults sized on the synthetic set (synthwhisper.py).
        onset = onset == 1000.0f ? 1500.0f : onset;
        release = release == 700.0f ? 1000.0f : release;
        floorLevel = floorLevel == 300.0f ? 1000.0f : floorLevel;
        d.onsetDetectionThreshold = onset;
        d.onsetReleaseThreshold = release;
        d.cooldownAfterOnsetMs = 0;
        d.maxTransientDurationMs = 0;
        d.minTransientDurationMs = 300;
        d.minTransientPeakStrength = 0.0f;
        d.releaseDebounceMs = 300;
        plan = detection::InspectionPlan{};
        detection::InspectionModuleConfig& lw = plan.modules[0];
        lw.kind = detection::InspectionModuleKind::LevelWindow;
        lw.target = detection::InspectionTarget::Loudness;
        lw.enabled = true;
        lw.minimumStrength = detection::StrengthClass::Strong;
        lw.level.floorLevel = floorLevel;
        lw.level.loudBound = loudBound;
        detection::InspectionModuleConfig& mo = plan.modules[1];
        mo.kind = detection::InspectionModuleKind::Modulation;
        mo.target = detection::InspectionTarget::Modulation;
        mo.enabled = true;
        mo.minimumStrength = detection::StrengthClass::Medium;
        mo.modulation.mediumIndex = modIndex;
        plan.count = 2;
        if (flatMax > 0.0f || lowMaxDb >= 0.0f) {  // the timbre, as Whispernet's shell runs it
            detection::InspectionModuleConfig& fl = plan.modules[2];
            fl.kind = detection::InspectionModuleKind::Flatness;
            fl.target = detection::InspectionTarget::Timbre;
            fl.enabled = true;
            fl.minimumStrength = detection::StrengthClass::Strong;
            if (lowMaxDb >= 0.0f) {  // the low band's excess over the room, at most lowMaxDb
                fl.flatness.stream = detection::FeatureStreamId::LowBandExcess;
                fl.flatness.minFlatness = -100.0f;
                fl.flatness.maxFlatness = lowMaxDb;
            } else {
                fl.flatness.minFlatness = flatMin;
                fl.flatness.maxFlatness = flatMax;
            }
            plan.count = 3;
        }
    }

    Wav roomWav;
    if (!roomFile.empty() && !readWav(roomFile.c_str(), roomWav)) {
        fprintf(stderr, "--room: not a PCM WAV: %s\n", roomFile.c_str());
        return 2;
    }
    static detection::EnvelopeRuntime runtime;  // its history is large: not on the stack
    const double scale = 8388607.0 * pow(10.0, levelDb / 20.0);
    const size_t kBlock = 128;
    for (const std::string& path : files) {
        Wav wav;
        if (!readWav(path.c_str(), wav)) {
            printf("%s: not a PCM WAV\n", path.c_str());
            continue;
        }
        std::vector<float> x = resample(wav.samples, wav.rate, rate);
        if (!roomWav.samples.empty()) {  // the room tone first: the low band's room level is learned on it
            const std::vector<float> r = resample(roomWav.samples, roomWav.rate, rate);
            x.insert(x.begin(), r.begin(), r.end());
        }
        x.resize(x.size() + rate / 2, 0.0f);  // 500 ms of silence so the last occurrence closes
        // Whispernet's output: the delay line under the gate below, written to --out-dir to be listened to.
        static constexpr unsigned long kDelayMs = 2000;  // Whispernet's delay line at 24 kHz
        if (whisper) {
            x.resize(x.size() + static_cast<size_t>(rate) * kDelayMs / 1000U, 0.0f);  // the line's tail plays out
        }
        if (whisper && loopGain > 0.0) {
            x.resize(x.size() + static_cast<size_t>(rate) * 20U, 0.0f);  // 20 s more: does the loop die down?
        }
        GatedDelayLine line;
        std::vector<int16_t> played;
        int16_t lastOut = 0;
        if (whisper && (!outDir.empty() || loopGain > 0.0) &&
            !line.begin(static_cast<size_t>(rate) * kDelayMs / 1000U, kBlock, static_cast<size_t>(rate) / 100U)) {
            printf("%s: no memory for the delay line\n", path.c_str());
            continue;
        }
        const auto sampleAtMs = [&](unsigned long ms) { return static_cast<uint64_t>(ms) * rate / 1000U; };
        runtime.configure(d, plan);
        runtime.setProvisional(whisper ? 300 : 0, 100);
        runtime.reset();
        EnvelopeFollower follower;
        BandFlatness flatness;
        flatness.begin(rate);
        BandBalance balance;
        balance.begin(rate);
        RoomLevel lowRoom;
        lowRoom.setTimes(10000, 500);  // Whispernet's defaults (whisper.room_rise_ms / room_fall_ms)
        uint32_t lastLowMs = 0;
        follower.setShift(EnvelopeFollower::shiftForRate(rate));
        size_t occurrences = 0, valid = 0, slow = 0, unmeasured = 0, outOfLevel = 0, flat = 0;
        double validMs = 0.0, judgedMs = 0.0;
        WhisperGate gate;
        gate.setDelayMs(kDelayMs);
        bool phraseOpened = false;
        unsigned long openedPhraseMs = 0, gateFromMs = 0;
        size_t phrasesOpened = 0, opens = 0;
        double gatedMs = 0.0, latencyMs = 0.0;
        // A gate step into the delay line and the counts; durationMs: how far into its phrase the gate opened.
        const auto apply = [&](const WhisperGate::Step& step, unsigned long durationMs) {
            if (step.event == WhisperGate::Event::Open) {
                gateFromMs = step.fromMs;
                if (line.delay() > 0) {
                    line.pass(sampleAtMs(gateFromMs), line.written());
                    line.setOpen(true);
                }
                ++opens;
                phraseOpened = phraseOpened && openedPhraseMs == gate.phraseStartMs();
                if (!phraseOpened) {  // the time to the first opening, per phrase
                    phraseOpened = true;
                    openedPhraseMs = gate.phraseStartMs();
                    ++phrasesOpened;
                    latencyMs += durationMs;
                }
            } else if (step.event == WhisperGate::Event::Close) {
                line.setOpen(false);
                gatedMs += step.fromMs - gateFromMs;
                if (verbose) {
                    printf("  gate_ms=%lu..%lu\n", gateFromMs, step.fromMs);
                }
            }
        };
        float peakLevel = 0.0f;
        for (size_t start = 0; start + kBlock <= x.size(); start += kBlock) {
            for (size_t i = start; i < start + kBlock; ++i) {
                // --loop-gain: the own replay back into the mic (the howl path, one sample late on top of D).
                const double heard = x[i] + loopGain * lastOut / 32768.0;
                double v = heard * scale;
                v = v > 8388607.0 ? 8388607.0 : (v < -8388608.0 ? -8388608.0 : v);
                const float mid = balance.push(static_cast<float>(v));
                follower.push(bandLevel ? static_cast<int32_t>(mid) : static_cast<int32_t>(v));
                flatness.push(static_cast<float>(v));
                if (line.delay() > 0) {
                    const double s = heard * 32767.0;
                    lastOut = line.process(static_cast<int16_t>(s > 32767.0 ? 32767.0 : (s < -32768.0 ? -32768.0 : s)));
                    played.push_back(lastOut);
                }
            }
            const uint64_t blockUs = static_cast<uint64_t>(start) * 1000000ULL / rate;
            const float level = follower.strength();
            peakLevel = level > peakLevel ? level : peakLevel;
            float flat = 0.0f;
            if (flatness.take(flat) && whisper && flatMax > 0.0f && level >= onset) {
                runtime.recordFeature(detection::FeatureStreamId::SpectralFlatness,
                                      static_cast<uint32_t>(blockUs / 1000U), flat);
            }
            if (whisper && lowMaxDb >= 0.0f) {
                float lowRms = 0.0f;
                const uint32_t ms = static_cast<uint32_t>(blockUs / 1000U);
                if (balance.takeLowRms(lowRms)) {
                    lowRoom.update(lowRms, ms > lastLowMs ? ms - lastLowMs : 0U, !runtime.candidateOpen());
                    lastLowMs = ms;
                    if (level >= onset && lowRoom.started()) {
                        runtime.recordFeature(detection::FeatureStreamId::LowBandExcess, ms,
                                              20.0f * log10f((lowRms + 1.0f) / (lowRoom.value() + 1.0f)));
                    }
                }
            } else {
                float lowRms = 0.0f;
                balance.takeLowRms(lowRms);
            }
            runtime.onBlock(level, blockUs, static_cast<uint32_t>(blockUs / 1000U));
            detection::EnvelopeRuntime::Result r;
            while (runtime.popResult(r)) {
                const detection::Occurrence& oc = r.inspected.occurrence;
                const detection::OccurrenceAttack& at = oc.attack;
                if (r.provisional) {
                    // The gate as the shell runs it (WhisperGate); every phrase is answered here (no Behavior).
                    const unsigned long nowMs = static_cast<unsigned long>(blockUs / 1000U);
                    const bool tooLoud = oc.level.measured && oc.level.p90 > loudBound;
                    WhisperGate::Step step = gate.onVerdict(oc.startMs, r.verdict.valid, tooLoud, nowMs);
                    if (step.event == WhisperGate::Event::Ask) {
                        step = gate.decide(true);
                    }
                    apply(step, oc.durationMs);
                    continue;
                }
                ++occurrences;
                if (whisper) {
                    apply(gate.onPhraseEnd(oc.startMs + oc.durationMs), 0);
                    judgedMs += oc.durationMs;
                    if (r.verdict.valid) {
                        ++valid;
                        validMs += oc.durationMs;
                    } else if (oc.level.levelClass == detection::StrengthClass::None) {
                        ++outOfLevel;
                    } else {
                        ++flat;
                    }
                    if (verbose) {
                        printf("  t_ms=%lu dur_ms=%lu peak=%.0f level_p90=%.0f mod=%.2f%s flat=%.2f%s valid=%d\n",
                               static_cast<unsigned long>(oc.startMs), static_cast<unsigned long>(oc.durationMs),
                               oc.strength, oc.level.p90, oc.modulation.index, oc.modulation.measured ? "" : "(na)",
                               oc.timbre.flatness, oc.timbre.measured ? "" : "(na)", r.verdict.valid ? 1 : 0);
                    }
                    continue;
                }
                if (r.verdict.valid) {
                    ++valid;
                } else if (at.measured) {
                    ++slow;
                } else {
                    ++unmeasured;
                }
                if (verbose) {
                    printf("  t_ms=%lu dur_ms=%lu strength=%.0f attack_ms=%s%lu valid=%d\n",
                           static_cast<unsigned long>(r.inspected.occurrence.startMs),
                           static_cast<unsigned long>(r.verdict.primaryDurationMs), r.verdict.primaryStrength,
                           at.measured ? "" : "na/", static_cast<unsigned long>(at.riseMs), r.verdict.valid ? 1 : 0);
                }
            }
            if (gate.open() && !runtime.candidateOpen()) {  // the detector dropped the phrase without accepting it
                apply(gate.onPhraseEnd(static_cast<unsigned long>(blockUs / 1000U)), 0);
            }
        }
        if (whisper) {
            printf("%-40s rate=%lu len_s=%.1f peak_level=%.0f phrases=%zu whispers=%zu (%.1f of %.1f s) "
                   "out_of_level=%zu flat=%zu gate_opens=%zu gated_s=%.1f open_after_ms=%.0f\n",
                   path.c_str(), static_cast<unsigned long>(rate), static_cast<double>(x.size()) / rate, peakLevel,
                   occurrences, valid, validMs / 1000.0, judgedMs / 1000.0, outOfLevel, flat, opens, gatedMs / 1000.0,
                   phrasesOpened > 0 ? latencyMs / phrasesOpened : 0.0);
            if (loopGain > 0.0 && !played.empty()) {
                // The output's RMS per second: the loudest, and the last 5 s (0 when the loop died down).
                double loudest = 0.0, tail = 0.0;
                const size_t sec = rate;
                for (size_t k = 0; k + sec <= played.size(); k += sec) {
                    double sum = 0.0;
                    for (size_t j = k; j < k + sec; ++j) {
                        sum += static_cast<double>(played[j]) * played[j];
                    }
                    loudest = sum > loudest ? sum : loudest;
                }
                const size_t tailFrom = played.size() > 5 * sec ? played.size() - 5 * sec : 0;
                for (size_t j = tailFrom; j < played.size(); ++j) {
                    tail += static_cast<double>(played[j]) * played[j];
                }
                const auto dbfs = [](double meanSquare) {
                    return 10.0 * log10(meanSquare / (32768.0 * 32768.0) + 1e-18);
                };
                printf("  loop_gain=%.2f out_loudest_s_dbfs=%.1f out_last5s_dbfs=%.1f\n", loopGain, dbfs(loudest / sec),
                       dbfs(tail / static_cast<double>(played.size() - tailFrom)));
            }
            if (!played.empty() && !outDir.empty()) {
                const size_t slash = path.find_last_of("/\\");
                std::string name = path.substr(slash == std::string::npos ? 0 : slash + 1);
                name = outDir + "/" + name.substr(0, name.rfind('.')) + ".gated.wav";
                if (!writeWav(name.c_str(), rate, played)) {
                    printf("%s: not written\n", name.c_str());
                }
            }
            continue;
        }
        printf("%-40s rate=%lu len_s=%.1f peak_level=%.0f occurrences=%zu claps=%zu slow=%zu unmeasured=%zu\n",
               path.c_str(), static_cast<unsigned long>(rate), static_cast<double>(x.size()) / rate, peakLevel,
               occurrences, valid, slow, unmeasured);
    }
    return 0;
}
