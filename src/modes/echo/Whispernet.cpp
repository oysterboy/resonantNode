#include "Whispernet.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "../../app/SerialOut.h"
#include "../../app/TextParse.h"
#include "../../app/TimingUtils.h"
#include "../../app/ParamIds.h"
#include "../../detection/inspection/InspectionNames.h"

namespace {

constexpr uint32_t kLineMs = 2000;  // the line's capacity: D's top
constexpr uint32_t kDelayMinMs =
    400;  // the gate needs ~400 ms to open (decisions/2026-11-01-whispernet-delay-per-node.md)

// Whispernet's Behavior (roadmap-echo.md ECHO-002 item 4): an answered phrase is passed at once (no wait, no jitter: D
// is the spread), the node keeps listening while it plays (suppression off); the level climbs with every phrase heard
// and every replay, and the rest at its top shuts the gate for a while, so a room of whisperers goes quiet. No idle
// call. The echo room's level and rest to start with.
BehaviorGateConfig whisperBehavior(const Whispernet::Values& v) {
    BehaviorGateConfig b;
    b.idleEnabled = false;
    b.waitAfterHeardMs = 0;
    b.refractoryAfterEmitMs = 0;
    b.behaviorSuppressSelfChirpMs = 0;
    b.detectionSuppressTailMsOwnEmit = 0;
    b.levelMax = v.levelMax > 0 ? v.levelMax : 1;
    b.levelOffset = v.levelOffset;
    b.levelDecayMs = v.levelDecayMs;
    b.restMs = v.restMs;
    b.answerJitterMs = 0;
    b.baseGlide = false;
    return b;
}

// D from the chip id: spread over 400 .. 2000 ms, the same on every boot of one board.
uint32_t delayForSeed(uint64_t seed) {
    uint64_t h = seed * 0x9E3779B97F4A7C15ULL;
    h ^= h >> 31;
    return kDelayMinMs + static_cast<uint32_t>(h % (kLineMs - kDelayMinMs + 1U));
}

}  // namespace

bool Whispernet::begin(uint32_t rateHz, uint64_t seed) {
    _rateHz = rateHz;
    _seed = seed;
    _values.delayMs = delayForSeed(seed);
    const bool line = _line.begin(static_cast<size_t>(rateHz) * kLineMs / 1000U, 128, rateHz * _values.rampMs / 1000U);
    _line.setDelay(static_cast<size_t>(rateHz) * _values.delayMs / 1000U);
    _gate.setDelayMs(_values.delayMs);
    _flat.begin(rateHz);
    return line;
}

void Whispernet::attachDuplex(DuplexStream& duplex) {
    _duplex = &duplex;
    duplex.setGain(_values.gain);
}

void Whispernet::release() {
    _line.end();
    _gate.reset();
}

bool Whispernet::resume() {
    const uint32_t delayMs = _values.delayMs;  // a WHISPER GATE delay= survives
    const bool ok = begin(_rateHz, _seed);
    _values.delayMs = delayMs;
    _line.setDelay(static_cast<size_t>(_rateHz) * delayMs / 1000U);
    _gate.setDelayMs(delayMs);
    return ok;
}

// The onset in force: the set onset, or roomFactor x the room's level when that is higher (adaptive).
float Whispernet::effectiveOnset() const {
    if (!_values.adaptive || !_room.started()) {
        return _values.onset;
    }
    const float byRoom = _values.roomFactor * _room.value();
    return byRoom > _values.onset ? byRoom : _values.onset;
}

