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
#include "../../net/RemoteCommandPolicy.h"

using text_parse::equalsIgnoreCase;
using text_parse::startsWithWordIgnoreCase;

namespace {

// The Node's loop bound (ResonantNodeApp.cpp, kMaxSamplesPerLoop) scaled to the rate: one pass drains at most
// ~8 ms of audio, the length of the DMA queue at 16 kHz, so the console and the radio still get their turn.
constexpr uint32_t kMaxDrainMs = 8;
// One-pole smoother for the envelope, as a shift: ~1.3 ms at 48 kHz (2^6 samples), the attack an onset gate needs.
constexpr int kEnvelopeShift = 6;
constexpr uint32_t kToneMaxMs = 10000;

uint32_t largestFreeBlock() {
    return static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

}  // namespace

EchoApp::EchoApp()
    : _i2sPort(runtime::kDefaultAudioI2SSckPin, runtime::kDefaultAudioI2SWsPin, runtime::kDefaultAudioI2SDataPin,
               static_cast<int>(runtime::kDefaultAudioI2SSampleRateHz),
               static_cast<int>(runtime::kDefaultAudioI2SBitsPerSample), runtime::kDefaultAudioI2SDataOutPin),
      _toneOutput(_i2sPort) {}

void EchoApp::begin() {
    printBuildIdentity(serialOut(), "echo");
    const roster::Entry* me = roster::find(ESP.getEfuseMac());
    _nodeName = me != nullptr ? me->name : "unknown";

    const uint32_t heapBeforeRing = largestFreeBlock();
    _ringSamples = static_cast<size_t>(runtime::kDefaultAudioI2SSampleRateHz) * ECHO_RING_MS / 1000U;
    _ring.reset(new (std::nothrow) int16_t[_ringSamples]);
    if (!_ring) {
        _ringSamples = 0;
    }
    _i2sPort.begin();
    _toneOutput.begin();
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
    serialOut().print(_ringSamples > 0 ? ECHO_RING_MS : 0);
    serialOut().print(" ring_bytes=");
    serialOut().print(static_cast<unsigned long>(_ringSamples * sizeof(int16_t)));
    serialOut().print(" largest_block_before_ring=");
    serialOut().print(heapBeforeRing);
    serialOut().print(" net_started=");
    serialOut().print(netUp ? 1 : 0);
    serialOut().print(" channel=");
    serialOut().println(NET_CHANNEL);
    if (_ringSamples == 0) {
        serialOut().println("ECHO ERR ring_alloc_failed");
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
        for (uint16_t i = 0; i < block.sampleCount; ++i) {
            const int32_t s = block.samples[i];
            const int32_t magnitude = s < 0 ? -s : s;
            _envelope += (magnitude - _envelope) >> kEnvelopeShift;
            if (_ringSamples > 0) {
                _ring[_ringHead] = static_cast<int16_t>(s >> 8);  // 24-bit canonical PCM to int16
                if (++_ringHead == _ringSamples) {
                    _ringHead = 0;
                }
            }
        }
        if (_envelope > _envelopePeak) {
            _envelopePeak = _envelope;
        }
        const uint32_t blockUs = micros() - startUs;
        if (blockUs > _load.blockUsMax) {
            _load.blockUsMax = blockUs;
        }
        processed += block.sampleCount;
        _load.samples += block.sampleCount;
    }
}

void EchoApp::pollUsb() {
    serial_line::pollLines(Serial, _usbLine, serialOut(), "ERR line_too_long max=",
                           [this](const char* line) { handleLine(line); });
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
    } else if (startsWithWordIgnoreCase(f.text, "OTA") || startsWithWordIgnoreCase(f.text, "ECHO")) {
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
    if (_net.started()) {
        _net.flush(1500);
    }
    ota::runCommand(line, serialOut());
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
    serialOut().println(_net.stats().txFrames);
    _envelopePeak = 0;
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
