#include "FeatureHistory.h"

#include <math.h>

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

float absoluteValue(float value) {
    return value < 0.0f ? -value : value;
}

} // namespace

bool FeatureHistory::isSupportedStream(FeatureStreamId stream) {
    switch (stream) {
        case FeatureStreamId::AmpMagnitude:
        case FeatureStreamId::AmpEnvelope:
        case FeatureStreamId::FrequencyTarget:
        case FeatureStreamId::FrequencyContrast:
            return true;
        case FeatureStreamId::Unknown:
        default:
            return false;
    }
}

size_t FeatureHistory::streamIndex(FeatureStreamId stream) {
    switch (stream) {
        case FeatureStreamId::AmpMagnitude:
            return 0U;
        case FeatureStreamId::AmpEnvelope:
            return 1U;
        case FeatureStreamId::FrequencyTarget:
            return 2U;
        case FeatureStreamId::FrequencyContrast:
            return 3U;
        case FeatureStreamId::Unknown:
        default:
            return 0U;
    }
}

bool FeatureHistory::streamRequiresFreshAggregation(FeatureStreamId stream) {
    return stream == FeatureStreamId::FrequencyTarget
        || stream == FeatureStreamId::FrequencyContrast;
}

float FeatureHistory::representativeValueForStream(FeatureStreamId stream, const FeatureHistoryBin& bin) {
    if (!bin.valid || bin.inputCount == 0) {
        return 0.0f;
    }

    switch (stream) {
        case FeatureStreamId::AmpMagnitude:
        case FeatureStreamId::AmpEnvelope:
            return bin.meanAbs;
        case FeatureStreamId::FrequencyTarget:
        case FeatureStreamId::FrequencyContrast:
            return bin.mean;
        case FeatureStreamId::Unknown:
        default:
            return bin.last;
    }
}

float FeatureHistory::representativeValueForAccumulator(FeatureStreamId stream, const FeatureBinAccumulator& bin) {
    if (!bin.valid || bin.inputCount == 0) {
        return 0.0f;
    }

    switch (stream) {
        case FeatureStreamId::AmpMagnitude:
        case FeatureStreamId::AmpEnvelope:
            return static_cast<float>(bin.sumAbs / static_cast<double>(bin.inputCount));
        case FeatureStreamId::FrequencyTarget:
        case FeatureStreamId::FrequencyContrast:
            return static_cast<float>(bin.sum / static_cast<double>(bin.inputCount));
        case FeatureStreamId::Unknown:
        default:
            return bin.last;
    }
}

void FeatureHistory::resetStream(StreamBuffer& buffer) {
    buffer = {};
}

void FeatureHistory::reset() {
    for (size_t i = 0; i < kStreamCount; ++i) {
        resetStream(_streams[i]);
    }
}

void FeatureHistory::startCurrentBin(StreamBuffer& buffer, unsigned long timeMs) {
    buffer.current = {};
    buffer.current.startMs = timeMs;
    buffer.current.valid = true;
    buffer.hasCurrent = true;
}

void FeatureHistory::accumulateIntoCurrentBin(StreamBuffer& buffer, float value, bool fresh) {
    if (!buffer.hasCurrent) {
        return;
    }

    FeatureBinAccumulator& current = buffer.current;
    ++current.inputCount;
    if (fresh) {
        ++current.freshCount;
    }
    current.sum += value;
    current.sumSquares += static_cast<double>(value) * static_cast<double>(value);
    current.sumAbs += absoluteValue(value);
    if (absoluteValue(value) > current.peak) {
        current.peak = absoluteValue(value);
    }
    current.last = value;
}

