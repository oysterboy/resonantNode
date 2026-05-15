#include "FeatureHistory.h"

namespace detection {

bool FeatureHistory::isSupportedStream(FeatureStreamId stream) {
    return stream != FeatureStreamId::Unknown &&
           static_cast<size_t>(stream) < kStreamCount;
}

size_t FeatureHistory::streamIndex(FeatureStreamId stream) {
    return static_cast<size_t>(stream);
}

void FeatureHistory::reset() {
    for (size_t i = 0; i < kStreamCount; ++i) {
        _streams[i] = {};
    }
}

void FeatureHistory::pushSample(StreamBuffer& buffer, const FeatureStream& sample) {
    buffer.samples[buffer.writeIndex] = sample;
    buffer.writeIndex = (buffer.writeIndex + 1U) % kMaxSamplesPerStream;
    if (buffer.sampleCount < kMaxSamplesPerStream) {
        ++buffer.sampleCount;
    }
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
    FeatureStream sample;
    sample.id = id;
    sample.timeMs = timeMs;
    sample.value = value;
    record(sample);
}

ScalarWindow FeatureHistory::getWindow(FeatureStreamId stream, unsigned long startMs, unsigned long endMs) const {
    ScalarWindow out;
    out.stream = stream;
    out.startMs = startMs;
    out.endMs = endMs;
    out.present = isSupportedStream(stream);
    if (!out.present || endMs < startMs) {
        return out;
    }

    const StreamBuffer& buffer = _streams[streamIndex(stream)];
    if (buffer.sampleCount == 0) {
        return out;
    }

    const size_t oldestIndex = buffer.sampleCount == kMaxSamplesPerStream ? buffer.writeIndex : 0U;
    float sum = 0.0f;
    float minValue = 0.0f;
    float maxValue = 0.0f;
    bool haveWindow = false;
    size_t count = 0;

    for (size_t i = 0; i < buffer.sampleCount; ++i) {
        const size_t index = (oldestIndex + i) % kMaxSamplesPerStream;
        const FeatureStream& sample = buffer.samples[index];
        if (sample.timeMs < startMs || sample.timeMs > endMs) {
            continue;
        }

        if (!haveWindow) {
            out.first = sample.value;
            out.last = sample.value;
            minValue = sample.value;
            maxValue = sample.value;
            out.peak = sample.value;
            out.peakTimeMs = sample.timeMs;
            haveWindow = true;
        } else {
            out.last = sample.value;
            if (sample.value < minValue) {
                minValue = sample.value;
            }
            if (sample.value > maxValue) {
                maxValue = sample.value;
            }
            if (sample.value > out.peak) {
                out.peak = sample.value;
                out.peakTimeMs = sample.timeMs;
            }
        }

        sum += sample.value;
        ++count;
    }

    out.sampleCount = count;
    out.valid = haveWindow;
    if (!out.valid) {
        return out;
    }

    out.durationMs = endMs >= startMs ? (endMs - startMs) : 0UL;
    out.min = minValue;
    out.max = maxValue;
    out.mean = count > 0 ? sum / static_cast<float>(count) : 0.0f;
    out.rise = out.last - out.first;
    return out;
}

size_t FeatureHistory::sampleCount(FeatureStreamId stream) const {
    if (!isSupportedStream(stream)) {
        return 0;
    }
    return _streams[streamIndex(stream)].sampleCount;
}

bool FeatureHistory::hasSamples(FeatureStreamId stream) const {
    return sampleCount(stream) > 0;
}

unsigned long FeatureHistory::latestTimeMs(FeatureStreamId stream) const {
    if (!isSupportedStream(stream)) {
        return 0;
    }

    const StreamBuffer& buffer = _streams[streamIndex(stream)];
    if (buffer.sampleCount == 0) {
        return 0;
    }

    const size_t latestIndex = buffer.sampleCount == 0 ? 0U : (buffer.writeIndex + kMaxSamplesPerStream - 1U) % kMaxSamplesPerStream;
    return buffer.samples[latestIndex].timeMs;
}

float FeatureHistory::latestValue(FeatureStreamId stream) const {
    if (!isSupportedStream(stream)) {
        return 0.0f;
    }

    const StreamBuffer& buffer = _streams[streamIndex(stream)];
    if (buffer.sampleCount == 0) {
        return 0.0f;
    }

    const size_t latestIndex = (buffer.writeIndex + kMaxSamplesPerStream - 1U) % kMaxSamplesPerStream;
    return buffer.samples[latestIndex].value;
}

} // namespace detection
