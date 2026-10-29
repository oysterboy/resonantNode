#include "ResonantNodeSignalComposer.h"

#include "../../detection/features/FreqBandStream.h"

// The grid the hop train is played on is the grid the detector listens on: band edges as applied (clamped to
// FreqBandStream's bank) and its bin spacing, never recomputed here.
NodeSignalComposer::BandGrid NodeSignalComposer::gridOf(const FreqBandStream& band) {
    BandGrid grid;
    grid.lowHz = static_cast<uint32_t>(band.bandLowHz());
    grid.spacingHz = static_cast<uint32_t>(band.bandSpacingHz() + 0.5f);
    grid.bins = static_cast<size_t>(band.bandBinCount());
    return grid;
}

size_t NodeSignalComposer::hopFrequencies(const BandGrid& grid, uint32_t* out, size_t capacity) const {
    if (grid.bins == 0 || capacity == 0) {
        return 0;
    }
    size_t steps = _values.hopSteps;
    if (steps > capacity) {
        steps = capacity;
    }
    // Bin-aligned by construction (the detector's bank sits on the same
    // grid); the rotation picks the start bin, the order is the grid's.
    for (size_t k = 0; k < steps; ++k) {
        const size_t bin = (k + static_cast<size_t>(_values.hopRotation)) % grid.bins;
        out[k] = grid.lowHz + static_cast<uint32_t>(bin) * grid.spacingHz;
    }
    return steps;
}

bool NodeSignalComposer::composeHopTrain(const BandGrid& grid, OutputRequest& request) const {
    const size_t n = hopFrequencies(grid, request.hopHz, kMaxHopSteps);
    if (n == 0 || _values.hopPulseMs == 0) {
        request.hopSteps = 0;
        return false;
    }
    request.hopSteps = n;
    request.pulseMs = _values.hopPulseMs;
    request.gapMs = _values.hopGapMs;
    return true;
}

// A response to a heard pattern is the network signal: the node's hop train inside the band (BEH-005). The idle
// call is an announcement tone outside the band (not detected), a gap, then the node's own call (owner
// 2026-10-17): the hop train, or the plain chirp when the hop is off. The self-test's chirps are the network signal
// too (ResonantBehavior::signalShape); the idle voice stays the plain pattern.
OutputRequest NodeSignalComposer::compose(const BandGrid& grid, ChirpPattern pattern, bool networkSignal) const {
    OutputRequest request;
    request.pattern = pattern;
    if (_values.hopEnabled && networkSignal && composeHopTrain(grid, request)) {
        request.kind = OutputRequest::Kind::HopTrain;
    } else if (_values.idleAnnounceHz > 0 && pattern == ChirpPattern::Idle) {
        request.kind = OutputRequest::Kind::IdleCall;
        // 1..99 Hz is below what the output plays (OutputLimits.minHz): raised to it, so the idle call is not
        // rejected into the fallback every time (review 12.0 M8; 0 still means no announcement).
        const uint32_t minHz = OutputLimits().minHz;
        request.announceHz = _values.idleAnnounceHz < minHz ? minHz : _values.idleAnnounceHz;
        request.announceMs = _values.idleAnnounceMs;
        request.announceGapMs = kIdleAnnounceGapMs;
        if (!_values.hopEnabled || !composeHopTrain(grid, request)) {
            request.hopSteps = 0;
        }
    } else {
        request.kind = OutputRequest::Kind::Pattern;
    }
    return request;
}
