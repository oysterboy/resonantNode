#include "Whispernet.h"

#include <string.h>

#include "../../app/SerialOut.h"
#include "../../app/TextParse.h"
#include "../../app/TimingUtils.h"

namespace {

constexpr uint32_t kLineMs = 2000;  // the line's capacity: D's top
constexpr uint32_t kDelayMinMs =
    400;  // the gate needs ~400 ms to open (decisions/2026-11-01-whispernet-delay-per-node.md)
constexpr uint32_t kFifoMs = 100;
constexpr uint32_t kPrimeMs = 20;  // the writer starts once this waits: the loop pushes in bursts of up to 8 ms
constexpr uint32_t kRampMs = 10;

// Whispernet's Behavior (roadmap-echo.md ECHO-002 item 4): an answered phrase is passed at once (no wait, no jitter: D
// is the spread), the node keeps listening while it plays (suppression off); the level climbs with every phrase heard
// and every replay, and the rest at its top shuts the gate for a while, so a room of whisperers goes quiet. No idle
// call. The echo room's level and rest to start with.
BehaviorGateConfig whisperBehavior() {
    BehaviorGateConfig b;
    b.idleEnabled = false;
    b.waitAfterHeardMs = 0;
    b.refractoryAfterEmitMs = 0;
    b.behaviorSuppressSelfChirpMs = 0;
    b.detectionSuppressTailMsOwnEmit = 0;
    b.levelMax = 4;
    b.levelOffset = 2;
    b.levelDecayMs = 3000;
    b.restMs = 4000;
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
    _values.delayMs = delayForSeed(seed);
    const bool line = _line.begin(static_cast<size_t>(rateHz) * kLineMs / 1000U, 128, rateHz * kRampMs / 1000U);
    const bool fifo = _fifo.begin(static_cast<size_t>(rateHz) * kFifoMs / 1000U);
    _line.setDelay(static_cast<size_t>(rateHz) * _values.delayMs / 1000U);
    _gate.setDelayMs(_values.delayMs);
    return line && fifo;
}

void Whispernet::attachOutput(I2sToneOutput& output) {
    _output = &output;
    output.setStream(&_fifo, _values.gain, static_cast<size_t>(_rateHz) * kPrimeMs / 1000U);
}

void Whispernet::configure(detection::EnvelopeRuntime& runtime, ResonantBehavior& behavior) {
    // A phrase is one occurrence: syllable gaps bridged by the release debounce, no length limit; it is judged while
    // it runs (the provisional verdicts), its final verdict only ends it.
    detection::ScalarTransientConfig d;
    d.observedStream = detection::FeatureStreamId::AmpEnvelope;
    d.onsetDetectionThreshold = _values.onset;
    d.onsetReleaseThreshold = _values.release;
    d.cooldownAfterOnsetMs = 0;
    d.maxTransientDurationMs = 0;
    d.minTransientDurationMs = 300;
    d.minTransientPeakStrength = 0.0f;
    d.releaseDebounceMs = 300;
    detection::InspectionPlan plan = detection::InspectionPlan{};
    detection::InspectionModuleConfig& level = plan.modules[0];
    level.kind = detection::InspectionModuleKind::LevelWindow;
    level.target = detection::InspectionTarget::Loudness;
    level.enabled = true;
    level.minimumStrength = detection::StrengthClass::Strong;
    level.level.floorLevel = _values.floor;
    level.level.loudBound = _values.loud;
    detection::InspectionModuleConfig& mod = plan.modules[1];
    mod.kind = detection::InspectionModuleKind::Modulation;
    mod.target = detection::InspectionTarget::Modulation;
    mod.enabled = true;
    mod.minimumStrength = detection::StrengthClass::Medium;
    mod.modulation.mediumIndex = _values.modulation;
    plan.count = 2;
    runtime.configure(d, plan);
    runtime.setProvisional(300, 100);
    behavior.configure(whisperBehavior());
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
        _line.pass(lineIndexAtMs(step.fromMs), _line.written());
        _line.setOpen(true);
        _openedAtMs = step.fromMs;
        ++_counts.opens;
    } else if (step.event == WhisperGate::Event::Close) {
        _line.setOpen(false);
        _counts.gatedMs += step.fromMs - _openedAtMs;
        _playedOutAtMs = _blockMs + _values.delayMs + kRampMs;  // what passed up to now plays within D
    }
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
            serialOut().print("WHISPER heard after_ms=");
            serialOut().print(oc.durationMs);
            serialOut().print(" level_p90=");
            serialOut().print(static_cast<unsigned long>(oc.level.p90));
            serialOut().print(" mod=");
            serialOut().print(oc.modulation.index, 2);
            serialOut().print(" decision=");
            serialOut().println(ResonantBehavior::behaviorDecisionName(decision));
        }
        apply(step);
        return;
    }
    ++_counts.phrases;
    _counts.whispers += result.verdict.valid ? 1 : 0;
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

void Whispernet::handleGateCommand(const char* line, detection::EnvelopeRuntime& runtime, ResonantBehavior& behavior) {
    const char* rest = text_parse::rest(line, "WHISPER GATE");
    struct FloatKey {
        const char* key;
        float* field;
    } floats[] = {{"onset=", &_values.onset}, {"release=", &_values.release}, {"floor=", &_values.floor},
                  {"loud=", &_values.loud},   {"mod=", &_values.modulation},  {"gain=", &_values.gain}};
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
    _line.setDelay(static_cast<size_t>(_rateHz) * _values.delayMs / 1000U);
    _gate.setDelayMs(_values.delayMs);
    if (_output != nullptr) {
        _output->setStream(&_fifo, _values.gain, static_cast<size_t>(_rateHz) * kPrimeMs / 1000U);
    }
    configure(runtime, behavior);
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
    serialOut().print(" delay_ms=");
    serialOut().print(_values.delayMs);
    serialOut().print(" gain=");
    serialOut().print(_values.gain, 2);
    serialOut().print(" on=");
    serialOut().println(_values.enabled ? 1 : 0);
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
    serialOut().print(" gate_open=");
    serialOut().print(_line.isOpen() ? 1 : 0);
    serialOut().print(" fifo_wait=");
    serialOut().print(static_cast<unsigned long>(_fifo.available()));
    serialOut().print(" fifo_dropped=");
    serialOut().print(_fifo.dropped());
    serialOut().print(" underruns=");
    serialOut().print(_fifo.underruns());
    serialOut().print(" line_bytes=");
    serialOut().println(static_cast<unsigned long>(bytes()));
}