void FeatureHistory::finalizeCurrentBin(StreamBuffer& buffer, FeatureStreamId stream) {
    if (!buffer.hasCurrent || !buffer.current.valid) {
        return;
    }

    FeatureHistoryBin finalized = {};
    finalized.startMs = buffer.current.startMs;
    finalized.inputCount = buffer.current.inputCount;
    finalized.freshCount = buffer.current.freshCount;
    finalized.last = buffer.current.last;
    finalized.peak = buffer.current.peak;
    finalized.mean = finalized.inputCount > 0
        ? static_cast<float>(buffer.current.sum / static_cast<double>(finalized.inputCount))
        : 0.0f;
    finalized.rms = finalized.inputCount > 0
        ? sqrtf(static_cast<float>(buffer.current.sumSquares / static_cast<double>(finalized.inputCount)))
        : 0.0f;
    finalized.meanAbs = finalized.inputCount > 0
        ? static_cast<float>(buffer.current.sumAbs / static_cast<double>(finalized.inputCount))
        : 0.0f;
    finalized.valid = finalized.inputCount > 0;

    if (finalized.valid) {
        FeatureHistoryBin& slot = buffer.bins[buffer.writeIndex];
        slot = finalized;
        buffer.writeIndex = (buffer.writeIndex + 1U) % kBinsPerStream;
        if (buffer.binCount < kBinsPerStream) {
            ++buffer.binCount;
        }
        buffer.latestTimeMs = finalized.startMs;
        buffer.latestValue = representativeValueForStream(stream, finalized);
    }

    buffer.hasCurrent = false;
    buffer.current = {};
}

void FeatureHistory::record(const FeatureStream& sample, bool fresh) {
    record(sample.id, sample.timeMs, sample.value, fresh);
}

void FeatureHistory::record(FeatureStreamId id, unsigned long timeMs, float value, bool fresh) {
    if (!isSupportedStream(id)) {
        return;
    }

    if (streamRequiresFreshAggregation(id) && !fresh) {
        return;
    }

    StreamBuffer& buffer = _streams[streamIndex(id)];
    if (!buffer.hasCurrent) {
        startCurrentBin(buffer, timeMs);
    } else if (timeMs != buffer.current.startMs) {
        finalizeCurrentBin(buffer, id);
        startCurrentBin(buffer, timeMs);
    }

    accumulateIntoCurrentBin(buffer, value, fresh);
    buffer.latestTimeMs = buffer.current.startMs;
    buffer.latestValue = representativeValueForAccumulator(id, buffer.current);
}