// The detector's config and the plan at the onset in force; release and floor keep their ratio to the set onset.
void Whispernet::buildConfig(detection::ScalarTransientConfig& d, detection::InspectionPlan& plan) const {
    // A phrase is one occurrence: syllable gaps bridged by the release debounce, no length limit; it is judged while
    // it runs (the provisional verdicts), its final verdict only ends it.
    const float onset = effectiveOnset();
    const float scale = _values.onset > 0.0f ? onset / _values.onset : 1.0f;
    d.observedStream = detection::FeatureStreamId::AmpEnvelope;
    d.onsetDetectionThreshold = onset;
    d.onsetReleaseThreshold = _values.release * scale;
    d.cooldownAfterOnsetMs = 0;
    d.maxTransientDurationMs = 0;
    d.minTransientDurationMs = _values.minPhraseMs;
    d.minTransientPeakStrength = 0.0f;
    d.releaseDebounceMs = _values.gapMs;
    plan = detection::InspectionPlan{};
    detection::InspectionModuleConfig& level = plan.modules[0];
    level.kind = detection::InspectionModuleKind::LevelWindow;
    level.target = detection::InspectionTarget::Loudness;
    level.enabled = true;
    level.minimumStrength = detection::StrengthClass::Strong;
    level.level.floorLevel = _values.floor * scale;
    level.level.loudBound = _values.loud;
    detection::InspectionModuleConfig& mod = plan.modules[1];
    mod.kind = detection::InspectionModuleKind::Modulation;
    mod.target = detection::InspectionTarget::Modulation;
    mod.enabled = true;
    mod.minimumStrength = detection::StrengthClass::Medium;
    mod.modulation.mediumIndex = _values.modulation;
    detection::InspectionModuleConfig& timbre = plan.modules[2];
    timbre.kind = detection::InspectionModuleKind::Flatness;
    timbre.target = detection::InspectionTarget::Timbre;
    timbre.enabled = true;
    timbre.minimumStrength = detection::StrengthClass::Strong;
    timbre.flatness.minFlatness = _values.flatMin;
    timbre.flatness.maxFlatness = _values.flatMax;
    plan.count = 3;
}

void Whispernet::configure(detection::EnvelopeRuntime& runtime, ResonantBehavior& behavior) {
    _room.setTimes(_values.roomRiseMs, _values.roomFallMs);
    detection::ScalarTransientConfig d;
    detection::InspectionPlan plan;
    buildConfig(d, plan);
    runtime.configure(d, plan);
    _onsetApplied = d.onsetDetectionThreshold;
    runtime.setProvisional(_values.firstCheckMs, _values.checkEveryMs);
    behavior.configure(whisperBehavior(_values));
}

// The room moved the onset by more than 5 %: the thresholds follow, the history and an open phrase kept.
void Whispernet::retune(detection::EnvelopeRuntime& runtime) {
    const float onset = effectiveOnset();
    if (onset > _onsetApplied * 0.95f && onset < _onsetApplied * 1.05f) {
        return;
    }
    detection::ScalarTransientConfig d;
    detection::InspectionPlan plan;
    buildConfig(d, plan);
    runtime.retune(d, plan);
    _onsetApplied = d.onsetDetectionThreshold;
}

void Whispernet::endBlock(detection::EnvelopeRuntime& runtime, float level, uint32_t blockMs) {
    if (_holdPending && timing::atOrAfter(blockMs, _holdUntilMs)) {
        shutLine();
    }
    // The room's level, learned while nothing runs: no phrase open, the line shut, no own playback ahead or sounding.
    const bool learning = !_gate.following() && !runtime.candidateOpen() && !_line.isOpen() && !_playArmed;
    _room.update(level, blockMs > _lastRoomMs ? blockMs - _lastRoomMs : 0U, learning);
    _lastRoomMs = blockMs;
    if (_values.adaptive && learning) {
        retune(runtime);
    }
    float flatness = 0.0f;
    if (_flat.take(flatness) && level >= _onsetApplied) {
        runtime.recordFeature(detection::FeatureStreamId::SpectralFlatness, blockMs, flatness);
    }
}

// The block's place in the mic's index (the DuplexStream records what it played under those indices) and, once the
// canceller has learned for half a second, the auto-centring: the direct sound should sit early in the window; at its
// far end the echo comes later than the delay says (move the window older), at its very start earlier.
void Whispernet::beginBlock(uint32_t blockMs, uint64_t blockUs, uint64_t rxIndex, uint16_t samples) {
    _blockIndex = _line.written();
    _blockMs = blockMs;
    _rxCursor = rxIndex;
    _outCount = 0;
    _rxEnd = rxIndex + samples;
    _rxEndUs = blockUs + static_cast<uint64_t>(samples) * 1000000ULL / _rateHz;
    if (!_values.aec || _aec.taps() == 0) {
        return;
    }
    if (timing::atOrAfter(blockMs, _nextCentreCheckMs) && _aec.adapted() > _rateHz / 2U) {
        _nextCentreCheckMs = blockMs + 2000U;
        const size_t peak = _aec.peakTap();
        const uint32_t taps = static_cast<uint32_t>(_aec.taps());
        if (peak + 8 >= taps) {
            _aecDelayNow += taps / 2;
            _aec.reset();
            ++_centreShifts;
        } else if (peak <= 2 && _aecDelayNow > taps / 4) {
            _aecDelayNow -= taps / 4;
            _aec.reset();
            ++_centreShifts;
        }
    }
}

