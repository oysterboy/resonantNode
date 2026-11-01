#include "EnvelopeRuntime.h"

#include "../app/TimingUtils.h"
#include "inspection/VerdictProjection.h"

namespace detection {

void EnvelopeRuntime::configure(const ScalarTransientConfig& detector, const InspectionPlan& plan) {
    _detector.setConfig(detector);
    _plan = plan;
    _inspector.configure(_plan);
    // The plan's streams are the ones the history keeps (one per module at most, as DetectionRuntime binds them).
    FeatureStreamId streams[kMaxInspectionModules] = {};
    size_t count = 0;
    for (size_t i = 0; i < _plan.count && i < kMaxInspectionModules; ++i) {
        const InspectionModuleConfig& m = _plan.modules[i];
        if (m.kind == InspectionModuleKind::AttackRise) {
            streams[count++] = m.attack.stream;
        } else if (m.kind == InspectionModuleKind::LevelWindow) {
            streams[count++] = m.level.stream;
        } else if (m.kind == InspectionModuleKind::Modulation) {
            streams[count++] = m.modulation.stream;
        } else if (m.kind == InspectionModuleKind::MagnitudeFeatureStrength) {
            streams[count++] = m.magnitude.stream;
        }
    }
    _history.setActiveStreams(streams, count);
}

void EnvelopeRuntime::reset() {
    _history.reset();
    _detector.resetState();
    _havePending = false;
    _resultCount = 0;
}

void EnvelopeRuntime::restartAfterInputGap() {
    _detector.restartAfterInputGap();
    _havePending = false;  // an occurrence cut by the gap is dropped, as the Node drops one at its restart
}

void EnvelopeRuntime::onBlock(float level, uint64_t blockUs, uint32_t blockMs) {
    _history.record(FeatureStreamId::AmpEnvelope, blockMs, level);
    AudioSamplePacket packet;
    packet.timeUs64 = blockUs;
    packet.timeMs = blockMs;
    packet.audioMagnitudeValue = static_cast<audio::Strength16>(level);
    packet.smoothedLevel = static_cast<audio::Strength16>(level);
    packet.valid = true;
    _detector.update(packet, level);
    Occurrence occurrence;
    while (_detector.popOccurrence(occurrence)) {
        if (_havePending) {
            judgePending(blockMs + 100000U);  // a second occurrence before the first's window closed: judge it now
        }
        _pending = occurrence;
        _havePending = true;
    }
    judgePending(blockMs);
}

void EnvelopeRuntime::judgePending(uint32_t nowMs) {
    if (!_havePending || !timing::atOrAfter(nowMs, static_cast<uint32_t>(_inspector.windowCompleteAtMs(_pending)))) {
        return;
    }
    _havePending = false;
    Result r;
    r.inspected = _inspector.inspectWithHistory(_pending, &_history, nowMs);
    if (!makeVerdict(r.inspected, _plan, r.verdict)) {
        return;
    }
    if (_resultCount == kResults) {
        for (size_t i = 1; i < kResults; ++i) {
            _results[i - 1] = _results[i];  // the oldest dropped: the caller takes results every block
        }
        --_resultCount;
    }
    _results[_resultCount++] = r;
}

bool EnvelopeRuntime::popResult(Result& out) {
    if (_resultCount == 0) {
        return false;
    }
    out = _results[0];
    for (size_t i = 1; i < _resultCount; ++i) {
        _results[i - 1] = _results[i];
    }
    --_resultCount;
    return true;
}

}  // namespace detection
