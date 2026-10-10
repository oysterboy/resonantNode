// Throwaway (issue #19): toneOn -> own-mic latency on one D-AMP board, using
// the repo's AudioSourceI2S + I2sToneOutput unchanged (copied from main).
// Per trial: 100 ms quiet, toneOn at T0, toneOff at T0+100 ms, 200 ms tail.
// Mic sample times come from AudioSourceI2S's sample clock (block stamps),
// the same time base the detector uses. Envelope = moving max of |x| over
// 6 samples of the First-Differenced stream (one 3200 Hz period is 5).
#include <Arduino.h>

#include "repo/hal/AudioSourceI2S.h"
#include "repo/hal/I2sToneOutput.h"

static AudioSourceI2S src(runtime::kDefaultAudioI2SSckPin, runtime::kDefaultAudioI2SWsPin,
                          runtime::kDefaultAudioI2SDataPin,
                          static_cast<int>(runtime::kDefaultAudioI2SSampleRateHz),
                          static_cast<int>(runtime::kDefaultAudioI2SBitsPerSample),
                          runtime::kDefaultAudioI2SDataOutPin);
static I2sToneOutput gTone(src);

static constexpr size_t kMax = 16000 * 45 / 100;  // 450 ms
static int32_t gX[kMax];
static uint32_t gT[kMax];
static size_t gN = 0;

static void collectUntil(uint32_t untilUs) {
    while ((int32_t)(micros() - untilUs) < 0) {
        AudioBlock b;
        if (src.readBlock(b)) {
            for (uint16_t i = 0; i < b.sampleCount && gN < kMax; ++i) {
                gX[gN] = b.samples[i];
                gT[gN] = b.approxStartMicros + (uint32_t)((uint64_t)i * 1000000ULL / 16000ULL);
                ++gN;
            }
        } else {
            delay(1);
        }
    }
}

static float envAt(size_t i) {
    int32_t m = 0;
    for (size_t k = (i >= 5 ? i - 5 : 0); k <= i; ++k) {
        const int32_t a = gX[k] < 0 ? -gX[k] : gX[k];
        if (a > m) m = a;
    }
    return (float)m;
}

static void trial(int n) {
    gN = 0;
    const uint32_t start = micros();
    collectUntil(start + 100000);
    const uint32_t t0 = micros();
    gTone.toneOn();
    collectUntil(t0 + 100000);
    const uint32_t t1 = micros();
    gTone.toneOff();
    collectUntil(t1 + 200000);
    // Drain anything still queued so the tail is complete.
    collectUntil(micros() + 30000);

    float pre = 0;
    for (size_t i = 0; i < gN; ++i) {
        if ((int32_t)(gT[i] - (start + 20000)) >= 0 && (int32_t)(gT[i] - (t0 - 5000)) < 0) pre = fmaxf(pre, envAt(i));
    }
    float plateau = 0;
    size_t cnt = 0;
    double acc = 0;
    for (size_t i = 0; i < gN; ++i) {
        if ((int32_t)(gT[i] - (t0 + 50000)) >= 0 && (int32_t)(gT[i] - (t0 + 90000)) < 0) { acc += envAt(i); ++cnt; }
    }
    plateau = cnt ? (float)(acc / cnt) : 0;
    long on10 = -1, on50 = -1, off10 = -1;
    for (size_t i = 0; i < gN; ++i) {
        const int32_t dt0 = (int32_t)(gT[i] - t0);
        const float e = envAt(i);
        if (dt0 >= -5000 && on10 < 0 && e >= 0.1f * plateau && e > 2 * pre) on10 = dt0;
        if (dt0 >= -5000 && on50 < 0 && e >= 0.5f * plateau) on50 = dt0;
        const int32_t dt1 = (int32_t)(gT[i] - t1);
        if (dt1 >= 0 && off10 < 0) {
            // first time the envelope stays under 10% of plateau for 2 ms
            bool quiet = true;
            for (size_t k = i; k < gN && k < i + 32; ++k) if (envAt(k) >= 0.1f * plateau) { quiet = false; break; }
            if (quiet) off10 = dt1;
        }
    }
    const double snr = 20.0 * log10((plateau + 1) / (pre + 1));
    Serial.printf("%s ", snr >= 30.0 ? "PASS" : "FAIL");
    Serial.printf("TRIAL n=%d samples=%u pre_env=%.0f plateau=%.0f snr_db=%.1f on10_us=%ld on50_us=%ld off10_us=%ld\n",
                  n, (unsigned)gN, pre, plateau, 20.0 * log10((plateau + 1) / (pre + 1)), on10, on50, off10);
}

void setup() {
    Serial.begin(921600);
    delay(200);
    src.begin();
    gTone.begin();
    gTone.setToneHz(3200);
    Serial.println("LATENCY ready");
    collectUntil(micros() + 3000000);  // let the mic and the sample clock settle
}

void loop() {
    static int n = 0;
    if (true) {
        trial(++n);
        collectUntil(micros() + 500000);
        gN = 0;
    } else {
        delay(1000);
    }
}
