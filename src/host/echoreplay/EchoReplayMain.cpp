// native-echoreplay (step 20): the echo modes' detection chain on WAV files, the board's classes on the PC.
//
// A WAV (PCM 16 / 24 / 32-bit, any rate, mono or mixed to mono) is resampled to --rate, scaled to canonical PCM
// (24-bit; a 16-bit sample << 8, then --level-db), and fed per 128-sample block through EnvelopeFollower and
// EnvelopeRuntime with the echo room's plan, as EchoApp runs them. One line per judged occurrence with --verbose, a
// summary per file. The levels a WAV reaches are not what a board's mic hears: a board's own capture (RAW mode=pcm,
// tools/bench/rawcorpus.py) gives real levels; --level-db moves a file toward them.
//
//   program [--rate 48000|24000] [--level-db DB] [--onset S16] [--release S16] [--floor S16] [--peak S16]
//           [--attack MS] [--verbose] <file.wav>...
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "../../audio/EnvelopeFollower.h"
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
    bool verbose = false;
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
        if (a == "--rate") {
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
            fprintf(stderr,
                    "usage: %s [--rate 48000|24000] [--level-db DB] [--onset S16] [--release S16] [--floor S16] "
                    "[--peak S16] [--attack MS] [--verbose] <file.wav>...\n",
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
        x.resize(x.size() + rate / 2, 0.0f);  // 500 ms of silence so the last occurrence closes
        runtime.configure(d, plan);
        runtime.reset();
        EnvelopeFollower follower;
        follower.setShift(EnvelopeFollower::shiftForRate(rate));
        size_t occurrences = 0, valid = 0, slow = 0, unmeasured = 0;
        float peakLevel = 0.0f;
        for (size_t start = 0; start + kBlock <= x.size(); start += kBlock) {
            for (size_t i = start; i < start + kBlock; ++i) {
                double v = x[i] * scale;
                v = v > 8388607.0 ? 8388607.0 : (v < -8388608.0 ? -8388608.0 : v);
                follower.push(static_cast<int32_t>(v));
            }
            const uint64_t blockUs = static_cast<uint64_t>(start) * 1000000ULL / rate;
            const float level = follower.strength();
            peakLevel = level > peakLevel ? level : peakLevel;
            runtime.onBlock(level, blockUs, static_cast<uint32_t>(blockUs / 1000U));
            detection::EnvelopeRuntime::Result r;
            while (runtime.popResult(r)) {
                ++occurrences;
                const detection::OccurrenceAttack& at = r.inspected.occurrence.attack;
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
        }
        printf("%-40s rate=%lu len_s=%.1f peak_level=%.0f occurrences=%zu claps=%zu slow=%zu unmeasured=%zu\n",
               path.c_str(), static_cast<unsigned long>(rate), static_cast<double>(x.size()) / rate, peakLevel,
               occurrences, valid, slow, unmeasured);
    }
    return 0;
}
