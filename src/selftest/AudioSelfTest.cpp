#include "AudioSelfTest.h"

#include <math.h>

#include "../app/RuntimeDefaults.h"

namespace selftest {

namespace {

constexpr size_t kEnvelopeTaps = 6;
constexpr uint32_t kAmpLeadUs = 100000;       // quiet before toneOn
constexpr uint32_t kAmpFloorFromUs = 20000;   // floor window: start + 20 ms ...
constexpr uint32_t kAmpFloorGuardUs = 5000;   // ... to toneOn - 5 ms
constexpr uint32_t kAmpToneMs = 100;
constexpr uint32_t kAmpPlateauFromUs = 50000; // plateau window: toneOn + 50 ms ...
constexpr uint32_t kAmpPlateauToUs = 90000;   // ... to toneOn + 90 ms
constexpr uint32_t kAmpTailUs = 200000;
constexpr uint32_t kAmpGapMs = 1000;

// Moving max of |x| over the last kEnvelopeTaps samples.
class Envelope {
public:
    float push(int32_t x) {
        _taps[_next] = x < 0 ? -static_cast<int64_t>(x) : static_cast<int64_t>(x);
        _next = (_next + 1) % kEnvelopeTaps;
        int64_t m = 0;
        for (size_t i = 0; i < kEnvelopeTaps; ++i) {
            if (_taps[i] > m) {
                m = _taps[i];
            }
        }
        return static_cast<float>(m);
    }

private:
    int64_t _taps[kEnvelopeTaps] = {};
    size_t _next = 0;
};

bool inWindow(uint32_t t, uint32_t from, uint32_t to) {
    return static_cast<int32_t>(t - from) >= 0 && static_cast<int32_t>(t - to) < 0;
}

float median(float* values, size_t count) {
    for (size_t i = 1; i < count; ++i) {
        const float v = values[i];
        size_t j = i;
        while (j > 0 && values[j - 1] > v) {
            values[j] = values[j - 1];
            --j;
        }
        values[j] = v;
    }
    if (count == 0) {
        return 0.0f;
    }
    return (count % 2 == 1) ? values[count / 2] : 0.5f * (values[count / 2 - 1] + values[count / 2]);
}

struct AmpTrial {
    float floor = 0.0f;
    float tone = 0.0f;
    float snrDb = 0.0f;
    uint32_t samples = 0;
    bool valid = false;
};

AmpTrial runAmpTrial(AudioSourceI2S& source, ToneOutput& tone) {
    AmpTrial trial;
    drainSource(source, 0);
    Envelope envelope;
    const uint32_t rateHz = source.sampleRateHz() > 0 ? source.sampleRateHz() : 16000U;
    const uint32_t start = micros();
    const uint32_t t0 = start + kAmpLeadUs;
    const uint32_t t1 = t0 + kAmpToneMs * 1000U;
    const uint32_t stop = t1 + kAmpTailUs;
    bool on = false;
    bool off = false;
    double plateauSum = 0.0;
    uint32_t plateauCount = 0;

    while (static_cast<int32_t>(micros() - stop) < 0) {
        const uint32_t now = micros();
        if (!on && static_cast<int32_t>(now - t0) >= 0) {
            tone.toneOn();
            on = true;
        }
        if (on && !off && static_cast<int32_t>(now - t1) >= 0) {
            tone.toneOff();
            off = true;
        }
        AudioBlock block;
        if (!source.readBlock(block) || block.samples == nullptr || block.sampleCount == 0) {
            delay(1);
            continue;
        }
        for (uint16_t i = 0; i < block.sampleCount; ++i) {
            const uint32_t t = block.approxStartMicros
                + static_cast<uint32_t>((static_cast<uint64_t>(i) * 1000000ULL) / rateHz);
            const float env = envelope.push(block.samples[i]);
            ++trial.samples;
            if (inWindow(t, start + kAmpFloorFromUs, t0 - kAmpFloorGuardUs)) {
                if (env > trial.floor) {
                    trial.floor = env;
                }
            } else if (inWindow(t, t0 + kAmpPlateauFromUs, t0 + kAmpPlateauToUs)) {
                plateauSum += env;
                ++plateauCount;
            }
        }
    }
    if (!off) {
        tone.toneOff();
    }
    trial.tone = plateauCount > 0 ? static_cast<float>(plateauSum / plateauCount) : 0.0f;
    trial.valid = plateauCount > 0;
    trial.snrDb = 20.0f * log10f((trial.tone + 1.0f) / (trial.floor + 1.0f));
    return trial;
}

} // namespace

void drainSource(AudioSourceI2S& source, uint32_t ms) {
    const uint32_t until = millis() + ms;
    AudioBlock block;
    for (;;) {
        while (source.readBlock(block)) {
        }
        if (static_cast<int32_t>(millis() - until) >= 0) {
            return;
        }
        delay(1);
    }
}

Result runMicCheck(Print& out, Tally& tally, AudioSourceI2S& source, uint32_t captureMs) {
    drainSource(source, 0);
    const uint32_t rateHz = source.sampleRateHz() > 0 ? source.sampleRateHz() : 16000U;
    const uint32_t target = (rateHz * captureMs) / 1000U;
    const uint32_t deadline = millis() + 2U * captureMs + 200U;

    uint32_t frames = 0;
    uint32_t nonZero = 0;
    uint32_t bit8Ones = 0;
    int32_t minValue = INT32_MAX;
    int32_t maxValue = INT32_MIN;
    int32_t previous = 0;
    bool havePrevious = false;
    Envelope envelope;
    double floorSum = 0.0;
    uint32_t floorCount = 0;
    float floorPeak = 0.0f;

    while (frames < target && static_cast<int32_t>(millis() - deadline) < 0) {
        int sample = 0;
        uint32_t sampleTimeUs = 0;
        if (!source.readRawSample(sample, sampleTimeUs)) {
            delay(1);
            continue;
        }
#ifdef RAW_I2S_UNDECODED
        // Diagnostic build: readRawSample hands over the undecoded slot word.
        const int32_t decoded = static_cast<int32_t>(sample) >> 8;
#else
        // Decoded = slot word >> 8, so its bit 0 is the raw word's bit 8.
        const int32_t decoded = static_cast<int32_t>(sample);
#endif
        ++frames;
        if (decoded != 0) {
            ++nonZero;
        }
        if ((decoded & 1) != 0) {
            ++bit8Ones;
        }
        if (decoded < minValue) {
            minValue = decoded;
        }
        if (decoded > maxValue) {
            maxValue = decoded;
        }
        if (havePrevious) {
            // Same First Difference as AudioSourceI2S::preprocessSample().
            const int32_t fd = static_cast<int32_t>((static_cast<int64_t>(decoded) - previous) / 2);
            const float env = envelope.push(fd);
            if (frames > kEnvelopeTaps) {
                floorSum += env;
                ++floorCount;
                if (env > floorPeak) {
                    floorPeak = env;
                }
            }
        }
        previous = decoded;
        havePrevious = true;
    }

    const float nonZeroPct = frames > 0 ? 100.0f * static_cast<float>(nonZero) / static_cast<float>(frames) : 0.0f;
    const float bit8Pct = frames > 0 ? 100.0f * static_cast<float>(bit8Ones) / static_cast<float>(frames) : 0.0f;
    const long span = frames > 0 ? static_cast<long>(static_cast<int64_t>(maxValue) - minValue) : 0L;
    const float floorMean = floorCount > 0 ? static_cast<float>(floorSum / floorCount) : 0.0f;
#if I2S_RX_MSB_ALIGN
    const bool framingJudged = true;
#else
    const bool framingJudged = false;
#endif
    const bool framingOk = bit8Pct >= kMicMinBit8Pct && bit8Pct <= kMicMaxBit8Pct;

    const char* reason = nullptr;
    if (frames < target / 2U) {
        reason = "no_data";
    } else if (nonZeroPct < kMicMinNonZeroPct) {
        reason = "all_zero";
    } else if (span < kMicMinRawSpan) {
        reason = "stuck";
    } else if (floorMean < 1.0f) {
        reason = "zero_floor";
    } else if (floorMean > kMicMaxFloor) {
        reason = "floor_high";
    } else if (framingJudged && !framingOk) {
        reason = "framing_bit8";
    }

    const Result result = reason == nullptr ? Result::Pass : Result::Fail;
    begin(out, tally, "mic", result);
    if (reason != nullptr) {
        field(out, "reason", reason);
    }
    field(out, "frames", static_cast<unsigned long>(frames));
    field(out, "nonzero_pct", nonZeroPct, 1);
    field(out, "span", span);
    field(out, "floor_mean", floorMean, 0);
    field(out, "floor_peak", floorPeak, 0);
    field(out, "bit8_pct", bit8Pct, 1);
    field(out, "framing", framingJudged ? (framingOk ? "ok" : "bad") : "not_judged");
    field(out, "slot", static_cast<long>(AUDIO_I2S_MIC_SLOT));
    end(out);
    return result;
}

Result runAmpCheck(Print& out, Tally& tally, AudioSourceI2S& source, ToneOutput& tone,
                   uint32_t toneHz, const char* path, uint8_t chirps) {
    if (chirps == 0) {
        chirps = 1;
    }
    if (chirps > kMaxChirps) {
        chirps = kMaxChirps;
    }
    float snr[kMaxChirps];
    float tones[kMaxChirps];
    float floors[kMaxChirps];
    size_t valid = 0;
    float snrMin = 0.0f;
    float snrMax = 0.0f;

    tone.toneOff();
    tone.setToneHz(toneHz);
    drainSource(source, 200);
    for (uint8_t n = 1; n <= chirps; ++n) {
        const AmpTrial trial = runAmpTrial(source, tone);
        out.print("SELFTEST_AMP n=");
        out.print(n);
        out.print(" floor=");
        out.print(trial.floor, 0);
        out.print(" tone=");
        out.print(trial.tone, 0);
        out.print(" snr_db=");
        out.print(trial.snrDb, 1);
        out.print(" samples=");
        out.print(trial.samples);
        out.print(" valid=");
        out.println(trial.valid ? 1 : 0);
        if (trial.valid) {
            if (valid == 0 || trial.snrDb < snrMin) {
                snrMin = trial.snrDb;
            }
            if (valid == 0 || trial.snrDb > snrMax) {
                snrMax = trial.snrDb;
            }
            snr[valid] = trial.snrDb;
            tones[valid] = trial.tone;
            floors[valid] = trial.floor;
            ++valid;
        }
        if (n < chirps) {
            drainSource(source, kAmpGapMs);
        }
    }
    tone.toneOff();

    const float snrMedian = median(snr, valid);
    const float toneMedian = median(tones, valid);
    const float floorMedian = median(floors, valid);
    const char* reason = nullptr;
    if (valid == 0) {
        reason = "no_data";
    } else if (snrMedian < kAmpPassSnrDb) {
        reason = "low_snr";
    }
    const Result result = reason == nullptr ? Result::Pass : Result::Fail;
    begin(out, tally, "amp", result);
    if (reason != nullptr) {
        field(out, "reason", reason);
    }
    field(out, "chirps", static_cast<unsigned long>(chirps));
    field(out, "valid", static_cast<unsigned long>(valid));
    field(out, "freq_hz", static_cast<unsigned long>(toneHz));
    field(out, "tone_ms", static_cast<unsigned long>(kAmpToneMs));
    field(out, "path", path);
    field(out, "tone", toneMedian, 0);
    field(out, "floor", floorMedian, 0);
    field(out, "snr_db", snrMedian, 1);
    field(out, "snr_min_db", snrMin, 1);
    field(out, "snr_max_db", snrMax, 1);
    field(out, "pass_db", kAmpPassSnrDb, 1);
    end(out);
    return result;
}

void emitChirpsBlocking(Print& out, AudioSourceI2S* source, ToneOutput& tone, uint32_t toneHz,
                        uint8_t n, uint32_t durMs, uint32_t gapMs) {
    out.print("SELFTEST_CHIRP start n=");
    out.print(n);
    out.print(" freq_hz=");
    out.print(toneHz);
    out.print(" dur_ms=");
    out.print(durMs);
    out.print(" gap_ms=");
    out.println(gapMs);
    tone.toneOff();
    tone.setToneHz(toneHz);
    for (uint8_t i = 0; i < n; ++i) {
        tone.toneOn();
        if (source != nullptr) {
            drainSource(*source, durMs);
        } else {
            delay(durMs);
        }
        tone.toneOff();
        if (source != nullptr) {
            drainSource(*source, gapMs);
        } else {
            delay(gapMs);
        }
    }
    out.print("SELFTEST_CHIRP done n=");
    out.print(n);
    out.print(" started=");
    out.println(n);
}

} // namespace selftest
