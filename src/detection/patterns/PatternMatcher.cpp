#include "PatternMatcher.h"

namespace detection {

void PatternMatcher::reset() {
    _assembler.reset();
}

void PatternMatcher::configure(const PatternRulesConfig& config) {
    _rules.configure(config);
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
        return false;
    }

    out = _rules.evaluate(candidate, nowMs);
    return true;
}

} // namespace detection
