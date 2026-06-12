#include "PatternMatcher.h"

namespace detection {

void PatternMatcher::reset() {
    _assembler.reset();
    _report = {};
}

void PatternMatcher::configure(const PatternMatcherConfig& config) {
    _rules.configure(config);
}

const PatternMatcherReport& PatternMatcher::report() const {
    return _report;
}

PatternResult PatternMatcher::update(const InspectedOccurrence& occurrence, unsigned long nowMs) {
    acceptOccurrence(occurrence);
    PatternResult result = {};
    if (popPatternResult(nowMs, result)) {
        return result;
    }
    return {};
}

void PatternMatcher::acceptOccurrence(const InspectedOccurrence& occurrence) {
    _assembler.acceptOccurrence(occurrence);
}

bool PatternMatcher::popPatternResult(unsigned long nowMs, PatternResult& out) {
    PatternCandidate candidate = {};
    if (!_assembler.popPatternCandidate(candidate)) {
        _report.candidatePresent = false;
        return false;
    }

    out = _rules.evaluate(candidate, nowMs);
    _report.candidatePresent = true;
    _report.patternMatched = out.patternMatched;
    _report.supportMatched = out.supportMatched;
    _report.valid = out.valid;
    _report.patternType = out.type;
    _report.rejectReason = out.rejectReason;
    _report.startMs = static_cast<uint32_t>(out.primaryStartMs);
    _report.peakMs = static_cast<uint32_t>(out.primaryHeardAtMs);
    _report.endMs = static_cast<uint32_t>(out.primaryAcceptedMs);
    _report.durationMs = static_cast<uint32_t>(out.primaryDurationMs);
    _report.confidence = out.confidence;
    _report.strength = out.primaryStrength;
    _report.occurrenceCount = out.occurrenceCount;
    _report.acceptedOccurrenceCount = out.valid ? out.occurrenceCount : 0;
    return true;
}

} // namespace detection
