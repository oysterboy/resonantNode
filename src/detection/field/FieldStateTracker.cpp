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
    _signalCountInWindow = 0;
    _acceptedSignalCountInWindow = 0;
    _patternCountInWindow = 0;
    _signalWindowStartMs = 0;
    _patternWindowStartMs = 0;
}

void FieldStateTracker::update(unsigned long nowMs) {
    if (_signalWindowStartMs == 0) {
        _signalWindowStartMs = nowMs;
    } else if (nowMs >= _signalWindowStartMs && nowMs - _signalWindowStartMs >= _config.signalWindowMs) {
        _signalCountInWindow = 0;
        _acceptedSignalCountInWindow = 0;
        _signalWindowStartMs = nowMs;
    }

    if (_patternWindowStartMs == 0) {
        _patternWindowStartMs = nowMs;
    } else if (nowMs >= _patternWindowStartMs && nowMs - _patternWindowStartMs >= _config.patternWindowMs) {
        _patternCountInWindow = 0;
        _patternWindowStartMs = nowMs;
    }

    recompute(nowMs);
}

void FieldStateTracker::observeSignalCandidate(const SignalCandidate& signal, unsigned long nowMs) {
    if (signal.present) {
        _state.lastSignalMs = nowMs;
        ++_signalCountInWindow;
        recompute(nowMs);
    }
}

void FieldStateTracker::observeInspectedSignal(const InspectedSignal& signal, unsigned long nowMs) {
    if (signal.accepted) {
        _state.lastInspectedSignalMs = nowMs;
        ++_acceptedSignalCountInWindow;
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
    _state.recentSignalCount = _signalCountInWindow;
    _state.recentAcceptedSignalCount = _acceptedSignalCountInWindow;
    _state.recentPatternCount = _patternCountInWindow;
    _state.chatter = _signalCountInWindow;
    _state.quiet = _signalCountInWindow <= _config.quietSignalCountThreshold;
    _state.active = _signalCountInWindow > 0;
    _state.dense = _signalCountInWindow >= _config.denseSignalCountThreshold;
    _state.activity = _signalCountInWindow == 0
        ? 0.0f
        : (_signalCountInWindow >= _config.busySignalCountThreshold
            ? 1.0f
            : static_cast<float>(_signalCountInWindow) / static_cast<float>(_config.busySignalCountThreshold));
    _state.density = _patternCountInWindow == 0
        ? 0.0f
        : (_patternCountInWindow >= _config.denseSignalCountThreshold
            ? 1.0f
            : static_cast<float>(_patternCountInWindow) / static_cast<float>(_config.denseSignalCountThreshold));
    _state.noiseFloor = _signalCountInWindow == 0 ? 0.0f : static_cast<float>(_signalCountInWindow) / static_cast<float>(_config.signalWindowMs);
    _state.avgAmbientLevel = _state.noiseFloor;
}

} // namespace detection
