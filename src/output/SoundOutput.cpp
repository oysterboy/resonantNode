#include "SoundOutput.h"

#include "../app/RuntimeDefaults.h"
#include "../app/TimeBase.h"
#include "../app/TimingUtils.h"

const char* outputRejectName(OutputReject reject) {
    switch (reject) {
        case OutputReject::None:
            return "none";
        case OutputReject::Busy:
            return "busy";
        case OutputReject::NoSteps:
            return "no_steps";
        case OutputReject::FrequencyOutOfRange:
            return "frequency_out_of_range";
        case OutputReject::PulseOutOfRange:
            return "pulse_out_of_range";
        case OutputReject::GapOutOfRange:
            return "gap_out_of_range";
        case OutputReject::AnnounceOutOfRange:
            return "announce_out_of_range";
        case OutputReject::SpanTooLong:
            return "span_too_long";
        case OutputReject::DeviceRefused:
            return "device_refused";
    }
    return "unknown";
}

const char* outputPhaseName(OutputPhase phase) {
    switch (phase) {
        case OutputPhase::Idle:
            return "idle";
        case OutputPhase::Playing:
            return "playing";
        case OutputPhase::Announce:
            return "announce";
        case OutputPhase::AnnounceGap:
            return "announce_gap";
        case OutputPhase::Call:
            return "call";
    }
    return "unknown";
}

namespace {

unsigned long trainMs(const OutputRequest& r) {
    return r.hopSteps == 0 ? 0UL
                           : static_cast<unsigned long>(r.hopSteps) * r.pulseMs +
                                 static_cast<unsigned long>(r.hopSteps - 1) * r.gapMs;
}

OutputReject validateTrain(const OutputRequest& r, const OutputLimits& limits) {
    if (r.hopSteps == 0 || r.hopSteps > kMaxHopSteps) {
        return OutputReject::NoSteps;
    }
    for (size_t k = 0; k < r.hopSteps; ++k) {
        if (r.hopHz[k] < limits.minHz || r.hopHz[k] > limits.maxHz) {
            return OutputReject::FrequencyOutOfRange;
        }
    }
    if (r.pulseMs == 0 || r.pulseMs > limits.maxPulseMs) {
        return OutputReject::PulseOutOfRange;
    }
    if (r.gapMs > limits.maxGapMs) {
        return OutputReject::GapOutOfRange;
    }
    return OutputReject::None;
}

}  // namespace

SoundOutput::SoundOutput(ChirpOutput& chirp, const OutputLimits& limits) : _chirp(chirp), _limits(limits) {}

unsigned long SoundOutput::plannedSpanMs(const OutputRequest& request, unsigned long plainCallMs) {
    switch (request.kind) {
        case OutputRequest::Kind::Pattern:
            return 0;
        case OutputRequest::Kind::HopTrain:
            return trainMs(request);
        case OutputRequest::Kind::IdleCall:
            return request.announceMs + request.announceGapMs + (request.hopSteps > 0 ? trainMs(request) : plainCallMs);
    }
    return 0;
}

OutputReject SoundOutput::validate(const OutputRequest& request, const OutputLimits& limits) {
    switch (request.kind) {
        case OutputRequest::Kind::Pattern:
            return OutputReject::None;
        case OutputRequest::Kind::HopTrain: {
            const OutputReject train = validateTrain(request, limits);
            if (train != OutputReject::None) {
                return train;
            }
            break;
        }
        case OutputRequest::Kind::IdleCall: {
            if (request.announceHz < limits.minHz || request.announceHz > limits.maxHz) {
                return OutputReject::FrequencyOutOfRange;
            }
            if (request.announceMs < limits.minAnnounceMs || request.announceMs > limits.maxAnnounceMs) {
                return OutputReject::AnnounceOutOfRange;
            }
            if (request.announceGapMs > limits.maxGapMs) {
                return OutputReject::GapOutOfRange;
            }
            if (request.hopSteps > 0) {
                const OutputReject train = validateTrain(request, limits);
                if (train != OutputReject::None) {
                    return train;
                }
            }
            break;
        }
    }
    if (plannedSpanMs(request, runtime::kDefaultChirpDurationMs) > limits.maxSpanMs) {
        return OutputReject::SpanTooLong;
    }
    return OutputReject::None;
}