ScalarWindow FeatureHistory::getWindow(
    FeatureStreamId stream,
    unsigned long startMs,
    unsigned long endMs,
    unsigned long inspectionNowMs,
    float sustainedThreshold
) const {
    ScalarWindow out;
    out.stream = stream;
    out.startMs = startMs;
    out.endMs = endMs;
    out.inspectionNowMs = inspectionNowMs;
    out.requestedStartMs = startMs;
    out.requestedEndMs = endMs;
    out.present = isSupportedStream(stream);
    if (!out.present || endMs < startMs) {
        return out;
    }

    const StreamBuffer& buffer = _streams[streamIndex(stream)];
    if (buffer.binCount == 0 && !buffer.hasCurrent) {
        return out;
    }

    float values[kBinsPerStream + 1U] = {};
    size_t valueCount = 0;
    size_t totalInputCount = 0;
    size_t totalFreshCount = 0;
    size_t bucketCount = 0;
    unsigned long coveredDurationMs = 0;
    float sum = 0.0f;
    float sumSquares = 0.0f;
    bool haveWindow = false;
    float firstValue = 0.0f;
    float lastValue = 0.0f;
    float minValue = 0.0f;
    float maxValue = 0.0f;
    float peakValue = 0.0f;
    unsigned long peakTimeMs = 0;
    unsigned long firstValueMs = 0;
    unsigned long lastValueMs = 0;
    size_t sustainedCount = 0;

    auto considerBin = [&](const FeatureHistoryBin& bin) {
        if (!bin.valid || bin.startMs < startMs || bin.startMs > endMs) {
            return;
        }
        if (streamRequiresFreshAggregation(stream) && bin.freshCount == 0) {
            return;
        }

        ++coveredDurationMs;

        const float representative = representativeValueForStream(stream, bin);
        if (valueCount < (kBinsPerStream + 1U)) {
            values[valueCount++] = representative;
        }
        ++bucketCount;
        totalInputCount += bin.inputCount;
        totalFreshCount += bin.freshCount;
        sum += representative;
        sumSquares += representative * representative;

        if (!haveWindow) {
            firstValue = representative;
            lastValue = representative;
            minValue = representative;
            maxValue = representative;
            peakValue = representative;
            peakTimeMs = bin.startMs;
            firstValueMs = bin.startMs;
            lastValueMs = bin.startMs;
            haveWindow = true;
        } else {
            lastValue = representative;
            lastValueMs = bin.startMs;
            if (representative < minValue) {
                minValue = representative;
            }
            if (representative > maxValue) {
                maxValue = representative;
            }
            if (representative > peakValue) {
                peakValue = representative;
                peakTimeMs = bin.startMs;
            }
        }

        if (sustainedThreshold > 0.0f && representative >= sustainedThreshold) {
            ++sustainedCount;
        }
    };

    const size_t oldestIndex = buffer.binCount == kBinsPerStream ? buffer.writeIndex : 0U;
    for (size_t i = 0; i < buffer.binCount; ++i) {
        const size_t index = (oldestIndex + i) % kBinsPerStream;
        considerBin(buffer.bins[index]);
    }
    if (buffer.hasCurrent) {
        FeatureHistoryBin current = {};
        current.startMs = buffer.current.startMs;
        current.inputCount = buffer.current.inputCount;
        current.freshCount = buffer.current.freshCount;
        current.mean = buffer.current.inputCount > 0
            ? static_cast<float>(buffer.current.sum / static_cast<double>(buffer.current.inputCount))
            : 0.0f;
        current.rms = buffer.current.inputCount > 0
            ? sqrtf(static_cast<float>(buffer.current.sumSquares / static_cast<double>(buffer.current.inputCount)))
            : 0.0f;
        current.peak = buffer.current.peak;
        current.meanAbs = buffer.current.inputCount > 0
            ? static_cast<float>(buffer.current.sumAbs / static_cast<double>(buffer.current.inputCount))
            : 0.0f;
        current.last = buffer.current.last;
        current.valid = buffer.current.inputCount > 0;
        considerBin(current);
    }

    out.hasValues = haveWindow;
    out.durationMs = endMs >= startMs ? (endMs - startMs) + 1UL : 0UL;
    out.requestedFutureAtInspection = endMs > inspectionNowMs;
    out.availableStartMs = firstValueMs;
    out.availableEndMs = lastValueMs;
    out.leftMissingMs = firstValueMs > startMs ? firstValueMs - startMs : 0UL;
    out.rightMissingMs = endMs > (lastValueMs + kBinDurationMs) ? endMs - (lastValueMs + kBinDurationMs) : 0UL;
    out.coverageComplete = haveWindow &&
        out.leftMissingMs == 0UL &&
        out.rightMissingMs == 0UL &&
        !out.requestedFutureAtInspection;
    out.valid = out.coverageComplete && out.hasValues;
    out.coveredDurationMs = coveredDurationMs * kBinDurationMs;
    out.sampleCount = bucketCount;
    out.valueCount = totalInputCount;
    out.freshValueCount = totalFreshCount;
    out.bucketCount = bucketCount;
    out.valuesPerBucket = bucketCount > 0 ? static_cast<float>(totalInputCount) / static_cast<float>(bucketCount) : 0.0f;
    out.coveredMs = out.coveredDurationMs;
    out.coverageRatio = out.durationMs > 0
        ? static_cast<float>(out.coveredDurationMs) / static_cast<float>(out.durationMs)
        : 0.0f;
    out.firstValueMs = firstValueMs;
    out.lastValueMs = lastValueMs;
    out.spanMs = haveWindow && lastValueMs >= firstValueMs ? lastValueMs - firstValueMs : 0UL;
    out.latestValueAgeMs = haveWindow && endMs >= lastValueMs ? endMs - lastValueMs : 0UL;
    out.first = firstValue;
    out.last = lastValue;
    out.min = minValue;
    out.max = maxValue;
    out.mean = valueCount > 0 ? sum / static_cast<float>(valueCount) : 0.0f;
    out.rms = valueCount > 0 ? sqrtf(sumSquares / static_cast<float>(valueCount)) : 0.0f;
    sortFloatValues(values, valueCount);
    out.median = exactQuantile(values, valueCount, 0.50f);
    out.p75 = exactQuantile(values, valueCount, 0.75f);
    out.p90 = exactQuantile(values, valueCount, 0.90f);
    out.trimmedMean = exactTrimmedMean(values, valueCount, 0.10f);
    out.peak = peakValue;
    out.peakTimeMs = peakTimeMs;
    out.rise = out.last - out.first;
    out.sustainedThreshold = sustainedThreshold;
    out.sustainedCount = sustainedCount;
    out.sustainedMs = static_cast<unsigned long>(sustainedCount) * kBinDurationMs;
    out.internalCoverageKnown = true;
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
    if (buffer.binCount == 0 && !buffer.hasCurrent) {
        return 0;
    }

    size_t written = 0;

    auto copyBin = [&](const FeatureHistoryBin& bin) {
        if (!bin.valid || bin.startMs < startMs || bin.startMs > endMs) {
            return;
        }
        if (streamRequiresFreshAggregation(stream) && bin.freshCount == 0) {
            return;
        }
        if (written < capacity) {
            outValues[written++] = representativeValueForStream(stream, bin);
        }
    };

    const size_t oldestIndex = buffer.binCount == kBinsPerStream ? buffer.writeIndex : 0U;
    for (size_t i = 0; i < buffer.binCount && written < capacity; ++i) {
        const size_t index = (oldestIndex + i) % kBinsPerStream;
        copyBin(buffer.bins[index]);
    }
    if (buffer.hasCurrent && written < capacity) {
        FeatureHistoryBin current = {};
        current.startMs = buffer.current.startMs;
        current.inputCount = buffer.current.inputCount;
        current.freshCount = buffer.current.freshCount;
        current.mean = buffer.current.inputCount > 0
            ? static_cast<float>(buffer.current.sum / static_cast<double>(buffer.current.inputCount))
            : 0.0f;
        current.rms = buffer.current.inputCount > 0
            ? sqrtf(static_cast<float>(buffer.current.sumSquares / static_cast<double>(buffer.current.inputCount)))
            : 0.0f;
        current.peak = buffer.current.peak;
        current.meanAbs = buffer.current.inputCount > 0
            ? static_cast<float>(buffer.current.sumAbs / static_cast<double>(buffer.current.inputCount))
            : 0.0f;
        current.last = buffer.current.last;
        current.valid = buffer.current.inputCount > 0;
        copyBin(current);
    }

    return written;
}