// The canceller's reference: the sample written for the mic index the delay back, 32 samples newer (the lead keeps
// the direct sound inside the window).
int32_t Whispernet::cancel(int32_t mic) {
    const uint64_t m = _rxCursor++;
    if (!_values.aec || _duplex == nullptr || _aec.taps() == 0) {
        return mic;
    }
    int16_t r = 0;
    const uint64_t back = _aecDelayNow > 32 ? _aecDelayNow - 32 : 0;
    if (m >= back) {
        _duplex->played(m - back, r);  // silence when not written (dropped buffers, the start)
    }
    const float e = _aec.process(static_cast<float>(r) / 32768.0f, static_cast<float>(mic) / 8388608.0f) * 8388608.0f;
    return static_cast<int32_t>(e > 8388607.0f ? 8388607.0f : (e < -8388608.0f ? -8388608.0f : e));
}

uint64_t Whispernet::lineIndexAtMs(unsigned long ms) const {
    if (static_cast<long>(_blockMs - ms) <= 0) {
        return _blockIndex;
    }
    const uint64_t back = static_cast<uint64_t>(_blockMs - ms) * _rateHz / 1000U;
    return _blockIndex > back ? _blockIndex - back : 0;
}

void Whispernet::apply(const WhisperGate::Step& step) {
    if (step.event == WhisperGate::Event::Open) {
        const unsigned long from = step.fromMs > _values.preMs ? step.fromMs - _values.preMs : 0UL;
        _line.pass(lineIndexAtMs(from), _line.written());
        _line.setOpen(true);
        _holdPending = false;
        _openedAtMs = step.fromMs;
        if (!_playArmed) {
            _playArmed = true;
            _playFromMs = from + _values.delayMs;  // the first passed sample's turn
        }
        ++_counts.opens;
    } else if (step.event == WhisperGate::Event::Close) {
        _counts.gatedMs += step.fromMs - _openedAtMs;
        _holdPending = true;  // shut after the tail (endBlock)
        _holdUntilMs = _blockMs + _values.holdMs;
    }
}

void Whispernet::shutLine() {
    _holdPending = false;
    if (_line.isOpen()) {
        _line.setOpen(false);
        _playedOutAtMs = _blockMs + _values.delayMs + _values.rampMs;  // what passed up to now plays within D
        _playUntilMs = _playedOutAtMs + _values.tailMs;
    }
}

bool Whispernet::inputSuppressed(uint32_t blockMs) {
    if (!_playArmed) {
        return false;
    }
    if (_values.aec && _values.listenWhilePlaying && _aec.erleDb() >= _values.listenErleDb) {
        return false;  // it hears the room, not itself: no need to go deaf
    }
    if (!_line.isOpen() && !_holdPending && timing::atOrAfter(blockMs, _playUntilMs)) {
        _playArmed = false;  // played out
        return false;
    }
    return timing::atOrAfter(blockMs, _playFromMs);
}

void Whispernet::suppressNow() {
    if (_gate.following()) {
        ++_counts.cuts;  // a phrase still running: its rest is not passed
    }
    apply(_gate.onPhraseEnd(_blockMs));  // the phrase ends here for the gate: what follows holds the own replay
    shutLine();
}