void SoundOutput::reject(OutputReject reason) {
    ++_status.rejected;
    _status.lastReject = reason;
}

bool SoundOutput::start(const OutputRequest& request) {
    if (busy() || _chirp.isActive()) {
        reject(OutputReject::Busy);
        return false;
    }
    const OutputReject invalid = validate(request, _limits);
    if (invalid != OutputReject::None) {
        reject(invalid);
        return false;
    }
    _request = request;
    switch (request.kind) {
        case OutputRequest::Kind::Pattern:
            _chirp.start(request.pattern);
            if (!_chirp.isActive()) {
                reject(OutputReject::DeviceRefused);
                return false;
            }
            _status.phase = OutputPhase::Playing;
            break;
        case OutputRequest::Kind::HopTrain:
            if (!startHop(request)) {
                reject(OutputReject::DeviceRefused);
                return false;
            }
            _status.phase = OutputPhase::Playing;
            break;
        case OutputRequest::Kind::IdleCall:
            // The announcement: a plain tone outside the band (not detected).
            _chirp.setToneHz(request.announceHz);
            _chirp.setTiming(request.announceMs, 0);
            _chirp.start(ChirpPattern::Single);
            if (!_chirp.isActive()) {
                _chirp.setToneHz(runtime::kDefaultChirpFrequencyHz);
                _chirp.setTiming(runtime::kDefaultChirpDurationMs, 150);
                reject(OutputReject::DeviceRefused);
                return false;
            }
            _status.phase = OutputPhase::Announce;
            break;
    }
    _status.kind = request.kind;
    _status.spanMs = plannedSpanMs(request, runtime::kDefaultChirpDurationMs);
    _status.lastStartMs = timebase::nowMs();
    ++_status.started;
    return true;
}

bool SoundOutput::startHop(const OutputRequest& request) {
    _chirp.setPulseHop(request.hopHz, request.hopSteps);
    _chirp.startPulseTrain(static_cast<uint32_t>(request.hopSteps), request.pulseMs, request.gapMs,
                           runtime::kDefaultI2sToneRampMs);
    if (!_chirp.isActive()) {
        _chirp.setPulseHop(nullptr, 0);
        return false;
    }
    return true;
}

// The call after the announcement: the hop train, or the plain chirp when the
// request has no train or the train does not start.
bool SoundOutput::startCall() {
    if (_request.hopSteps > 0 && startHop(_request)) {
        return true;
    }
    _request.hopSteps = 0;
    _chirp.start(ChirpPattern::Single);
    return _chirp.isActive();
}

void SoundOutput::finish() {
    _status.phase = OutputPhase::Idle;
    _status.lastDoneMs = timebase::nowMs();  // fresh: the own-emit tail runs from the actual toneOff (issue #31)
    ++_status.done;
}

uint8_t SoundOutput::update() {
    _chirp.update();
    uint8_t events = 0;

    if (_status.phase == OutputPhase::AnnounceGap && timing::atOrAfter(timebase::nowMs(), _callAtMs) &&
        !_chirp.isActive()) {
        _status.phase = OutputPhase::Call;
        if (startCall()) {
            events |= kEventCallStarted;
        } else {
            finish();
            events |= kEventFinished;
        }
    }

    if (_status.phase == OutputPhase::Announce) {
        if (_chirp.finished()) {
            // The announcement ended: back to the chirp tone and timing, then the gap.
            _chirp.setToneHz(runtime::kDefaultChirpFrequencyHz);
            _chirp.setTiming(runtime::kDefaultChirpDurationMs, 150);
            _callAtMs = timebase::nowMs() + _request.announceGapMs;
            _status.phase = OutputPhase::AnnounceGap;
        }
    } else if ((_status.phase == OutputPhase::Playing || _status.phase == OutputPhase::Call) && _chirp.finished()) {
        finish();
        events |= kEventFinished;
    }
    return events;
}

void SoundOutput::stop() {
    const bool announcing = _status.phase == OutputPhase::Announce;
    _chirp.stop();
    if (announcing) {
        _chirp.setToneHz(runtime::kDefaultChirpFrequencyHz);
        _chirp.setTiming(runtime::kDefaultChirpDurationMs, 150);
    }
    if (_status.busy()) {
        _status.phase = OutputPhase::Idle;
        _status.lastDoneMs = timebase::nowMs();
    }
}
