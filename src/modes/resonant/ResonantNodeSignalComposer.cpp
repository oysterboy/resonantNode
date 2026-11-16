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

size_t NodeSignalComposer::maxBaseBin(const BandGrid& grid) const {
    const size_t steps = _values.hopSteps < grid.bins ? _values.hopSteps : grid.bins;
    return grid.bins > steps ? grid.bins - steps : 0;
}

size_t NodeSignalComposer::clampBaseBin(const BandGrid& grid, uint32_t base) const {
    const size_t top = maxBaseBin(grid);
    return base < top ? static_cast<size_t>(base) : top;
}

size_t NodeSignalComposer::hopFrequencies(const BandGrid& grid, uint32_t baseBin, uint32_t* out,
                                          size_t capacity) const {
    if (grid.bins == 0 || capacity == 0) {
        return 0;
    }
    size_t steps = _values.hopSteps;
    if (steps > capacity) {
        steps = capacity;
    }
    if (steps > grid.bins) {
        steps = grid.bins;
    }
    // Bin-aligned by construction (the detector's bank sits on the same grid). The train climbs from the node's
    // base bin without wrapping (the base is the node's note, decisions/2026-10-29-wider-band-base-bin-per-node.md);
    // a base too high for the train is lowered until the train fits.
    const size_t base = clampBaseBin(grid, baseBin);
    for (size_t k = 0; k < steps; ++k) {
        out[k] = grid.lowHz + static_cast<uint32_t>(base + k) * grid.spacingHz;
    }
    return steps;
}

bool NodeSignalComposer::composeHopTrain(const BandGrid& grid, uint32_t baseBin, OutputRequest& request,
                                         unsigned long maxTrainMs) const {
    uint32_t climb[kMaxHopSteps] = {};
    const size_t n = hopFrequencies(grid, baseBin, climb, kMaxHopSteps);
    if (n == 0 || _values.hopPulseMs == 0) {
        request.hopSteps = 0;
        return false;
    }
    size_t trill =
        _values.hopTrillPulses < runtime::kMaxTrillPulses ? _values.hopTrillPulses : runtime::kMaxTrillPulses;
    const unsigned long period = _values.hopPulseMs + _values.hopGapMs;
    while (trill > 0 && (n + trill) * period - _values.hopGapMs > maxTrainMs) {
        --trill;
    }
    for (size_t k = 0; k < trill; ++k) {
        request.hopHz[k] = climb[0];
    }
    for (size_t k = 0; k < n; ++k) {
        request.hopHz[trill + k] = climb[k];
    }
    request.hopSteps = trill + n;
    request.pulseMs = _values.hopPulseMs;
    request.gapMs = _values.hopGapMs;
    return true;
}

// A response to a heard pattern is the network signal: the node's hop train inside the band (BEH-005). The idle
// call is an announcement tone outside the band (not detected), a gap, then the node's own call (chosen
// 2026-10-17): the hop train, or the plain chirp when the hop is off. The self-test's chirps are the network signal
// too (ResonantBehavior::signalShape); the idle voice stays the plain pattern.
OutputRequest NodeSignalComposer::compose(const BandGrid& grid, ChirpPattern pattern, bool networkSignal,
                                          uint32_t baseBin) const {
    OutputRequest request;
    request.pattern = pattern;
    const unsigned long maxSpanMs = OutputLimits().maxSpanMs;
    if (_values.hopEnabled && networkSignal && composeHopTrain(grid, baseBin, request, maxSpanMs)) {
        request.kind = OutputRequest::Kind::HopTrain;
    } else if (_values.idleAnnounceHz > 0 && pattern == ChirpPattern::Idle) {
        request.kind = OutputRequest::Kind::IdleCall;
        // 1..99 Hz is below what the output plays (OutputLimits.minHz): raised to it, so the idle call is not
        // rejected into the fallback every time (review 12.0 M8; 0 still means no announcement).
        const uint32_t minHz = OutputLimits().minHz;
        request.announceHz = _values.idleAnnounceHz < minHz ? minHz : _values.idleAnnounceHz;
        request.announceMs = _values.idleAnnounceMs;
        request.announceGapMs = kIdleAnnounceGapMs;
        const unsigned long announceSpanMs = request.announceMs + request.announceGapMs;
        const unsigned long callMaxMs = announceSpanMs < maxSpanMs ? maxSpanMs - announceSpanMs : 0;
        if (!_values.hopEnabled || !composeHopTrain(grid, baseBin, request, callMaxMs)) {
            request.hopSteps = 0;
        }
    } else {
        request.kind = OutputRequest::Kind::Pattern;
    }
    return request;
}
