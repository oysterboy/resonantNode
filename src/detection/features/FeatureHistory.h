#pragma once

#include <stddef.h>
#include <stdint.h>

#include "FeatureStream.h"
#include "ScalarWindow.h"

namespace detection {

class FeatureHistory {
public:
    static constexpr size_t kMaxSamplesPerStream = 512;
    static constexpr size_t kStreamCount = static_cast<size_t>(FeatureStreamId::FrequencyContrast) + 1U;

    void reset();

    void record(const FeatureStream& sample);
    void record(FeatureStreamId id, unsigned long timeMs, float value);

    ScalarWindow getWindow(FeatureStreamId stream, unsigned long startMs, unsigned long endMs) const;

    size_t sampleCount(FeatureStreamId stream) const;
    bool hasSamples(FeatureStreamId stream) const;
    unsigned long latestTimeMs(FeatureStreamId stream) const;
    float latestValue(FeatureStreamId stream) const;

private:
    struct StreamBuffer {
        FeatureStream samples[kMaxSamplesPerStream] = {};
        size_t sampleCount = 0;
        size_t writeIndex = 0;
    };

    static bool isSupportedStream(FeatureStreamId stream);
    static size_t streamIndex(FeatureStreamId stream);

    void pushSample(StreamBuffer& buffer, const FeatureStream& sample);

    StreamBuffer _streams[kStreamCount] = {};
};

} // namespace detection
