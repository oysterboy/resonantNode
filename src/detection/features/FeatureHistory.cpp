#include "FeatureHistory.h"

#include <algorithm>
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace detection {

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
        memset(buffer.bins, 0, sizeof(buffer.bins));
    }
}

void FeatureHistory::pushSample(StreamBuffer& buffer, const FeatureStream& sample) {
    FeatureBin& bin = buffer.bins[buffer.writeIndex];
    if (buffer.binCount == kMaxSamplesPerStream) {
        if (buffer.valueCount >= bin.count) {
            buffer.valueCount -= bin.count;
        } else {
            buffer.valueCount = 0;
        }
    } else {
        ++buffer.binCount;
    }

    bin.timeMs = sample.timeMs;
    bin.first = sample.value;
    bin.last = sample.value;
    bin.min = sample.value;
    bin.max = sample.value;
    bin.sum = sample.value;
    bin.sumSquares = static_cast<double>(sample.value) * static_cast<double>(sample.value);
    bin.count = 1;

    ++buffer.valueCount;
    buffer.writeIndex = (buffer.writeIndex + 1U) % kMaxSamplesPerStream;
}

void FeatureHistory::record(const FeatureStream& sample) {
    if (!isSupportedStream(sample.id)) {
        return;
    }
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
                latest.sumSquares += static_cast<double>(value) * static_cast<double>(value);
                ++latest.count;
            }
            latest.last = value;
            ++buffer.valueCount;
            return;
        }
    }

    FeatureStream sample;
    sample.id = id;
    sample.timeMs = timeMs;
    sample.value = value;
    record(sample);
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
    float sum = 0.0f;
    float minValue = 0.0f;
    float maxValue = 0.0f;
    bool haveWindow = false;
    unsigned long valueCount = 0;
    size_t sustainedCount = 0;
    size_t bucketCount = 0;
    double sumSquares = 0.0;
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

        sum += bin.sum;
        sumSquares += bin.sumSquares;
        valueCount += static_cast<unsigned long>(bin.count);
        if (sustainedThreshold > 0.0f && bin.max >= sustainedThreshold) {
            ++sustainedCount;
        }
    }

    out.valid = haveWindow;
    if (!out.valid) {
        return out;
    }

    out.durationMs = endMs >= startMs ? (endMs - startMs) : 0UL;
    out.min = minValue;
    out.max = maxValue;
    out.mean = valueCount > 0 ? sum / static_cast<float>(valueCount) : 0.0f;
    out.rms = valueCount > 0 ? static_cast<float>(sqrt(sumSquares / static_cast<double>(valueCount))) : 0.0f;
    out.sampleCount = valueCount;
    out.valueCount = valueCount;
    out.freshValueCount = valueCount;
    out.bucketCount = bucketCount;
    out.valuesPerBucket = bucketCount > 0 ? static_cast<float>(valueCount) / static_cast<float>(bucketCount) : 0.0f;
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

    if (valueCount > 0) {
        float* values = static_cast<float*>(malloc(sizeof(float) * valueCount));
        if (values != nullptr) {
            const size_t copied = copyWindowApproximateValues(stream, startMs, endMs, values, valueCount);
            if (copied > 0) {
                std::sort(values, values + copied);
                const auto quantile = [values, copied](float q) -> float {
                    if (copied == 0) {
                        return 0.0f;
                    }
                    size_t index = static_cast<size_t>(q * static_cast<float>(copied - 1U));
                    if (index >= copied) {
                        index = copied - 1U;
                    }
                    return values[index];
                };

                out.median = quantile(0.50f);
                out.p75 = quantile(0.75f);
                out.p90 = quantile(0.90f);

                const size_t trimCount = copied / 10U;
                const size_t trimStart = trimCount;
                const size_t trimEnd = copied > trimCount ? copied - trimCount : copied;
                if (trimEnd > trimStart) {
                    double trimSum = 0.0;
                    for (size_t i = trimStart; i < trimEnd; ++i) {
                        trimSum += values[i];
                    }
                    out.trimmedMean = static_cast<float>(trimSum / static_cast<double>(trimEnd - trimStart));
                } else {
                    out.trimmedMean = out.mean;
                }
            }
            free(values);
        }
    }
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

    const size_t oldestIndex = buffer.binCount == kMaxSamplesPerStream ? buffer.writeIndex : 0U;
    size_t written = 0;

    for (size_t i = 0; i < buffer.binCount && written < capacity; ++i) {
        const size_t index = (oldestIndex + i) % kMaxSamplesPerStream;
        const FeatureBin& bin = buffer.bins[index];
        if (bin.timeMs < startMs || bin.timeMs > endMs) {
            continue;
        }

        const float representative = bin.count > 0 ? bin.sum / static_cast<float>(bin.count) : bin.last;
        for (size_t j = 0; j < bin.count && written < capacity; ++j) {
            outValues[written++] = representative;
        }
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
    if (buffer.binCount == 0) {
        return 0;
    }

    const size_t latestIndex = (buffer.writeIndex + kMaxSamplesPerStream - 1U) % kMaxSamplesPerStream;
    return buffer.bins[latestIndex].timeMs;
}

float FeatureHistory::latestValue(FeatureStreamId stream) const {
    if (!isSupportedStream(stream)) {
        return 0.0f;
    }

    const StreamBuffer& buffer = _streams[streamIndex(stream)];
    if (buffer.binCount == 0) {
        return 0.0f;
    }

    const size_t latestIndex = (buffer.writeIndex + kMaxSamplesPerStream - 1U) % kMaxSamplesPerStream;
    return buffer.bins[latestIndex].last;
}

} // namespace detection
