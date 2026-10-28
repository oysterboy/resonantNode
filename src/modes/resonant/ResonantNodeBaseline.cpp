#include "ResonantNodeBaseline.h"

#include "../../app/TimingUtils.h"

void NodeBaseline::begin(unsigned long now) {
    const Config config = _config;
    *this = NodeBaseline(config);
    _state = State::Boot;
    _stateStartedMs = now;
}

void NodeBaseline::startListening(unsigned long now) {
    _state = State::ListenForQuiet;
    _stateStartedMs = now;
    _quietSinceMs = 0;
    _lastLogMs = 0;
    _settleUntilMs = 0;
}

void NodeBaseline::noteForcedRebase(unsigned long now) {
    _state = State::Settle;
    _settleUntilMs = now + _config.settleMs;
    ++_forcedRebaseCount;
}

void NodeBaseline::resettle(unsigned long untilMs) {
    if (_state == State::Active || _state == State::Settle) {
        _state = State::Settle;
        _settleUntilMs = untilMs;
    }
}

void NodeBaseline::noteRebaseResult(RebaseResult result, unsigned long atMs) {
    if (_bootResult == RebaseResult::Pending) {
        _bootResult = result;
    }
    _lastResult = result;
    _lastRebaseAtMs = atMs;
    ++_quietRebaseCount;
}

const char* NodeBaseline::stateName() const {
    switch (_state) {
        case State::Boot:
            return "BOOT";
        case State::ListenForQuiet:
            return "LISTEN_FOR_QUIET";
        case State::Settle:
            return "SETTLE";
        case State::Active:
            return "ACTIVE";
        case State::FailedNoQuiet:
            return "FAILED_NO_QUIET";
    }
    return "UNKNOWN";
}

NodeBaseline::Tick NodeBaseline::update(unsigned long now, float smooth) {
    Tick tick;
    tick.smooth = smooth;
    switch (_state) {
        case State::Boot:
            if (timing::elapsedSince(now, _stateStartedMs, _config.micWarmupMs)) {
                // The Node rebases the warmed-up mic's floor, then calls startListening().
                tick.step = Step::WarmupDone;
                tick.elapsedMs = now - _stateStartedMs;
            } else if (_lastLogMs == 0 || timing::elapsedSince(now, _lastLogMs, _config.logEveryMs)) {
                _lastLogMs = now;
                tick.step = Step::WarmupWaiting;
                tick.elapsedMs = now - _stateStartedMs;
            }
            break;

        case State::ListenForQuiet: {
            const bool quietNow = smooth < static_cast<float>(_config.quietThreshold);
            if (quietNow) {
                if (_quietSinceMs == 0) {
                    _quietSinceMs = now;
                }
                const unsigned long quietMs = now - _quietSinceMs;
                tick.elapsedMs = quietMs;
                if (quietMs >= _config.quietHoldMs) {
                    _state = State::Settle;
                    _settleUntilMs = now + _config.settleMs;
                    _lastRebaseSmooth = smooth;
                    tick.step = Step::QuietHeld;
                } else if (_lastLogMs == 0 || timing::elapsedSince(now, _lastLogMs, _config.logEveryMs)) {
                    _lastLogMs = now;
                    tick.step = Step::RebaseWaiting;
                }
            } else {
                _quietSinceMs = 0;
                if (timing::elapsedSince(now, _stateStartedMs, _config.timeoutMs)) {
                    _state = State::FailedNoQuiet;
                    _lastRebaseSmooth = smooth;
                    tick.step = Step::NoQuiet;
                }
            }
        } break;

        case State::Settle:
            if (timing::atOrAfter(now, _settleUntilMs)) {
                _state = State::Active;
                tick.step = Step::Activated;
            }
            break;

        case State::Active:
        case State::FailedNoQuiet:
            break;
    }
    return tick;
}
