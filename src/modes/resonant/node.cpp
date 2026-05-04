#include "node.h"
#include <Arduino.h>

#ifndef CHIRP_FREQUENCY_HZ
#define CHIRP_FREQUENCY_HZ 2400
#endif

namespace {
constexpr int kMaxSamplesPerLoop = 128;
}

Node::Node(int inputPin, int ledPin, int chirpPin, AudioSourceKind sourceKind)
    : Node(inputPin, ledPin, chirpPin, -1, sourceKind) {}

Node::Node(int inputPin, int ledPin, int chirpPin, int chirpBtlPin, AudioSourceKind sourceKind)
    : _ledPin(ledPin),
      _analogSource(inputPin),
      _i2sSource(14, 27, 33, 16000, 32),
      _sourceKind(sourceKind),
      _audioSource(sourceKind == AudioSourceKind::I2S
                       ? static_cast<AudioSource&>(_i2sSource)
                       : static_cast<AudioSource&>(_analogSource)),
      _audioSignal(_audioSource),
      _audioOnsetDetector(),
      _toneOutput(chirpPin),
      _toneOutputBTL(chirpPin, chirpBtlPin),
      _chirpOutput(chirpBtlPin >= 0
                       ? static_cast<ToneOutput&>(_toneOutputBTL)
                       : static_cast<ToneOutput&>(_toneOutput)) {}

void Node::begin() {
    configureParameters();
    _audioSource.begin();
    _audioSource.resetStats();
    _audioSignal.begin();
    _audioOnsetDetector.begin();
    _chirpOutput.begin();
    _debug.begin(_ledPin);
    _debug.markLoopStart(micros());
}

void Node::configureParameters() {
    configureSharedParameters();

    if (_sourceKind == AudioSourceKind::I2S) {
        configureI2SParameters();
    } else {
        configureAnalogParameters();
    }
}

void Node::configureSharedParameters() {
    // Signal conditioning shared across sources.
    _audioSignal.setSmoothingFactor(0.5f);
    _audioSignal.setBaselineUpdateFactor(0.005f);

    // Chirp tone is configured here so the node owns its output tuning.
    // The fallback/default frequency is defined by CHIRP_FREQUENCY_HZ.
    _chirpOutput.setToneHz(2000);
}

void Node::configureAnalogParameters() {
    _audioSignal.setBaselineTrackingQuietThreshold(40);
    _audioOnsetDetector.setOnsetDetectionThreshold(36.0f);
    _audioOnsetDetector.setOnsetReleaseThreshold(26.0f);
    _audioOnsetDetector.setCooldownAfterOnsetMs(300);
    _audioOnsetDetector.setReleaseDebounceMs(30);
    _audioOnsetDetector.setMinTransientDurationMs(60);
    _audioOnsetDetector.setMaxTransientDurationMs(240);
    _audioOnsetDetector.setMinTransientPeakStrength(40.0f);

    _behavior.setWaitAfterTransientMs(500); // Shared response timing: wait after a transient before emit.
    _behavior.setRefractoryAfterEmitMs(500); // Shared response timing: post-emit holdoff.
    _behavior.setIdleTimeoutMs(10000); // Shared response timing: idle self-trigger timeout.
}

void Node::configureI2SParameters() {
    _audioSignal.setBaselineTrackingQuietThreshold(20);
    _audioOnsetDetector.setOnsetDetectionThreshold(36.0f);
    _audioOnsetDetector.setOnsetReleaseThreshold(26.0f);
    _audioOnsetDetector.setCooldownAfterOnsetMs(300);
    _audioOnsetDetector.setReleaseDebounceMs(30);
    _audioOnsetDetector.setMinTransientDurationMs(60);
    _audioOnsetDetector.setMaxTransientDurationMs(240);
    _audioOnsetDetector.setMinTransientPeakStrength(40.0f);

    _behavior.setWaitAfterTransientMs(500); // Shared response timing: wait after a transient before emit.
    _behavior.setRefractoryAfterEmitMs(500); // Shared response timing: post-emit holdoff.
    _behavior.setIdleTimeoutMs(10000); // Shared response timing: idle self-trigger timeout.
}