size_t FeatureHistory::sampleCount(FeatureStreamId stream) const {
    if (!isSupportedStream(stream)) {
        return 0;
    }

    const StreamBuffer& buffer = _streams[streamIndex(stream)];
    return buffer.binCount + (buffer.hasCurrent && buffer.current.inputCount > 0 ? 1U : 0U);
}

bool FeatureHistory::hasSamples(FeatureStreamId stream) const {
    return sampleCount(stream) > 0;
}

unsigned long FeatureHistory::latestTimeMs(FeatureStreamId stream) const {
    if (!isSupportedStream(stream)) {
        return 0;
    }

    const StreamBuffer& buffer = _streams[streamIndex(stream)];
    if (buffer.hasCurrent && buffer.current.inputCount > 0) {
        return buffer.current.startMs;
    }

    if (buffer.binCount == 0) {
        return 0;
    }

    const size_t latestIndex = (buffer.writeIndex + kBinsPerStream - 1U) % kBinsPerStream;
    return buffer.bins[latestIndex].startMs;
}

float FeatureHistory::latestValue(FeatureStreamId stream) const {
    if (!isSupportedStream(stream)) {
        return 0.0f;
    }

    const StreamBuffer& buffer = _streams[streamIndex(stream)];
    if (buffer.hasCurrent && buffer.current.inputCount > 0) {
        return representativeValueForAccumulator(stream, buffer.current);
    }

    if (buffer.binCount == 0) {
        return 0.0f;
    }

    const size_t latestIndex = (buffer.writeIndex + kBinsPerStream - 1U) % kBinsPerStream;
    return representativeValueForStream(stream, buffer.bins[latestIndex]);
}

} // namespace detection
