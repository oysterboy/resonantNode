#include "FeatureHistory.h"

#include <math.h>
#include <string.h>

namespace detection {

namespace {

void sortFloatValues(float* values, size_t count) {
    for (size_t i = 0; i + 1 < count; ++i) {
        size_t best = i;
        for (size_t j = i + 1; j < count; ++j) {
            if (values[j] < values[best]) {
                best = j;
            }
        }
        if (best != i) {
            const float tmp = values[i];
            values[i] = values[best];
            values[best] = tmp;
        }
    }
}

float exactQuantile(const float* values, size_t count, float quantile) {
    if (values == nullptr || count == 0) {
        return 0.0f;
    }

    const float clampedQuantile = quantile < 0.0f ? 0.0f : (quantile > 1.0f ? 1.0f : quantile);
    if (count == 1) {
        return values[0];
    }

    const float position = clampedQuantile * static_cast<float>(count - 1U);
    const size_t lowerIndex = static_cast<size_t>(position);
    const size_t upperIndex = lowerIndex + 1U < count ? lowerIndex + 1U : lowerIndex;
    const float fraction = position - static_cast<float>(lowerIndex);
    if (upperIndex == lowerIndex) {
        return values[lowerIndex];
    }

    return values[lowerIndex] + ((values[upperIndex] - values[lowerIndex]) * fraction);
}

float exactTrimmedMean(const float* values, size_t count, float trimFraction) {
    if (values == nullptr || count == 0) {
        return 0.0f;
    }

    const float clampedTrim = trimFraction < 0.0f ? 0.0f : (trimFraction > 0.5f ? 0.5f : trimFraction);
    const size_t trimCount = static_cast<size_t>(static_cast<float>(count) * clampedTrim);
    if (trimCount == 0 || count <= trimCount * 2U) {
        float sum = 0.0f;
        for (size_t i = 0; i < count; ++i) {
            sum += values[i];
        }
        return sum / static_cast<float>(count);
    }

    float sum = 0.0f;
    size_t keptCount = 0;
    for (size_t i = trimCount; i < count - trimCount; ++i) {
        sum += values[i];
        ++keptCount;
    }

    return keptCount > 0 ? (sum / static_cast<float>(keptCount)) : 0.0f;
}

} // namespace

bool FeatureHistory::isSupportedStream(FeatureStreamId stream) {
    switch (stream) {
        case FeatureStreamId::AmpEnvelope:
        case FeatureStreamId::FrequencyScore:
        case FeatureStreamId::FrequencyContrast:
            return true;
        case FeatureStreamId::Unknown:
        default:
            return false;
    }
}

size_t FeatureHistory::streamIndex(FeatureStreamId stream) {
    switch (stream) {
        case FeatureStreamId::AmpEnvelope:
            return 0U;
        case FeatureStreamId::FrequencyScore:
            return 1U;
        case FeatureStreamId::FrequencyContrast:
            return 2U;
        case FeatureStreamId::Unknown:
        default:
            return 0U;
    }
}

void FeatureHistory::reset() {
    for (size_t i = 0; i < kStreamCount; ++i) {
        StreamBuffer& buffer = _streams[i];
        buffer.binCount = 0;
        buffer.writeIndex = 0;
        buffer.valueCount = 0;
        buffer.sampleWriteIndex = 0;
        memset(buffer.bins, 0, sizeof(buffer.bins));
        memset(buffer.samples, 0, sizeof(buffer.samples));
    }
}

void FeatureHistory::pushRawSample(StreamBuffer& buffer, const FeatureStream& sample) {
    RawSample& rawSample = buffer.samples[buffer.sampleWriteIndex];
    rawSample.timeMs = sample.timeMs;
    rawSample.value = sample.value;

    if (buffer.valueCount < kMaxSamplesPerStream) {
        ++buffer.valueCount;
    }
    buffer.sampleWriteIndex = (buffer.sampleWriteIndex + 1U) % kMaxSamplesPerStream;
}

void FeatureHistory::pushSample(StreamBuffer& buffer, const FeatureStream& sample) {
    FeatureBin& bin = buffer.bins[buffer.writeIndex];
    if (buffer.binCount < kMaxSamplesPerStream) {
        ++buffer.binCount;
    }

    bin.timeMs = sample.timeMs;
    bin.first = sample.value;
    bin.last = sample.value;
    bin.min = sample.value;
    bin.max = sample.value;
    bin.sum = sample.value;
    bin.sumSquares = sample.value * sample.value;
    bin.count = 1;

    buffer.writeIndex = (buffer.writeIndex + 1U) % kMaxSamplesPerStream;
}

void FeatureHistory::record(const FeatureStream& sample) {
    if (!isSupportedStream(sample.id)) {
        return;
    }
    pushRawSample(_streams[streamIndex(sample.id)], sample);
    pushSample(_streams[streamIndex(sample.id)], sample);
}

void FeatureHistory::record(FeatureStreamId id, unsigned long timeMs, float value) {
    if (!isSupportedStream(id)) {
        return;
    }

    StreamBuffer& buffer = _streams[streamIndex(id)];
    if (buffer.binCount > 0) {
        const size_t latestIndex = (buffer.writeIndex + kMaxSamplesPerStream - 1U) % kMaxSamplesPerStream;
        FeatureBin& latest = buffer.bins[latestIndex];
        if (latest.timeMs == timeMs) {
            if (latest.count == 0) {
                latest.first = value;
                latest.min = value;
                latest.max = value;
                latest.sum = value;
                latest.count = 1;
            } else {
                if (latest.count == 1) {
                    latest.first = latest.last;
                }
                if (value < latest.min) {
                    latest.min = value;
                }
                if (value > latest.max) {
                    latest.max = value;
                }
                latest.sum += value;
                latest.sumSquares += value * value;
                ++latest.count;
            }
            latest.last = value;
            FeatureStream sample;
            sample.id = id;
            sample.timeMs = timeMs;
            sample.value = value;
            pushRawSample(buffer, sample);
            return;
        }
    }

    FeatureStream sample;
    sample.id = id;
    sample.timeMs = timeMs;
    sample.value = value;
    pushRawSample(buffer, sample);
    pushSample(buffer, sample);
}

ScalarWindow FeatureHistory::getWindow(FeatureStreamId stream, unsigned long startMs, unsigned long endMs, float sustainedThreshold) const {
    ScalarWindow out;
    out.stream = stream;
    out.startMs = startMs;
    out.endMs = endMs;
    out.present = isSupportedStream(stream);
    if (!out.present || endMs < startMs) {
        return out;
    }

    const StreamBuffer& buffer = _streams[streamIndex(stream)];
    if (buffer.binCount == 0) {
        return out;
    }

    const size_t oldestIndex = buffer.binCount == kMaxSamplesPerStream ? buffer.writeIndex : 0U;
    float rawValues[kMaxSamplesPerStream] = {};
    size_t rawCount = 0;
    float sum = 0.0f;
    float minValue = 0.0f;
    float maxValue = 0.0f;
    bool haveWindow = false;
    size_t sustainedCount = 0;
    size_t bucketCount = 0;
    float sumSquares = 0.0f;
    unsigned long firstValueMs = 0;
    unsigned long lastValueMs = 0;

    for (size_t i = 0; i < buffer.binCount; ++i) {
        const size_t index = (oldestIndex + i) % kMaxSamplesPerStream;
        const FeatureBin& bin = buffer.bins[index];
        if (bin.timeMs < startMs || bin.timeMs > endMs) {
            continue;
        }
        ++bucketCount;

        if (!haveWindow) {
            out.first = bin.first;
            out.last = bin.last;
            minValue = bin.min;
            maxValue = bin.max;
            out.peak = bin.max;
            out.peakTimeMs = bin.timeMs;
            firstValueMs = bin.timeMs;
            haveWindow = true;
        } else {
            out.last = bin.last;
            if (bin.min < minValue) {
                minValue = bin.min;
            }
            if (bin.max > maxValue) {
                maxValue = bin.max;
            }
            if (bin.max > out.peak) {
                out.peak = bin.max;
                out.peakTimeMs = bin.timeMs;
            }
        }
        lastValueMs = bin.timeMs;
        if (sustainedThreshold > 0.0f && bin.max >= sustainedThreshold) {
            ++sustainedCount;
        }
    }

    const size_t oldestSampleIndex = buffer.valueCount == kMaxSamplesPerStream ? buffer.sampleWriteIndex : 0U;
    for (size_t i = 0; i < buffer.valueCount; ++i) {
        const size_t index = (oldestSampleIndex + i) % kMaxSamplesPerStream;
        const RawSample& sample = buffer.samples[index];
        if (sample.timeMs < startMs || sample.timeMs > endMs) {
            continue;
        }

        if (rawCount < kMaxSamplesPerStream) {
            rawValues[rawCount++] = sample.value;
        }
        sum += sample.value;
        sumSquares += sample.value * sample.value;
    }

    out.valid = haveWindow;
    if (!out.valid) {
        return out;
    }

    out.durationMs = endMs >= startMs ? (endMs - startMs) : 0UL;
    out.min = minValue;
    out.max = maxValue;
    out.mean = rawCount > 0 ? sum / static_cast<float>(rawCount) : 0.0f;
    out.rms = rawCount > 0 ? sqrtf(sumSquares / static_cast<float>(rawCount)) : 0.0f;
    out.sampleCount = rawCount;
    out.valueCount = rawCount;
    out.freshValueCount = rawCount;
    out.bucketCount = bucketCount;
    out.valuesPerBucket = bucketCount > 0 ? static_cast<float>(rawCount) / static_cast<float>(bucketCount) : 0.0f;
    out.coveredMs = static_cast<unsigned long>(bucketCount);
    out.coverageRatio = out.durationMs > 0
        ? static_cast<float>(out.coveredMs) / static_cast<float>(out.durationMs)
        : 0.0f;
    out.firstValueMs = firstValueMs;
    out.lastValueMs = lastValueMs;
    out.latestValueAgeMs = haveWindow && endMs >= lastValueMs ? endMs - lastValueMs : 0UL;
    out.rise = out.last - out.first;
    out.sustainedThreshold = sustainedThreshold;
    out.sustainedCount = sustainedCount;
    out.sustainedMs = static_cast<unsigned long>(sustainedCount);

    sortFloatValues(rawValues, rawCount);
    out.median = exactQuantile(rawValues, rawCount, 0.50f);
    out.p75 = exactQuantile(rawValues, rawCount, 0.75f);
    out.p90 = exactQuantile(rawValues, rawCount, 0.90f);
    out.trimmedMean = exactTrimmedMean(rawValues, rawCount, 0.10f);
    return out;
}

size_t FeatureHistory::copyWindowApproximateValues(
    FeatureStreamId stream,
    unsigned long startMs,
    unsigned long endMs,
    float* outValues,
    size_t capacity
) const {
    if (!isSupportedStream(stream) || outValues == nullptr || capacity == 0 || endMs < startMs) {
        return 0;
    }

    const StreamBuffer& buffer = _streams[streamIndex(stream)];
    if (buffer.binCount == 0) {
        return 0;
    }

    const size_t oldestSampleIndex = buffer.valueCount == kMaxSamplesPerStream ? buffer.sampleWriteIndex : 0U;
    size_t written = 0;

    for (size_t i = 0; i < buffer.valueCount && written < capacity; ++i) {
        const size_t index = (oldestSampleIndex + i) % kMaxSamplesPerStream;
        const RawSample& sample = buffer.samples[index];
        if (sample.timeMs < startMs || sample.timeMs > endMs) {
            continue;
        }
        outValues[written++] = sample.value;
    }

    return written;
}

size_t FeatureHistory::sampleCount(FeatureStreamId stream) const {
    if (!isSupportedStream(stream)) {
        return 0;
    }
    return _streams[streamIndex(stream)].valueCount;
}

bool FeatureHistory::hasSamples(FeatureStreamId stream) const {
    return sampleCount(stream) > 0;
}

unsigned long FeatureHistory::latestTimeMs(FeatureStreamId stream) const {
    if (!isSupportedStream(stream)) {
        return 0;
    }

    const StreamBuffer& buffer = _streams[streamIndex(stream)];
    if (buffer.valueCount == 0) {
        return 0;
    }

    const size_t latestIndex = (buffer.sampleWriteIndex + kMaxSamplesPerStream - 1U) % kMaxSamplesPerStream;
    return buffer.samples[latestIndex].timeMs;
}

float FeatureHistory::latestValue(FeatureStreamId stream) const {
    if (!isSupportedStream(stream)) {
        return 0.0f;
    }

    const StreamBuffer& buffer = _streams[streamIndex(stream)];
    if (buffer.valueCount == 0) {
        return 0.0f;
    }

    const size_t latestIndex = (buffer.sampleWriteIndex + kMaxSamplesPerStream - 1U) % kMaxSamplesPerStream;
    return buffer.samples[latestIndex].value;
}

} // namespace detection
