#include "FieldStateTracker.h"

namespace detection {

void FieldStateTracker::setConfig(const FieldStateConfig& config) {
    _config = config;
}

const FieldStateConfig& FieldStateTracker::config() const {
    return _config;
}

void FieldStateTracker::reset() {
    _state = {};
    _occurrenceCountInWindow = 0;
    _acceptedOccurrenceCountInWindow = 0;
    _patternCountInWindow = 0;
    _occurrenceWindowStartMs = 0;
    _patternWindowStartMs = 0;
}

void FieldStateTracker::update(unsigned long nowMs) {
    if (_occurrenceWindowStartMs == 0) {
        _occurrenceWindowStartMs = nowMs;
    } else if (nowMs >= _occurrenceWindowStartMs && nowMs - _occurrenceWindowStartMs >= _config.occurrenceWindowMs) {
        _occurrenceCountInWindow = 0;
        _acceptedOccurrenceCountInWindow = 0;
        _occurrenceWindowStartMs = nowMs;
    }

    if (_patternWindowStartMs == 0) {
        _patternWindowStartMs = nowMs;
    } else if (nowMs >= _patternWindowStartMs && nowMs - _patternWindowStartMs >= _config.patternWindowMs) {
        _patternCountInWindow = 0;
        _patternWindowStartMs = nowMs;
    }

    recompute(nowMs);
}

void FieldStateTracker::observeOccurrence(const Occurrence& occurrence, unsigned long nowMs) {
    if (occurrence.present) {
        _state.lastOccurrenceMs = nowMs;
        ++_occurrenceCountInWindow;
        recompute(nowMs);
    }
}

void FieldStateTracker::observeInspectedOccurrence(const InspectedOccurrence& occurrence, unsigned long nowMs) {
    if (occurrence.accepted) {
        _state.lastInspectedOccurrenceMs = nowMs;
        ++_acceptedOccurrenceCountInWindow;
        recompute(nowMs);
    }
}

void FieldStateTracker::observePatternResult(const PatternResult& result, unsigned long nowMs) {
    if (result.valid) {
        _state.lastPatternMs = nowMs;
        ++_patternCountInWindow;
        recompute(nowMs);
    }
}

const FieldState& FieldStateTracker::state() const {
    return _state;
}

void FieldStateTracker::recompute(unsigned long nowMs) {
    (void)nowMs;
    _state.recentOccurrenceCount = _occurrenceCountInWindow;
    _state.recentAcceptedOccurrenceCount = _acceptedOccurrenceCountInWindow;
    _state.recentPatternCount = _patternCountInWindow;
    _state.chatter = _occurrenceCountInWindow;
    _state.quiet = _occurrenceCountInWindow <= _config.quietOccurrenceCountThreshold;
    _state.active = _occurrenceCountInWindow > 0;
    _state.dense = _occurrenceCountInWindow >= _config.denseOccurrenceCountThreshold;
    _state.activity = _occurrenceCountInWindow == 0
        ? 0.0f
        : (_occurrenceCountInWindow >= _config.busyOccurrenceCountThreshold
            ? 1.0f
            : static_cast<float>(_occurrenceCountInWindow) / static_cast<float>(_config.busyOccurrenceCountThreshold));
    _state.density = _patternCountInWindow == 0
        ? 0.0f
        : (_patternCountInWindow >= _config.denseOccurrenceCountThreshold
            ? 1.0f
            : static_cast<float>(_patternCountInWindow) / static_cast<float>(_config.denseOccurrenceCountThreshold));
    _state.noiseFloor = _occurrenceCountInWindow == 0 ? 0.0f : static_cast<float>(_occurrenceCountInWindow) / static_cast<float>(_config.occurrenceWindowMs);
    _state.avgAmbientLevel = _state.noiseFloor;
}

} // namespace detection

