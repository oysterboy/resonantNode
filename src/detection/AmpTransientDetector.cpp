#include "AmpTransientDetector.h"

AmpTransientDetector::AmpTransientDetector() {
    begin();
}

void AmpTransientDetector::begin() {
    _detector.begin();
}

void AmpTransientDetector::resetState() {
    _detector.resetState();
}

void AmpTransientDetector::update(float signalLevel, uint32_t sampleTimeUs) {
    _detector.update(signalLevel, sampleTimeUs);
}

void AmpTransientDetector::setOnsetDetectionThreshold(float value) {
    _detector.setOnsetDetectionThreshold(value);
}

void AmpTransientDetector::setOnsetReleaseThreshold(float value) {
    _detector.setOnsetReleaseThreshold(value);
}

void AmpTransientDetector::setCooldownAfterOnsetMs(unsigned long value) {
    _detector.setCooldownAfterOnsetMs(value);
}

void AmpTransientDetector::setMinTransientDurationMs(unsigned long value) {
    _detector.setMinTransientDurationMs(value);
}

void AmpTransientDetector::setMaxTransientDurationMs(unsigned long value) {
    _detector.setMaxTransientDurationMs(value);
}

void AmpTransientDetector::setMinTransientPeakStrength(float value) {
    _detector.setMinTransientPeakStrength(value);
}

void AmpTransientDetector::setReleaseDebounceMs(unsigned long value) {
    _detector.setReleaseDebounceMs(value);
}

void AmpTransientDetector::setDiagnosticsEnabled(bool enabled) {
    _detector.setDiagnosticsEnabled(enabled);
}

bool AmpTransientDetector::onsetDetected() const {
    return _detector.onsetDetected();
}

float AmpTransientDetector::onsetStrength() const {
    return _detector.onsetStrength();
}

const char* AmpTransientDetector::lastOnsetRejectReasonName() const {
    return _detector.lastOnsetRejectReasonName();
}

bool AmpTransientDetector::transientDetected() const {
    return _detector.transientDetected();
}

float AmpTransientDetector::transientStrength() const {
    return _detector.transientStrength();
}

unsigned long AmpTransientDetector::transientDurationMs() const {
    return _detector.transientDurationMs();
}

bool AmpTransientDetector::peakActive() const {
    return _detector.peakActive();
}

float AmpTransientDetector::peakStrength() const {
    return _detector.peakStrength();
}

const char* AmpTransientDetector::lastTransientRejectReasonName() const {
    return _detector.lastTransientRejectReasonName();
}

unsigned long AmpTransientDetector::lastTransientRejectedDurationMs() const {
    return _detector.lastTransientRejectedDurationMs();
}

float AmpTransientDetector::lastTransientRejectedStrength() const {
    return _detector.lastTransientRejectedStrength();
}

unsigned long AmpTransientDetector::onsetRejectedCount() const {
    return _detector.onsetRejectedCount();
}

unsigned long AmpTransientDetector::transientRejectedCount() const {
    return _detector.transientRejectedCount();
}

unsigned long AmpTransientDetector::transientRejectedDurationTooShortCount() const {
    return _detector.transientRejectedDurationTooShortCount();
}

unsigned long AmpTransientDetector::transientRejectedDurationTooLongCount() const {
    return _detector.transientRejectedDurationTooLongCount();
}

unsigned long AmpTransientDetector::transientRejectedStrengthTooLowCount() const {
    return _detector.transientRejectedStrengthTooLowCount();
}

float AmpTransientDetector::onsetDetectionThreshold() const {
    return _detector.onsetDetectionThreshold();
}

float AmpTransientDetector::onsetReleaseThreshold() const {
    return _detector.onsetReleaseThreshold();
}

unsigned long AmpTransientDetector::cooldownAfterOnsetMs() const {
    return _detector.cooldownAfterOnsetMs();
}

unsigned long AmpTransientDetector::minTransientDurationMs() const {
    return _detector.minTransientDurationMs();
}

unsigned long AmpTransientDetector::maxTransientDurationMs() const {
    return _detector.maxTransientDurationMs();
}

float AmpTransientDetector::minTransientPeakStrength() const {
    return _detector.minTransientPeakStrength();
}

unsigned long AmpTransientDetector::releaseDebounceMs() const {
    return _detector.releaseDebounceMs();
}