void Whispernet::handleResult(const detection::EnvelopeRuntime::Result& result, ResonantBehavior& behavior,
                              unsigned long now) {
    const detection::Occurrence& oc = result.inspected.occurrence;
    if (result.provisional) {
        ++_counts.checks;
        const bool tooLoud = oc.level.measured && oc.level.p90 > _values.loud;
        WhisperGate::Step step = _gate.onVerdict(oc.startMs, result.verdict.valid, tooLoud, _blockMs);
        if (step.event == WhisperGate::Event::Ask) {
            ++_counts.asked;
            ResonantBehavior::BehaviorDecision decision = ResonantBehavior::BehaviorDecision::None;
            if (_values.enabled) {
                decision = behavior.handleOccurrenceVerdict(result.verdict, now);
            }
            const bool answer = decision == ResonantBehavior::BehaviorDecision::ConsumedPattern;
            _counts.answered += answer ? 1 : 0;
            step = _gate.decide(answer);
            if (_recArmed > 0 && !_recPending) {  // WHISPER REC: this phrase, from 300 ms before its start
                --_recArmed;
                _recPending = true;
                const uint64_t start = lineIndexAtMs(oc.startMs);
                const uint64_t pre = static_cast<uint64_t>(_rateHz) * 3U / 10U;
                _recFrom = start > pre ? start - pre : 0;
                _recDueMs = _blockMs + 1200;
                snprintf(_recNote, sizeof(_recNote), "after_ms=%lu level_p90=%lu mod=%.2f flat=%.2f decision=%s",
                         static_cast<unsigned long>(oc.durationMs), static_cast<unsigned long>(oc.level.p90),
                         static_cast<double>(oc.modulation.index), static_cast<double>(oc.timbre.flatness),
                         ResonantBehavior::behaviorDecisionName(decision));
            }
            serialOut().print("WHISPER heard after_ms=");
            serialOut().print(oc.durationMs);
            serialOut().print(" level_p90=");
            serialOut().print(static_cast<unsigned long>(oc.level.p90));
            serialOut().print(" mod=");
            serialOut().print(oc.modulation.index, 2);
            serialOut().print(" flat=");
            serialOut().print(oc.timbre.flatness, 2);
            serialOut().print(" decision=");
            serialOut().println(ResonantBehavior::behaviorDecisionName(decision));
        }
        apply(step);
        return;
    }
    ++_counts.phrases;
    _counts.whispers += result.verdict.valid ? 1 : 0;
    // Every phrase, passed or not: what the checks read over its last 512 ms (the desk sets the bounds from these).
    serialOut().print("WHISPER phrase dur_ms=");
    serialOut().print(oc.durationMs);
    serialOut().print(" level_p90=");
    serialOut().print(static_cast<unsigned long>(oc.level.p90));
    serialOut().print(" mod=");
    serialOut().print(oc.modulation.index, 2);
    serialOut().print(" flat=");
    if (oc.timbre.measured) {
        serialOut().print(oc.timbre.flatness, 2);
    } else {
        serialOut().print("na");
    }
    serialOut().print(" valid=");
    serialOut().print(result.verdict.valid ? 1 : 0);
    serialOut().print(" failed=");
    serialOut().println(
        result.verdict.valid ? "none" : detection::inspectionTargetName(result.verdict.firstFailedRequirementTarget));
    apply(_gate.onPhraseEnd(oc.startMs + oc.durationMs));
}

void Whispernet::afterBlock(const detection::EnvelopeRuntime& runtime, uint32_t blockMs) {
    if (_gate.following() && !runtime.candidateOpen()) {
        apply(_gate.onPhraseEnd(blockMs));  // the detector dropped the phrase without a verdict
    }
}

void Whispernet::runBehavior(ResonantBehavior& behavior, unsigned long now) {
    if (!_emitting && behavior.shouldStartChirp()) {
        behavior.notifyChirpStarted(now, _values.delayMs);
        _emitting = true;
        if (_line.isOpen()) {
            _playedOutAtMs = 0;  // set when the gate shuts
        }
        ++_counts.replays;
    }
    // Finished once the gate has shut and its last passed sample has played (D later).
    if (_emitting && !_line.isOpen() && _playedOutAtMs != 0 && timing::atOrAfter(now, _playedOutAtMs)) {
        behavior.notifyChirpFinished(now);
        _emitting = false;
    }
}

