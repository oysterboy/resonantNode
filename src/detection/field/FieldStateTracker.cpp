#include "FieldStateTracker.h"

namespace detection {

void FieldStateTracker::reset() {
    _state = {};
    _signalCountInWindow = 0;
    _acceptedSignalCountInWindow = 0;
    _patternCountInWindow = 0;
    _lastWindowResetMs = 0;
}

void FieldStateTracker::update(unsigned long nowMs) {
    if (_lastWindowResetMs == 0) {
        _lastWindowResetMs = nowMs;
    } else if (nowMs >= _lastWindowResetMs && nowMs - _lastWindowResetMs >= _windowMs) {
        _signalCountInWindow = 0;
        _acceptedSignalCountInWindow = 0;
        _patternCountInWindow = 0;
        _lastWindowResetMs = nowMs;
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
    _state.quiet = _signalCountInWindow == 0;
    _state.active = _signalCountInWindow > 0;
    _state.dense = _signalCountInWindow >= 8;
    _state.activity = _signalCountInWindow == 0 ? 0.0f : (_signalCountInWindow >= 8 ? 1.0f : static_cast<float>(_signalCountInWindow) / 8.0f);
    _state.density = _patternCountInWindow == 0 ? 0.0f : (_patternCountInWindow >= 8 ? 1.0f : static_cast<float>(_patternCountInWindow) / 8.0f);
    _state.noiseFloor = _signalCountInWindow == 0 ? 0.0f : static_cast<float>(_signalCountInWindow) / static_cast<float>(_windowMs);
}

} // namespace detection
