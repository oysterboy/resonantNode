#include "io/AudioFrequencyDetector.h"

#include <Arduino.h>

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------

AudioFrequencyDetector::AudioFrequencyDetector(AudioSignal& audioSignal)
    : _audioSignal(audioSignal) {
    _streamExtractor.setTargetFrequencyHz(3200);
    _streamExtractor.setSampleRateHz(16000);
    _streamExtractor.setWindowSizeSamples(64);
    _transientDetector.setDiagnosticsLabel("EVT freq");
}

void AudioFrequencyDetector::begin() {
    resetState();
}

void AudioFrequencyDetector::resetState() {
    _streamExtractor.resetState();
    _transientDetector.resetState();
}

// -----------------------------------------------------------------------------
// Runtime update and sample window management
// -----------------------------------------------------------------------------

void AudioFrequencyDetector::update(unsigned long now) {
    observeCenteredSample(_audioSignal.centeredSignal());
    if (_streamExtractor.windowSizeSamples() > 0) {
        _transientDetector.update(_streamExtractor.lastFrequencyScore(), now * 1000UL);
    }
}

void AudioFrequencyDetector::observeCenteredSample(int centeredSample) {
    _streamExtractor.observeCenteredSample(centeredSample);
}

bool AudioFrequencyDetector::onsetDetected() const {
    return _transientDetector.onsetDetected();
}

float AudioFrequencyDetector::onsetStrength() const {
    return _transientDetector.onsetStrength();
}

bool AudioFrequencyDetector::transientDetected() const {
    return _transientDetector.transientDetected();
}

float AudioFrequencyDetector::transientStrength() const {
    return _transientDetector.transientStrength();
}

unsigned long AudioFrequencyDetector::transientDurationMs() const {
    return _transientDetector.transientDurationMs();
}

float AudioFrequencyDetector::lastFrequencyScore() const {
    return _streamExtractor.lastFrequencyScore();
}

float AudioFrequencyDetector::lastTargetPower() const {
    return _streamExtractor.lastTargetPower();
}

float AudioFrequencyDetector::lastNeighborPower() const {
    return _streamExtractor.lastNeighborPower();
}

float AudioFrequencyDetector::lastTotalEnergy() const {
    return _streamExtractor.lastTotalEnergy();
}

float AudioFrequencyDetector::lastSpectralContrast() const {
    return _streamExtractor.lastSpectralContrast();
}

float AudioFrequencyDetector::frequencyBinSpacingHz() const {
    return _streamExtractor.frequencyBinSpacingHz();
}

float AudioFrequencyDetector::onsetDetectionThreshold() const {
    return _transientDetector.onsetDetectionThreshold();
}

float AudioFrequencyDetector::onsetReleaseThreshold() const {
    return _transientDetector.onsetReleaseThreshold();
}

unsigned long AudioFrequencyDetector::cooldownAfterOnsetMs() const {
    return _transientDetector.cooldownAfterOnsetMs();
}

unsigned long AudioFrequencyDetector::minTransientDurationMs() const {
    return _transientDetector.minTransientDurationMs();
}

unsigned long AudioFrequencyDetector::maxTransientDurationMs() const {
    return _transientDetector.maxTransientDurationMs();
}

float AudioFrequencyDetector::minTransientPeakStrength() const {
    return _transientDetector.minTransientPeakStrength();
}

unsigned long AudioFrequencyDetector::releaseDebounceMs() const {
    return _transientDetector.releaseDebounceMs();
}

unsigned long AudioFrequencyDetector::targetFrequencyHz() const {
    return _streamExtractor.targetFrequencyHz();
}

unsigned long AudioFrequencyDetector::sampleRateHz() const {
    return _streamExtractor.sampleRateHz();
}

unsigned long AudioFrequencyDetector::windowSizeSamples() const {
    return _streamExtractor.windowSizeSamples();
}

unsigned long AudioFrequencyDetector::streamSampleCount() const {
    return _streamExtractor.sampleCount();
}

bool AudioFrequencyDetector::streamWindowReady() const {
    return _streamExtractor.windowReady();
}

void AudioFrequencyDetector::setOnsetDetectionThreshold(float value) {
    _transientDetector.setOnsetDetectionThreshold(value);
}

void AudioFrequencyDetector::setOnsetReleaseThreshold(float value) {
    _transientDetector.setOnsetReleaseThreshold(value);
}

void AudioFrequencyDetector::setCooldownAfterOnsetMs(unsigned long value) {
    _transientDetector.setCooldownAfterOnsetMs(value);
}

void AudioFrequencyDetector::setReleaseDebounceMs(unsigned long value) {
    _transientDetector.setReleaseDebounceMs(value);
}

void AudioFrequencyDetector::setMinTransientDurationMs(unsigned long value) {
    _transientDetector.setMinTransientDurationMs(value);
}

void AudioFrequencyDetector::setMaxTransientDurationMs(unsigned long value) {
    _transientDetector.setMaxTransientDurationMs(value);
}

void AudioFrequencyDetector::setMinTransientPeakStrength(float value) {
    _transientDetector.setMinTransientPeakStrength(value);
}

void AudioFrequencyDetector::setDiagnosticsEnabled(bool enabled) {
    _transientDetector.setDiagnosticsEnabled(enabled);
}

void AudioFrequencyDetector::setTargetFrequencyHz(unsigned long value) {
    _streamExtractor.setTargetFrequencyHz(value);
}

void AudioFrequencyDetector::setSampleRateHz(unsigned long value) {
    _streamExtractor.setSampleRateHz(value);
}

void AudioFrequencyDetector::setWindowSizeSamples(unsigned long value) {
    _streamExtractor.setWindowSizeSamples(value);
}