void Node::update() {
    const unsigned long now = millis();
    const unsigned long nowUs = micros();
    const bool selfChirpSuppressed = _behavior.selfChirpSuppressed(now);

    _debug.noteCoreLoopUs(nowUs);

    // Update input first, then detection, then behavior, so each layer sees the
    // latest state from the layer below it.
    if (selfChirpSuppressed) {
        _audioOnsetDetector.resetState();
    }
    int processedSamples = 0;
    bool sawI2SSample = false;
    if (_sourceKind == AudioSourceKind::I2S) {
        AudioBlock block;
        while (processedSamples < kMaxSamplesPerLoop && _i2sSource.readBlock(block)) {
            if (block.sampleCount == 0 || block.samples == nullptr) {
                break;
            }

            for (uint16_t i = 0; i < block.sampleCount && processedSamples < kMaxSamplesPerLoop; ++i) {
                const uint32_t sampleTimeUs = block.approxStartMicros + static_cast<uint32_t>(static_cast<uint32_t>(i) * _i2sSource.samplePeriodUs());
                const int sample = static_cast<int>(block.samples[i]);
                _audioSignal.update(sample, sampleTimeUs);
                sawI2SSample = true;

                if (!selfChirpSuppressed) {
                    _audioOnsetDetector.update(static_cast<float>(_audioSignal.signalMagnitude()), sampleTimeUs);
                }

                const bool onsetDetected = selfChirpSuppressed ? false : _audioOnsetDetector.onsetDetected();
                _debug.observeOnset(now, onsetDetected, _audioOnsetDetector.onsetStrength());
                _debug.observeTransient(now, _audioOnsetDetector.transientDetected(), _audioOnsetDetector.transientStrength(), selfChirpSuppressed);

                const bool transientDetected = selfChirpSuppressed ? false : _audioOnsetDetector.transientDetected();
                const float transientStrength = selfChirpSuppressed ? 0.0f : _audioOnsetDetector.transientStrength();
                _behavior.update(transientDetected, transientStrength, now);

                processedSamples++;
            }
        }
    } else {
        while (processedSamples < kMaxSamplesPerLoop && _audioSource.available()) {
            int sample = 0;
            uint32_t sampleTimeUs = 0;
            if (!_audioSource.readSample(sample, sampleTimeUs)) {
                break;
            }
            _audioSignal.update(sample, sampleTimeUs);

            if (!selfChirpSuppressed) {
                _audioOnsetDetector.update(static_cast<float>(_audioSignal.signalMagnitude()), sampleTimeUs);
            }

            const bool onsetDetected = selfChirpSuppressed ? false : _audioOnsetDetector.onsetDetected();
            _debug.observeOnset(now, onsetDetected, _audioOnsetDetector.onsetStrength());
            _debug.observeTransient(now, _audioOnsetDetector.transientDetected(), _audioOnsetDetector.transientStrength(), selfChirpSuppressed);

            const bool transientDetected = selfChirpSuppressed ? false : _audioOnsetDetector.transientDetected();
            const float transientStrength = selfChirpSuppressed ? 0.0f : _audioOnsetDetector.transientStrength();
            _behavior.update(transientDetected, transientStrength, now);

            processedSamples++;
        }
    }

    if (_sourceKind == AudioSourceKind::I2S && sawI2SSample) {
        _debug.observeI2SSignal(now, _audioSignal);
    }

    if (_behavior.shouldStartChirp()) {
        const auto chirpPattern = _behavior.chirpPattern();
        const char* sourceName = _behavior.chirpRequestSourceName();
        _debug.observeChirpStarted(now, sourceName, chirpPattern);
        _chirpOutput.start(chirpPattern);
        _behavior.notifyChirpStarted(now);
    }

    _chirpOutput.update();

    if (_chirpOutput.finished()) {
        _behavior.notifyChirpFinished(now);
        _debug.observeChirpFinished(now);
    }

    _debug.updateLed(now, _behavior, _chirpOutput, selfChirpSuppressed);

    _debug.endLoop(micros());
}