// whisper.* (ECHO-003): every Values field, in the order PARAM LIST prints them.
void Whispernet::describeParams(param::ParamRegistry& registry) {
    registry.addFloat(param::ParamId::Whisper_Onset, param::ModuleId::Detection, "whisper.onset", &_values.onset, 0.0f,
                      32767.0f);
    registry.addFloat(param::ParamId::Whisper_Release, param::ModuleId::Detection, "whisper.release", &_values.release,
                      0.0f, 32767.0f);
    registry.addFloat(param::ParamId::Whisper_Floor, param::ModuleId::Detection, "whisper.floor", &_values.floor, 0.0f,
                      32767.0f);
    registry.addFloat(param::ParamId::Whisper_Loud, param::ModuleId::Detection, "whisper.loud", &_values.loud, 0.0f,
                      32767.0f);
    registry.addFloat(param::ParamId::Whisper_Modulation, param::ModuleId::Detection, "whisper.modulation",
                      &_values.modulation, 0.0f, 2.0f);
    registry.addFloat(param::ParamId::Whisper_FlatMin, param::ModuleId::Detection, "whisper.flat_min", &_values.flatMin,
                      0.0f, 1.0f);
    registry.addFloat(param::ParamId::Whisper_FlatMax, param::ModuleId::Detection, "whisper.flat_max", &_values.flatMax,
                      0.0f, 1.0f);
    registry.addUInt32(param::ParamId::Whisper_DelayMs, param::ModuleId::Output, "whisper.delay_ms", &_values.delayMs,
                       kDelayMinMs, kLineMs);
    registry.addFloat(param::ParamId::Whisper_Gain, param::ModuleId::Output, "whisper.gain", &_values.gain, 0.0f,
                      512.0f);
    registry.addBool(param::ParamId::Whisper_Enabled, param::ModuleId::Output, "whisper.enabled", &_values.enabled);
    registry.addUInt32(param::ParamId::Whisper_PreMs, param::ModuleId::Output, "whisper.pre_ms", &_values.preMs, 0U,
                       1000U);
    registry.addUInt32(param::ParamId::Whisper_HoldMs, param::ModuleId::Output, "whisper.hold_ms", &_values.holdMs, 0U,
                       2000U);
    registry.addUInt32(param::ParamId::Whisper_TailMs, param::ModuleId::Output, "whisper.tail_ms", &_values.tailMs, 0U,
                       3000U);
    registry.addUInt32(param::ParamId::Whisper_RampMs, param::ModuleId::Output, "whisper.ramp_ms", &_values.rampMs, 1U,
                       500U);
    registry.addUInt32(param::ParamId::Whisper_FirstCheckMs, param::ModuleId::Detection, "whisper.first_check_ms",
                       &_values.firstCheckMs, 100U, 2000U);
    registry.addUInt32(param::ParamId::Whisper_CheckEveryMs, param::ModuleId::Detection, "whisper.check_every_ms",
                       &_values.checkEveryMs, 20U, 1000U);
    registry.addUInt32(param::ParamId::Whisper_MinPhraseMs, param::ModuleId::Detection, "whisper.min_phrase_ms",
                       &_values.minPhraseMs, 0U, 5000U);
    registry.addUInt32(param::ParamId::Whisper_GapMs, param::ModuleId::Detection, "whisper.gap_ms", &_values.gapMs, 0U,
                       2000U);
    registry.addUInt32(param::ParamId::Whisper_LevelMax, param::ModuleId::Behavior, "whisper.level_max",
                       &_values.levelMax, 1U, 50U);
    registry.addUInt32(param::ParamId::Whisper_LevelOffset, param::ModuleId::Behavior, "whisper.level_offset",
                       &_values.levelOffset, 0U, 50U);
    registry.addUInt32(param::ParamId::Whisper_LevelDecayMs, param::ModuleId::Behavior, "whisper.level_decay_ms",
                       &_values.levelDecayMs, 0U, 60000U);
    registry.addUInt32(param::ParamId::Whisper_RestMs, param::ModuleId::Behavior, "whisper.rest_ms", &_values.restMs,
                       0U, 60000U);
    registry.addBool(param::ParamId::Whisper_Adaptive, param::ModuleId::Detection, "whisper.adaptive",
                     &_values.adaptive);
    registry.addFloat(param::ParamId::Whisper_RoomFactor, param::ModuleId::Detection, "whisper.room_factor",
                      &_values.roomFactor, 1.0f, 20.0f);
    registry.addUInt32(param::ParamId::Whisper_RoomRiseMs, param::ModuleId::Detection, "whisper.room_rise_ms",
                       &_values.roomRiseMs, 100U, 600000U);
    registry.addBool(param::ParamId::Whisper_Aec, param::ModuleId::Detection, "whisper.aec", &_values.aec);
    registry.addUInt32(param::ParamId::Whisper_AecTaps, param::ModuleId::Detection, "whisper.aec_taps",
                       &_values.aecTaps, 16U, 512U);
    registry.addFloat(param::ParamId::Whisper_AecMu, param::ModuleId::Detection, "whisper.aec_mu", &_values.aecMu,
                      0.001f, 1.0f);
    registry.addUInt32(param::ParamId::Whisper_AecDelay, param::ModuleId::Detection, "whisper.aec_delay",
                       &_values.aecDelay, 32U, 4000U);
    registry.addBool(param::ParamId::Whisper_ListenWhilePlaying, param::ModuleId::Detection,
                     "whisper.listen_while_playing", &_values.listenWhilePlaying);
    registry.addFloat(param::ParamId::Whisper_ListenErleDb, param::ModuleId::Detection, "whisper.listen_erle_db",
                      &_values.listenErleDb, 0.0f, 60.0f);
    registry.addUInt32(param::ParamId::Whisper_RoomFallMs, param::ModuleId::Detection, "whisper.room_fall_ms",
                       &_values.roomFallMs, 10U, 60000U);
}

