#include "io/AudioOnsetDetector.h"

AudioOnsetDetector::AudioOnsetDetector() {
    _detector.setDiagnosticsLabel("EVT");
}

void AudioOnsetDetector::begin() {
    _detector.begin();
}

void AudioOnsetDetector::resetState() {
    _detector.resetState();
}

void AudioOnsetDetector::update(float signalMagnitude, uint32_t sampleTimeUs) {
    _detector.update(signalMagnitude, sampleTimeUs);
}

void AudioOnsetDetector::setOnsetDetectionThreshold(float value) {
    _detector.setOnsetDetectionThreshold(value);
}

void AudioOnsetDetector::setOnsetReleaseThreshold(float value) {
    _detector.setOnsetReleaseThreshold(value);
}

void AudioOnsetDetector::setCooldownAfterOnsetMs(unsigned long value) {
    _detector.setCooldownAfterOnsetMs(value);
}

void AudioOnsetDetector::setMinTransientDurationMs(unsigned long value) {
    _detector.setMinTransientDurationMs(value);
}

void AudioOnsetDetector::setMaxTransientDurationMs(unsigned long value) {
    _detector.setMaxTransientDurationMs(value);
}

void AudioOnsetDetector::setMinTransientPeakStrength(float value) {
    _detector.setMinTransientPeakStrength(value);
}

void AudioOnsetDetector::setReleaseDebounceMs(unsigned long value) {
    _detector.setReleaseDebounceMs(value);
}

void AudioOnsetDetector::setDiagnosticsEnabled(bool enabled) {
    _detector.setDiagnosticsEnabled(enabled);
}

bool AudioOnsetDetector::onsetDetected() const {
    return _detector.onsetDetected();
}

float AudioOnsetDetector::onsetStrength() const {
    return _detector.onsetStrength();
}

const char* AudioOnsetDetector::lastOnsetRejectReasonName() const {
    return _detector.lastOnsetRejectReasonName();
}

bool AudioOnsetDetector::transientDetected() const {
    return _detector.transientDetected();
}

float AudioOnsetDetector::transientStrength() const {
    return _detector.transientStrength();
}

unsigned long AudioOnsetDetector::transientDurationMs() const {
    return _detector.transientDurationMs();
}

bool AudioOnsetDetector::peakActive() const {
    return _detector.peakActive();
}

float AudioOnsetDetector::peakStrength() const {
    return _detector.peakStrength();
}

const char* AudioOnsetDetector::lastTransientRejectReasonName() const {
    return _detector.lastTransientRejectReasonName();
}

unsigned long AudioOnsetDetector::lastTransientRejectedDurationMs() const {
    return _detector.lastTransientRejectedDurationMs();
}

float AudioOnsetDetector::lastTransientRejectedStrength() const {
    return _detector.lastTransientRejectedStrength();
}

unsigned long AudioOnsetDetector::onsetRejectedCount() const {
    return _detector.onsetRejectedCount();
}

unsigned long AudioOnsetDetector::transientRejectedCount() const {
    return _detector.transientRejectedCount();
}

unsigned long AudioOnsetDetector::transientRejectedDurationTooShortCount() const {
    return _detector.transientRejectedDurationTooShortCount();
}

unsigned long AudioOnsetDetector::transientRejectedDurationTooLongCount() const {
    return _detector.transientRejectedDurationTooLongCount();
}

unsigned long AudioOnsetDetector::transientRejectedStrengthTooLowCount() const {
    return _detector.transientRejectedStrengthTooLowCount();
}

float AudioOnsetDetector::onsetDetectionThreshold() const {
    return _detector.onsetDetectionThreshold();
}

float AudioOnsetDetector::onsetReleaseThreshold() const {
    return _detector.onsetReleaseThreshold();
}

unsigned long AudioOnsetDetector::cooldownAfterOnsetMs() const {
    return _detector.cooldownAfterOnsetMs();
}

unsigned long AudioOnsetDetector::minTransientDurationMs() const {
    return _detector.minTransientDurationMs();
}

unsigned long AudioOnsetDetector::maxTransientDurationMs() const {
    return _detector.maxTransientDurationMs();
}

float AudioOnsetDetector::minTransientPeakStrength() const {
    return _detector.minTransientPeakStrength();
}

unsigned long AudioOnsetDetector::releaseDebounceMs() const {
    return _detector.releaseDebounceMs();
}
