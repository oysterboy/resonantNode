#include "FreqTransientDetector.h"

FreqTransientDetector::FreqTransientDetector() {
    _streamExtractor.setTargetFrequencyHz(3200UL);
    _streamExtractor.setSampleRateHz(16000UL);
    _streamExtractor.setWindowSizeSamples(64UL);
    resetState();
}

void FreqTransientDetector::begin() {
    resetState();
}

void FreqTransientDetector::resetState() {
    _streamExtractor.resetState();
}

void FreqTransientDetector::setTargetFrequencyHz(unsigned long value) {
    _streamExtractor.setTargetFrequencyHz(value);
}

void FreqTransientDetector::setSampleRateHz(unsigned long value) {
    _streamExtractor.setSampleRateHz(value);
}

void FreqTransientDetector::setWindowSizeSamples(unsigned long value) {
    _streamExtractor.setWindowSizeSamples(value);
}

void FreqTransientDetector::observeCenteredSample(int centeredSample) {
    _streamExtractor.observeCenteredSample(centeredSample);
}

float FreqTransientDetector::lastFrequencyScore() const {
    return _streamExtractor.lastFrequencyScore();
}

float FreqTransientDetector::lastTargetPower() const {
    return _streamExtractor.lastTargetPower();
}

float FreqTransientDetector::lastNeighborPower() const {
    return _streamExtractor.lastNeighborPower();
}

float FreqTransientDetector::lastTotalEnergy() const {
    return _streamExtractor.lastTotalEnergy();
}

float FreqTransientDetector::lastSpectralContrast() const {
    return _streamExtractor.lastSpectralContrast();
}

float FreqTransientDetector::frequencyBinSpacingHz() const {
    return _streamExtractor.frequencyBinSpacingHz();
}

unsigned long FreqTransientDetector::targetFrequencyHz() const {
    return _streamExtractor.targetFrequencyHz();
}

unsigned long FreqTransientDetector::sampleRateHz() const {
    return _streamExtractor.sampleRateHz();
}

unsigned long FreqTransientDetector::windowSizeSamples() const {
    return _streamExtractor.windowSizeSamples();
}

unsigned long FreqTransientDetector::sampleCount() const {
    return _streamExtractor.sampleCount();
}

bool FreqTransientDetector::windowReady() const {
    return _streamExtractor.windowReady();
}