void Whispernet::applyValues(detection::EnvelopeRuntime& runtime, ResonantBehavior& behavior) {
    _values.delayMs =
        _values.delayMs < kDelayMinMs ? kDelayMinMs : (_values.delayMs > kLineMs ? kLineMs : _values.delayMs);
    _line.setDelay(static_cast<size_t>(_rateHz) * _values.delayMs / 1000U);
    _line.setRamp(static_cast<size_t>(_rateHz) * _values.rampMs / 1000U);
    if (_values.aec && _aec.taps() != _values.aecTaps) {
        _aec.begin(_values.aecTaps);
    }
    if (_aecDelaySet != _values.aecDelay) {  // a new delay param restarts the centring from it
        _aecDelaySet = _values.aecDelay;
        _aecDelayNow = _values.aecDelay;
        _aec.reset();
    }
    _aec.setStep(_values.aecMu);
    _gate.setDelayMs(_values.delayMs);
    if (_duplex != nullptr) {
        _duplex->setGain(_values.gain);
    }
    configure(runtime, behavior);
}

void Whispernet::handleGateCommand(const char* line, detection::EnvelopeRuntime& runtime, ResonantBehavior& behavior) {
    const char* rest = text_parse::rest(line, "WHISPER GATE");
    struct FloatKey {
        const char* key;
        float* field;
    } floats[] = {{"onset=", &_values.onset},     {"release=", &_values.release}, {"floor=", &_values.floor},
                  {"loud=", &_values.loud},       {"mod=", &_values.modulation},  {"flat=", &_values.flatMax},
                  {"flatmin=", &_values.flatMin}, {"gain=", &_values.gain}};
    char value[16] = {};
    for (const FloatKey& k : floats) {
        float v = 0.0f;
        if (text_parse::keyValue(rest, k.key, value, sizeof(value)) && text_parse::parseFiniteFloat(value, v) &&
            v >= 0.0f) {
            *k.field = v;
        }
    }
    uint32_t ms = 0;
    if (text_parse::keyValue(rest, "delay=", value, sizeof(value)) && text_parse::parseUnsigned(value, ms)) {
        _values.delayMs = ms < kDelayMinMs ? kDelayMinMs : (ms > kLineMs ? kLineMs : ms);
    }
    if (text_parse::keyValue(rest, "on=", value, sizeof(value))) {
        _values.enabled = value[0] != '0';
    }
    applyValues(runtime, behavior);
    serialOut().print("WHISPER gate onset=");
    serialOut().print(_values.onset, 0);
    serialOut().print(" release=");
    serialOut().print(_values.release, 0);
    serialOut().print(" floor=");
    serialOut().print(_values.floor, 0);
    serialOut().print(" loud=");
    serialOut().print(_values.loud, 0);
    serialOut().print(" mod=");
    serialOut().print(_values.modulation, 2);
    serialOut().print(" flatmin=");
    serialOut().print(_values.flatMin, 2);
    serialOut().print(" flat=");
    serialOut().print(_values.flatMax, 2);
    serialOut().print(" delay_ms=");
    serialOut().print(_values.delayMs);
    serialOut().print(" gain=");
    serialOut().print(_values.gain, 2);
    serialOut().print(" on=");
    serialOut().println(_values.enabled ? 1 : 0);
}

