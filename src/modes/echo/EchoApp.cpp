#include "EchoApp.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <string.h>

#include <new>

#include "../../app/BuildInfo.h"
#include "../../app/FirmwareOta.h"
#include "../../app/NodeRoster.h"
#include "../../app/SerialOut.h"
#include "../../app/TextParse.h"
#include "../../app/TimeBase.h"
#include "../../audio/AudioPcm.h"
#include "../../detection/inspection/VerdictProjection.h"
#include "../../net/RemoteCommandPolicy.h"

using text_parse::equalsIgnoreCase;
using text_parse::startsWithWordIgnoreCase;

namespace {

// The Node's loop bound (ResonantNodeApp.cpp, kMaxSamplesPerLoop) scaled to the rate: one pass drains at most
// ~8 ms of audio, the length of the DMA queue at 16 kHz, so the console and the radio still get their turn.
constexpr uint32_t kMaxDrainMs = 8;
constexpr uint32_t kToneMaxMs = 10000;

uint32_t largestFreeBlock() {
    return static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

// The echo room's Behavior (roadmap-echo.md ECHO-001 item 3): no idle call, a first clap answered with 3 / 4, a short
// rest at the top of a small level, the answer after 350 ms (the clip's 270 ms after the onset recorded by then) plus a
// little jitter. Own-emit suppression covers the replay and a tail for the room's ring-down.
BehaviorGateConfig echoBehavior() {
    BehaviorGateConfig b;
    b.idleEnabled = false;
    b.waitAfterHeardMs = 350;
    b.refractoryAfterEmitMs = 300;
    b.behaviorSuppressSelfChirpMs = 100;
    b.detectionSuppressTailMsOwnEmit = 150;
    b.levelMax = 4;
    b.levelOffset = 2;
    b.levelDecayMs = 3000;
    b.restMs = 4000;
    b.answerJitterMs = 300;
    b.baseGlide = false;
    return b;
}

}  // namespace

EchoApp::EchoApp()
    : _i2sPort(runtime::kDefaultAudioI2SSckPin, runtime::kDefaultAudioI2SWsPin, runtime::kDefaultAudioI2SDataPin,
               static_cast<int>(runtime::kDefaultAudioI2SSampleRateHz),
               static_cast<int>(runtime::kDefaultAudioI2SBitsPerSample), runtime::kDefaultAudioI2SDataOutPin),
      _toneOutput(_i2sPort) {}

void EchoApp::begin() {
    printBuildIdentity(serialOut(), kWhispernet ? "whisper" : "echo");
    const roster::Entry* me = roster::find(ESP.getEfuseMac());
    _nodeName = me != nullptr ? me->name : "unknown";

    const uint32_t heapBeforeRing = largestFreeBlock();
    bool whisperReady = false;
    if (kWhispernet) {
        // Before the radio takes its heap: the line is one 96 KB block at 24 kHz.
        whisperReady = _whisper.begin(runtime::kDefaultAudioI2SSampleRateHz, ESP.getEfuseMac());
    } else {
        _ring.begin(static_cast<size_t>(runtime::kDefaultAudioI2SSampleRateHz) * ECHO_RING_MS / 1000U);
        _clipCapacity = static_cast<size_t>(runtime::kDefaultAudioI2SSampleRateHz) * 300U / 1000U;
        _clip.reset(new (std::nothrow) int16_t[_clipCapacity]);
        if (!_clip) {
            _clipCapacity = 0;
        }
    }
    _i2sPort.begin();
    _toneOutput.begin();
    _chirpOutput.begin();
    if (kWhispernet && whisperReady) {
        _whisper.attachOutput(_toneOutput);
    }
    _follower.setShift(EnvelopeFollower::shiftForRate(_i2sPort.sampleRateHz()));  // ~1.3 ms at any rate
    _behavior.resetState();
    configureEcho();
    const bool netUp = _net.begin();
    _netGateway.begin();

    serialOut().print("ECHO boot node=");
    serialOut().print(_nodeName);
    serialOut().print(" rate_hz=");
    serialOut().print(_i2sPort.sampleRateHz());
    serialOut().print(" dma_bufs=");
    serialOut().print(I2S_DMA_BUF_COUNT);
    serialOut().print(" dma_len=");
    serialOut().print(I2S_DMA_BUF_LEN);
    serialOut().print(" ring_ms=");
    serialOut().print(_ring.capacity() > 0 ? ECHO_RING_MS : 0);
    serialOut().print(" ring_bytes=");
    serialOut().print(static_cast<unsigned long>(_ring.capacity() * sizeof(int16_t)));
    serialOut().print(" clip_bytes=");
    serialOut().print(static_cast<unsigned long>(_clipCapacity * sizeof(int16_t)));
    serialOut().print(" largest_block_before_ring=");
    serialOut().print(heapBeforeRing);
    serialOut().print(" whisper_bytes=");
    serialOut().print(static_cast<unsigned long>(kWhispernet ? _whisper.bytes() : 0));
    serialOut().print(" delay_ms=");
    serialOut().print(kWhispernet ? _whisper.values().delayMs : 0);
    serialOut().print(" net_started=");
    serialOut().print(netUp ? 1 : 0);
    serialOut().print(" channel=");
    serialOut().println(NET_CHANNEL);
    if (kWhispernet ? !whisperReady : (_ring.capacity() == 0 || _clipCapacity == 0)) {
        serialOut().println(kWhispernet ? "ECHO ERR whisper_alloc_failed" : "ECHO ERR ring_or_clip_alloc_failed");
    }
    resetLoad();
    _nextLogMs = timebase::nowMs() + _logEveryMs;
}

void EchoApp::update() {
    const uint32_t passStartUs = micros();
    pollAudio();
    pollUsb();
    pollNet();

    const unsigned long now = timebase::nowMs();
    runBehavior(now);
    if (_toneOn && static_cast<long>(now - _toneOffAtMs) >= 0) {
        _toneOutput.toneOff();
        _toneOn = false;
    }
    if (_logEveryMs > 0 && static_cast<long>(now - _nextLogMs) >= 0) {
        printStatus();
        _nextLogMs = now + _logEveryMs;
    }
    ota::confirmRunningImage(serialOut(), _net.started());

    const uint32_t passUs = micros() - passStartUs;
    ++_load.passes;
    _load.passUsSum += passUs;
    if (passUs > _load.passUsMax) {
        _load.passUsMax = passUs;
    }
}

// The per-sample path both modes will run: rectify, smooth, store. Timed per block.
void EchoApp::pollAudio() {
    const size_t maxSamples = static_cast<size_t>(_i2sPort.sampleRateHz()) * kMaxDrainMs / 1000U;
    size_t processed = 0;
    AudioBlock block;
    while (processed < maxSamples && _i2sPort.readBlock(block)) {
        const uint32_t startUs = micros();
        // The ring's own index of this block's first sample: the clip is cut by it. Not block.startSampleIndex, which
        // also counts the DMA buffers the driver dropped (the ring never saw them; found on the desk 2026-10-31).
        const uint64_t ringIndex = _ring.written();
        const uint32_t blockMs = timebase::msFromUs64(block.approxStartUs64);
        if (kWhispernet) {
            _whisper.beginBlock(blockMs);
        }
        for (uint16_t i = 0; i < block.sampleCount; ++i) {
            const int32_t s = block.samples[i];
            _follower.push(s);
            if (kWhispernet) {
                _whisper.pushSample(static_cast<int16_t>(s >> 8));  // 24-bit canonical PCM to int16, D later out
            } else {
                _ring.push(static_cast<int16_t>(s >> 8));  // 24-bit canonical PCM to int16
            }
        }
        if (_follower.value() > _envelopePeak) {
            _envelopePeak = _follower.value();
        }
        detectBlock(block, ringIndex, blockMs);
        takeClip();
        const uint32_t blockUs = micros() - startUs;
        if (blockUs > _load.blockUsMax) {
            _load.blockUsMax = blockUs;
        }
        processed += block.sampleCount;
        _load.samples += block.sampleCount;
    }
}

void EchoApp::configureEcho() {
    if (kWhispernet) {
        _whisper.configure(_runtime, _behavior);
        return;
    }
    detection::ScalarTransientConfig d;
    d.observedStream = detection::FeatureStreamId::AmpEnvelope;
    d.onsetDetectionThreshold = _gate.onset;
    d.onsetReleaseThreshold = _gate.release;
    d.cooldownAfterOnsetMs = 50;
    d.minTransientDurationMs = 0;
    d.maxTransientDurationMs = 300;
    d.minTransientPeakStrength = _gate.minPeak;
    d.releaseDebounceMs = 20;
    // The echo room's plan: one module, the attack; Medium (within maxAttackMs) is a clap.
    detection::InspectionPlan plan = detection::InspectionPlan{};
    detection::InspectionModuleConfig& m = plan.modules[0];
    m.kind = detection::InspectionModuleKind::AttackRise;
    m.target = detection::InspectionTarget::Attack;
    m.enabled = true;
    m.minimumStrength = detection::StrengthClass::Medium;
    m.attack.stream = detection::FeatureStreamId::AmpEnvelope;
    m.attack.floorLevel = _gate.floor;
    m.attack.mediumMaxMs = _gate.maxAttackMs;
    m.attack.strongMaxMs = _gate.maxAttackMs / 2;
    m.attack.weakMaxMs = _gate.maxAttackMs * 3;
    plan.count = 1;
    _runtime.configure(d, plan);
    _behavior.configure(echoBehavior());
}

// One DMA block into the detection chain (EnvelopeRuntime); paused while the own replay sounds and for its tail
// (Behavior's own-emit suppression). Each judged occurrence goes to handleResult.
void EchoApp::detectBlock(const AudioBlock& block, uint64_t ringIndex, uint32_t blockMs) {
    _lastBlockIndex = ringIndex;
    _lastBlockMs = blockMs;
    if (_detectionPaused) {
        return;
    }
    _runtime.onBlock(_follower.strength(), block.approxStartUs64, blockMs);
    detection::EnvelopeRuntime::Result result;
    while (_runtime.popResult(result)) {
        if (kWhispernet) {
            _whisper.handleResult(result, _behavior, timebase::nowMs());
            continue;
        }
        ++_echo.occurrences;
        handleResult(result);
    }
    if (kWhispernet) {
        _whisper.afterBlock(_runtime, blockMs);
    }
}

// A judged occurrence: counted, handed to Behavior; a clap Behavior takes marks its clip to be copied.
void EchoApp::handleResult(const detection::EnvelopeRuntime::Result& result) {
    const detection::OccurrenceVerdict& verdict = result.verdict;
    if (verdict.valid) {
        ++_echo.claps;
    } else {
        ++_echo.slow;
    }
    // The onset's ring index, from the newest block's index and time (the clip is cut around it).
    const uint64_t rate = _i2sPort.sampleRateHz();
    const uint32_t backMs = _lastBlockMs - static_cast<uint32_t>(result.inspected.occurrence.startMs);
    const uint64_t back = static_cast<uint64_t>(backMs) * rate / 1000U;
    const uint64_t onset = _lastBlockIndex > back ? _lastBlockIndex - back : 0;
    ResonantBehavior::BehaviorDecision decision = ResonantBehavior::BehaviorDecision::None;
    if (_gate.enabled) {
        decision = _behavior.handleOccurrenceVerdict(verdict, timebase::nowMs());
        if (decision == ResonantBehavior::BehaviorDecision::ConsumedPattern) {
            _clipOnset = onset;
            _clipWanted = true;
            _clipReady = false;
        }
    }
    if (!verdict.valid) {
        return;  // counted; not printed (music made ~800 rejects in 7 min on the desk, and their lines cost the loop)
    }
    const detection::OccurrenceAttack& a = result.inspected.occurrence.attack;
    serialOut().print("ECHO clap valid=");
    serialOut().print(verdict.valid ? 1 : 0);
    serialOut().print(" attack_ms=");
    if (a.measured) {
        serialOut().print(a.riseMs);
    } else {
        serialOut().print("na");
    }
    serialOut().print(" strength=");
    serialOut().print(static_cast<unsigned long>(verdict.primaryStrength));
    serialOut().print(" duration_ms=");
    serialOut().print(verdict.primaryDurationMs);
    serialOut().print(" decision=");
    serialOut().println(ResonantBehavior::behaviorDecisionName(decision));
}

// Behavior's clock, the replay's end, and detection resuming clean after the own sound.
void EchoApp::runBehavior(unsigned long now) {
    if (kWhispernet) {
        // Suppression off: detection keeps running while the node plays (it hears its own replay as a phrase).
        _behavior.update(now);
        _whisper.runBehavior(_behavior, now);
        return;
    }
    const bool paused = _behavior.ownEmitDetectionSuppressed(now);
    if (_detectionPaused && !paused) {
        _runtime
            .restartAfterInputGap();  // an occurrence cut by the replay is dropped, as the Node drops one at its restart
    }
    _detectionPaused = paused;
    if (_soundOutput.update() & SoundOutput::kEventFinished) {
        _behavior.notifyChirpFinished(now);
    }
    _behavior.update(now);
    if (!_soundOutput.busy() && _behavior.shouldStartChirp()) {
        startReplay(now);
    }
}

// The clap's clip, preMs before its onset to postMs after, copied once the ring holds all of it.
void EchoApp::takeClip() {
    if (!_clipWanted || !_clip) {
        return;
    }
    const uint64_t rate = _i2sPort.sampleRateHz();
    const uint64_t pre = rate * _gate.preMs / 1000U;
    uint64_t count = rate * (_gate.preMs + _gate.postMs) / 1000U;
    if (count > _clipCapacity) {
        count = _clipCapacity;
    }
    const uint64_t from = _clipOnset > pre ? _clipOnset - pre : 0;
    if (_ring.written() < from + count) {
        return;  // the tail is not recorded yet
    }
    _clipWanted = false;
    _clipReady = _ring.copy(from, static_cast<size_t>(count), _clip.get());
    _clipCount = _clipReady ? static_cast<size_t>(count) : 0;
}

// The answer: the clap as it was heard.
void EchoApp::startReplay(unsigned long now) {
    const uint64_t rate = _i2sPort.sampleRateHz();
    OutputRequest request;
    request.kind = OutputRequest::Kind::Clip;
    request.clipSamples = _clip.get();
    request.clipCount = _clipCount;
    request.clipMs = static_cast<unsigned long>(static_cast<uint64_t>(_clipCount) * 1000U / (rate > 0 ? rate : 1));
    request.clipGain = _gate.gain;
    const bool cut = _clipReady;
    _clipReady = false;
    if (!cut || !_soundOutput.start(request)) {
        // Behavior asked; close its request so it leaves Chirping.
        if (!cut) {
            ++_echo.clipMissing;
        }
        _behavior.notifyChirpStarted(now, 0);
        _behavior.notifyChirpFinished(now);
        serialOut().println(cut ? "ECHO replay rejected" : "ECHO replay clip_missing");
        return;
    }
    ++_echo.replays;
    _behavior.notifyChirpStarted(now, request.clipMs);
    serialOut().print("ECHO replay onset_index=");
    serialOut().print(static_cast<unsigned long long>(_clipOnset));
    serialOut().print(" ms=");
    serialOut().print(request.clipMs);
    serialOut().print(" gain=");
    serialOut().println(_gate.gain, 2);
}

// ECHO GATE [onset=S16] [release=S16] [floor=S16] [attack=MS] [pre=MS] [post=MS] [gain=X] [on=0|1]: live values for the
// desk (not saved); prints them either way.
void EchoApp::handleGateCommand(const char* line) {
    const char* rest = text_parse::rest(line, "ECHO GATE");
    struct FloatKey {
        const char* key;
        float* field;
    } floats[] = {{"onset=", &_gate.onset},
                  {"release=", &_gate.release},
                  {"floor=", &_gate.floor},
                  {"gain=", &_gate.gain},
                  {"peak=", &_gate.minPeak}};
    char value[16] = {};
    for (const FloatKey& k : floats) {
        float v = 0.0f;
        if (text_parse::keyValue(rest, k.key, value, sizeof(value)) && text_parse::parseFiniteFloat(value, v) &&
            v >= 0.0f) {
            *k.field = v;
        }
    }
    struct UintKey {
        const char* key;
        uint32_t* field;
    } uints[] = {{"attack=", &_gate.maxAttackMs}, {"pre=", &_gate.preMs}, {"post=", &_gate.postMs}};
    for (const UintKey& k : uints) {
        uint32_t v = 0;
        if (text_parse::keyValue(rest, k.key, value, sizeof(value)) && text_parse::parseUnsigned(value, v)) {
            *k.field = v;
        }
    }
    if (_gate.preMs + _gate.postMs > 300) {
        _gate.postMs = _gate.preMs < 300 ? 300 - _gate.preMs : 0;  // the clip buffer holds 300 ms
    }
    if (text_parse::keyValue(rest, "on=", value, sizeof(value))) {
        _gate.enabled = value[0] != '0';
    }
    configureEcho();
    serialOut().print("ECHO gate onset=");
    serialOut().print(_gate.onset, 0);
    serialOut().print(" release=");
    serialOut().print(_gate.release, 0);
    serialOut().print(" floor=");
    serialOut().print(_gate.floor, 0);
    serialOut().print(" peak=");
    serialOut().print(_gate.minPeak, 0);
    serialOut().print(" attack_ms=");
    serialOut().print(_gate.maxAttackMs);
    serialOut().print(" pre_ms=");
    serialOut().print(_gate.preMs);
    serialOut().print(" post_ms=");
    serialOut().print(_gate.postMs);
    serialOut().print(" gain=");
    serialOut().print(_gate.gain, 2);
    serialOut().print(" on=");
    serialOut().println(_gate.enabled ? 1 : 0);
}

void EchoApp::pollUsb() {
    serial_line::pollLines(Serial, _usbLine, serialOut(),
                           "ERR line_too_long max=", [this](const char* line) { handleLine(line); });
}

// The receive side of the radio: a NET command from a USB Node runs here as if typed, its output goes back as the
// reply. One frame per pass and only with room in the UART ring, as on the Node (ResonantNodeNet.cpp).
void EchoApp::pollNet() {
    if (!_net.started()) {
        return;
    }
    _net.service(timebase::nowMs());
    NodeNet::Received received;
    if (Serial.availableForWrite() <= 4096 || !_net.poll(received)) {
        return;
    }
    const net::Frame& f = received.frame;
    if (f.type == net::FrameType::Reply || !net::targetMatches(f.target, _nodeName)) {
        return;
    }
    NetGateway::ReplyScope reply(_netGateway, received.mac, f.seq, _nodeName);
    if (equalsIgnoreCase(f.text, "NET PING")) {
        printNetStatus();
    } else if (startsWithWordIgnoreCase(f.text, "OTA") || startsWithWordIgnoreCase(f.text, "ECHO") ||
               startsWithWordIgnoreCase(f.text, "WHISPER")) {
        handleLine(f.text);
    } else {
        serialOut().println("NET ERR not_in_echo (OTA, ECHO STATUS, NET PING)");
    }
    ++_net.stats().commandsRun;
}

void EchoApp::handleLine(const char* line) {
    if (equalsIgnoreCase(line, "ECHO STATUS") || equalsIgnoreCase(line, "STATUS")) {
        printStatus();
    } else if (equalsIgnoreCase(line, "ECHO RESET")) {
        resetLoad();
        serialOut().println("ECHO reset");
    } else if (startsWithWordIgnoreCase(line, "ECHO GATE")) {
        handleGateCommand(line);
    } else if (kWhispernet && startsWithWordIgnoreCase(line, "WHISPER GATE")) {
        _whisper.handleGateCommand(line, _runtime, _behavior);
    } else if (kWhispernet && equalsIgnoreCase(line, "WHISPER STATUS")) {
        _whisper.printStatus();
    } else if (startsWithWordIgnoreCase(line, "ECHO LOG")) {
        handleLogCommand(line);
    } else if (startsWithWordIgnoreCase(line, "TONE")) {
        handleToneCommand(line);
    } else if (startsWithWordIgnoreCase(line, "OTA")) {
        handleOtaCommand(line);
    } else if (equalsIgnoreCase(line, "NET STATUS") || equalsIgnoreCase(line, "NET PING")) {
        printNetStatus();
    } else if (equalsIgnoreCase(line, "HELP")) {
        printHelp();
    } else {
        serialOut().println("ECHO ERR unknown_command (HELP)");
    }
}

// TONE <hz> <ms>: the writer task at the echo rate, for the load numbers and an ear check of the amp.
void EchoApp::handleToneCommand(const char* line) {
    const char* rest = text_parse::rest(line, "TONE");
    char hzText[12] = {};
    size_t n = 0;
    while (rest[n] != '\0' && rest[n] != ' ' && n < sizeof(hzText) - 1) {
        hzText[n] = rest[n];
        ++n;
    }
    const char* msText = rest + n;
    while (*msText == ' ') {
        ++msText;
    }
    uint32_t hz = 0;
    uint32_t ms = 0;
    if (!text_parse::parseUnsigned(hzText, hz) || !text_parse::parseUnsigned(msText, ms) || hz < 100 ||
        hz >= _i2sPort.sampleRateHz() / 2 || ms == 0 || ms > kToneMaxMs) {
        serialOut().println("ECHO ERR usage=TONE <hz 100..rate/2> <ms 1..10000>");
        return;
    }
    _toneOutput.setToneHz(hz);
    _toneOutput.toneOn();
    _toneOn = true;
    _toneOffAtMs = timebase::nowMs() + ms;
    serialOut().print("ECHO tone hz=");
    serialOut().print(hz);
    serialOut().print(" ms=");
    serialOut().println(ms);
}

void EchoApp::handleOtaCommand(const char* line) {
    // As on the Node (ResonantNodeCommands.cpp): silence, send what the outbox holds before WiFi takes the radio off
    // its channel, pull and reboot; on failure the radio goes back to its channel.
    _toneOutput.toneOff();
    _toneOn = false;
    if (kWhispernet) {
        _whisper.release();
    }
    if (_net.started()) {
        _net.flush(1500);
    }
    ota::runCommand(line, serialOut());
    if (kWhispernet && !_whisper.resume()) {  // the update failed: back to listening, if the heap still allows
        serialOut().println("ECHO ERR whisper_alloc_failed");
    }
    if (_net.started()) {
        _net.restoreChannel();
    }
}

// ECHO LOG <s>: the status line every s seconds, 0 = only on ECHO STATUS.
void EchoApp::handleLogCommand(const char* line) {
    const char* value = text_parse::rest(line, "ECHO LOG");
    uint32_t seconds = 0;
    if (!text_parse::parseUnsigned(value, seconds) || seconds > 3600) {
        serialOut().println("ECHO ERR usage=ECHO LOG <seconds 0..3600>");
        return;
    }
    _logEveryMs = seconds * 1000UL;
    _nextLogMs = timebase::nowMs() + _logEveryMs;
    serialOut().print("ECHO log every_s=");
    serialOut().println(seconds);
}

// One line, the go / no-go numbers: dropped DMA buffers since ECHO RESET (or boot), the loop's pass time, the
// per-block path, the samples read against the time (a rate below the nominal one means lost audio), the heap.
void EchoApp::printStatus() {
    const AudioSourceStats& st = _i2sPort.stats();
    const unsigned long elapsedMs = timebase::nowMs() - _load.startMs;
    serialOut().print("ECHO status node=");
    serialOut().print(_nodeName);
    serialOut().print(" uptime_s=");
    serialOut().print(timebase::nowMs() / 1000UL);
    serialOut().print(" window_s=");
    serialOut().print(elapsedMs / 1000UL);
    serialOut().print(" rate_hz=");
    serialOut().print(_i2sPort.sampleRateHz());
    serialOut().print(" measured_mhz=");
    serialOut().print(st.sampleClockRateMilliHz);
    serialOut().print(" samples=");
    serialOut().print(static_cast<unsigned long long>(_load.samples));
    serialOut().print(" dropped_bufs=");
    serialOut().print(st.droppedDmaBuffers - _load.droppedAtStart);
    serialOut().print(" dropped_total=");
    serialOut().print(st.droppedDmaBuffers);
    serialOut().print(" passes=");
    serialOut().print(_load.passes);
    serialOut().print(" pass_us_mean=");
    serialOut().print(_load.passes > 0 ? static_cast<unsigned long>(_load.passUsSum / _load.passes) : 0UL);
    serialOut().print(" pass_us_max=");
    serialOut().print(_load.passUsMax);
    serialOut().print(" block_us_max=");
    serialOut().print(_load.blockUsMax);
    serialOut().print(" env_peak=");
    serialOut().print(_envelopePeak);
    serialOut().print(" tone=");
    serialOut().print(_toneOn ? 1 : 0);
    serialOut().print(" free_heap=");
    serialOut().print(static_cast<unsigned long>(ESP.getFreeHeap()));
    serialOut().print(" largest_block=");
    serialOut().print(largestFreeBlock());
    serialOut().print(" net_rx=");
    serialOut().print(_net.stats().rxFrames);
    serialOut().print(" net_tx=");
    serialOut().print(_net.stats().txFrames);
    serialOut().print(" level=");
    serialOut().print(static_cast<unsigned long>(_follower.strength()));
    serialOut().print(" occurrences=");
    serialOut().print(_echo.occurrences);
    serialOut().print(" claps=");
    serialOut().print(_echo.claps);
    serialOut().print(" slow=");
    serialOut().print(_echo.slow);
    serialOut().print(" replays=");
    serialOut().print(_echo.replays);
    serialOut().print(" clip_missing=");
    serialOut().print(_echo.clipMissing);
    serialOut().print(" behavior=");
    serialOut().print(_behavior.stateName());
    serialOut().print(" level_auto=");
    serialOut().println(_behavior.level(timebase::nowMs()));
    _envelopePeak = 0;
    if (kWhispernet) {
        _whisper.printStatus();
    }
}

void EchoApp::printNetStatus() {
    const NodeNet::Stats& st = _net.stats();
    serialOut().print("NET node=");
    serialOut().print(_nodeName);
    serialOut().print(" role=echo started=");
    serialOut().print(_net.started() ? 1 : 0);
    serialOut().print(" build=");
    serialOut().print(BUILD_GIT_SHA);
    serialOut().print(" uptime_s=");
    serialOut().print(timebase::nowMs() / 1000UL);
    serialOut().print(" rx=");
    serialOut().print(st.rxFrames);
    serialOut().print(" tx=");
    serialOut().print(st.txFrames);
    serialOut().print(" commands=");
    serialOut().println(st.commandsRun);
}

void EchoApp::printHelp() {
    serialOut().println("ECHO CMD: ECHO STATUS (also STATUS)");
    serialOut().println("ECHO CMD: ECHO RESET (the load counters)");
    serialOut().println("ECHO CMD: ECHO LOG <seconds> (status every n s, 0 = off)");
    serialOut().println(
        "ECHO CMD: ECHO GATE [onset=] [release=] [floor=] [peak=] [attack=MS] [pre=MS] [post=MS] [gain=] [on=0|1]");
    if (kWhispernet) {
        serialOut().println("ECHO CMD: WHISPER STATUS");
        serialOut().println(
            "ECHO CMD: WHISPER GATE [onset=] [release=] [floor=] [loud=] [mod=] [delay=MS 400..2000] [gain=] [on=0|1]");
    }
    serialOut().println("ECHO CMD: TONE <hz> <ms>");
    serialOut().println("ECHO CMD: OTA url=... token=...");
    serialOut().println("ECHO CMD: NET STATUS");
    serialOut().println("ECHO radio: NET <name|*> OTA ... / ECHO STATUS / NET PING from a USB Node");
}

void EchoApp::resetLoad() {
    _load = Load{};
    _load.droppedAtStart = _i2sPort.stats().droppedDmaBuffers;
    _load.startMs = timebase::nowMs();
}