void Whispernet::handleRecCommand(const char* line) {
    const char* rest = text_parse::rest(line, "WHISPER REC");
    unsigned long n = 0;
    if (text_parse::equalsIgnoreCase(rest, "now")) {
        const uint64_t span = static_cast<uint64_t>(_rateHz) * 3U / 2U;
        _recFrom = _line.written() > span ? _line.written() - span : 0;
        _recDueMs = _blockMs;
        _recPending = true;
        snprintf(_recNote, sizeof(_recNote), "manual=1");
    } else if (text_parse::parseUnsigned(rest, n) && n <= 50) {
        _recArmed = n;
    } else {
        serialOut().println("WHISPER ERR usage=WHISPER REC <n 0..50>|now");
        return;
    }
    serialOut().print("WHISPER rec armed=");
    serialOut().print(_recArmed);
    serialOut().print(" pending=");
    serialOut().println(_recPending ? 1 : 0);
}

bool Whispernet::recordingDue(unsigned long now) const {
    return _recPending && timing::atOrAfter(now, _recDueMs);
}

// The recorded span as base64 of little-endian int16, 96 characters a line (72 bytes), between a header and an end.
void Whispernet::dumpRecording() {
    static const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    _recPending = false;
    const uint64_t to = _line.written();
    const uint64_t from = _recFrom + _line.capacity() > to ? _recFrom : to - _line.capacity() + 1;
    const uint64_t count = to > from ? to - from : 0;
    ++_recSeq;
    serialOut().print("WHISPER_REC_BEGIN seq=");
    serialOut().print(_recSeq);
    serialOut().print(" rate=");
    serialOut().print(_rateHz);
    serialOut().print(" samples=");
    serialOut().print(static_cast<unsigned long>(count));
    serialOut().print(" from_index=");
    serialOut().print(static_cast<unsigned long long>(from));
    serialOut().print(" ");
    serialOut().println(_recNote);
    char out[100];
    uint8_t bytes[72];
    size_t nBytes = 0;
    const auto flush = [&]() {
        size_t o = 0;
        for (size_t i = 0; i < nBytes; i += 3) {
            const uint32_t v = (static_cast<uint32_t>(bytes[i]) << 16) |
                (i + 1 < nBytes ? static_cast<uint32_t>(bytes[i + 1]) << 8 : 0U) |
                (i + 2 < nBytes ? static_cast<uint32_t>(bytes[i + 2]) : 0U);
            out[o++] = kB64[(v >> 18) & 63];
            out[o++] = kB64[(v >> 12) & 63];
            out[o++] = i + 1 < nBytes ? kB64[(v >> 6) & 63] : '=';
            out[o++] = i + 2 < nBytes ? kB64[v & 63] : '=';
        }
        out[o] = '\0';
        serialOut().print("WHISPER_REC ");
        serialOut().println(out);
        nBytes = 0;
    };
    for (uint64_t i = from; i < to; ++i) {
        int16_t s = 0;
        _line.sampleAt(i, s);
        bytes[nBytes++] = static_cast<uint8_t>(static_cast<uint16_t>(s) & 0xFF);
        bytes[nBytes++] = static_cast<uint8_t>(static_cast<uint16_t>(s) >> 8);
        if (nBytes == sizeof(bytes)) {
            flush();
        }
    }
    if (nBytes > 0) {
        flush();
    }
    serialOut().print("WHISPER_REC_END seq=");
    serialOut().println(_recSeq);
}

namespace {

// Base64 of little-endian int16, 96 characters a line, each line "<tag> <data>".
template <typename Get>
void printBase64(const char* tag, uint64_t count, Get get) {
    static const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char out[100];
    uint8_t bytes[72];
    size_t n = 0;
    const auto flush = [&]() {
        size_t o = 0;
        for (size_t i = 0; i < n; i += 3) {
            const uint32_t v = (static_cast<uint32_t>(bytes[i]) << 16) |
                (i + 1 < n ? static_cast<uint32_t>(bytes[i + 1]) << 8 : 0U) |
                (i + 2 < n ? static_cast<uint32_t>(bytes[i + 2]) : 0U);
            out[o++] = kB64[(v >> 18) & 63];
            out[o++] = kB64[(v >> 12) & 63];
            out[o++] = i + 1 < n ? kB64[(v >> 6) & 63] : '=';
            out[o++] = i + 2 < n ? kB64[v & 63] : '=';
        }
        out[o] = 0;
        serialOut().print(tag);
        serialOut().print(" ");
        serialOut().println(out);
        n = 0;
    };
    for (uint64_t i = 0; i < count; ++i) {
        const int16_t s = get(i);
        bytes[n++] = static_cast<uint8_t>(static_cast<uint16_t>(s) & 0xFF);
        bytes[n++] = static_cast<uint8_t>(static_cast<uint16_t>(s) >> 8);
        if (n == sizeof(bytes)) {
            flush();
        }
    }
    if (n > 0) {
        flush();
    }
}

}  // namespace

bool Whispernet::calibrationDue(unsigned long now) const {
    return _calArmed && timing::atOrAfter(now, _calStartMs + 250U);
}

// Both on the mic's index: the mic (the line, raw with whisper.aec 0) and what was written for those indices, n each,
// ending at the last block: the cross-correlation's lag is the speaker's delay behind the mic, in samples.
void Whispernet::dumpCalibration() {
    _calArmed = false;
    if (_duplex == nullptr) {
        return;
    }
    const uint64_t lineEnd = _line.written();
    const uint64_t rxEnd = _rxEnd;
    const uint64_t n = _duplex->recordFrames() < 4096 ? _duplex->recordFrames() : 4096;
    serialOut().print("WHISPER_CAL_BEGIN rate=");
    serialOut().print(_rateHz);
    serialOut().print(" n=");
    serialOut().print(static_cast<unsigned long>(n));
    serialOut().print(" rx_end=");
    serialOut().print(static_cast<unsigned long long>(rxEnd));
    serialOut().print(" queue=");
    serialOut().print(static_cast<unsigned long>(DuplexStream::kQueueFrames));
    serialOut().print(" aec_delay=");
    serialOut().print(_aecDelayNow);
    serialOut().print(" aec=");
    serialOut().println(_values.aec ? 1 : 0);
    printBase64("WHISPER_CAL_MIC", n, [&](uint64_t i) {
        int16_t v = 0;
        _line.sampleAt(lineEnd - n + i, v);
        return v;
    });
    printBase64("WHISPER_CAL_REF", n, [&](uint64_t i) {
        int16_t v = 0;
        _duplex->played(rxEnd - n + i, v);
        return v;
    });
    serialOut().println("WHISPER_CAL_END");
}

void Whispernet::printStatus() {
    serialOut().print("WHISPER status delay_ms=");
    serialOut().print(_values.delayMs);
    serialOut().print(" phrases=");
    serialOut().print(_counts.phrases);
    serialOut().print(" whispers=");
    serialOut().print(_counts.whispers);
    serialOut().print(" checks=");
    serialOut().print(_counts.checks);
    serialOut().print(" asked=");
    serialOut().print(_counts.asked);
    serialOut().print(" answered=");
    serialOut().print(_counts.answered);
    serialOut().print(" opens=");
    serialOut().print(_counts.opens);
    serialOut().print(" gated_ms=");
    serialOut().print(_counts.gatedMs);
    serialOut().print(" replays=");
    serialOut().print(_counts.replays);
    serialOut().print(" deafs=");
    serialOut().print(_counts.deafs);
    serialOut().print(" deaf_ms=");
    serialOut().print(_counts.deafMs);
    serialOut().print(" cuts=");
    serialOut().print(_counts.cuts);
    serialOut().print(" gate_open=");
    serialOut().print(_line.isOpen() ? 1 : 0);
    serialOut().print(" short_writes=");
    serialOut().print(_duplex != nullptr ? _duplex->shortWrites() : 0);
    serialOut().print(" refills=");
    serialOut().print(_duplex != nullptr ? _duplex->refills() : 0);
    serialOut().print(" room=");
    serialOut().print(static_cast<unsigned long>(_room.value()));
    serialOut().print(" onset_now=");
    serialOut().print(static_cast<unsigned long>(_onsetApplied));
    serialOut().print(" aec=");
    serialOut().print(_values.aec ? 1 : 0);
    serialOut().print(" aec_erle_db=");
    serialOut().print(_aec.erleDb(), 1);
    serialOut().print(" aec_peak_tap=");
    serialOut().print(static_cast<unsigned long>(_aec.taps() > 0 ? _aec.peakTap() : 0));
    serialOut().print(" aec_delay=");
    serialOut().print(_aecDelayNow);
    serialOut().print(" aec_shifts=");
    serialOut().print(_centreShifts);
    serialOut().print(" line_bytes=");
    serialOut().println(static_cast<unsigned long>(bytes()));
}
