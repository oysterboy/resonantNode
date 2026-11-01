// The echo modes' inspection modules (ECHO-001 the echo room, ECHO-002 Whispernet): the attack, the level window, the
// modulation and the timbre, read from the FeatureHistory. Compiled into the builds that define ECHO_INSPECTION
// (esp32dev-echo, esp32dev-whisper, native-echoreplay, and the host suites that include this file): the Node, the
// Analyzer and the Emitter neither build nor link them (OccurrenceInspector::runInspectionModule's dispatch).
#ifdef ECHO_INSPECTION

#include <math.h>

#include "OccurrenceInspector.h"

namespace detection {

// ECHO-001: the rise into the onset, from the stream's history around the occurrence's start (32-bit ms, wrap-safe in
// FeatureHistory::riseTimeMs).
void OccurrenceInspector::annotateAttack(InspectedOccurrence& out, const Occurrence& occurrence,
                                         const FeatureHistory* featureHistory,
                                         const AttackInspectionConfig& config) const {
    OccurrenceAttack& attack = out.occurrence.attack;
    attack = OccurrenceAttack{};
    attack.applicable = true;
    if (featureHistory == nullptr) {
        return;  // Unknown
    }
    const unsigned long fromMs = occurrence.startMs - config.preMs;
    const unsigned long toMs = occurrence.startMs + config.postMs;
    const MagnitudeWindow window = featureHistory->getWindow(config.stream, fromMs, toMs, toMs);
    if (!window.hasValues || window.max <= config.floorLevel) {
        return;  // Unknown: nothing above the noise
    }
    // 10 % -> 90 % of the window's peak, the low point never below the noise floor.
    const float low =
        config.lowFraction * window.max > config.floorLevel ? config.lowFraction * window.max : config.floorLevel;
    unsigned long riseMs = 0;
    if (!featureHistory->riseTimeMs(config.stream, fromMs, toMs, low, config.highFraction * window.max, riseMs)) {
        return;  // Unknown: no low bin before the crossing (loud from the window's start)
    }
    attack.measured = true;
    attack.riseMs = static_cast<uint32_t>(riseMs);
    attack.attackClass = riseMs <= config.strongMaxMs ? StrengthClass::Strong
        : riseMs <= config.mediumMaxMs                ? StrengthClass::Medium
        : riseMs <= config.weakMaxMs                  ? StrengthClass::Weak
                                                      : StrengthClass::None;
}

namespace {

// The occurrence's span, its last maxWindowMs when longer (32-bit ms, wrap-safe in FeatureHistory).
void spanWindow(const Occurrence& occurrence, uint32_t maxWindowMs, unsigned long& fromMs, unsigned long& toMs) {
    toMs = occurrence.startMs + occurrence.durationMs;
    fromMs = occurrence.durationMs > maxWindowMs ? toMs - maxWindowMs : occurrence.startMs;
}

}  // namespace

// ECHO-002: the level window, P90 over the span.
void OccurrenceInspector::annotateLevel(InspectedOccurrence& out, const Occurrence& occurrence,
                                        const FeatureHistory* featureHistory,
                                        const LevelWindowInspectionConfig& config) const {
    OccurrenceLevel& level = out.occurrence.level;
    level = OccurrenceLevel{};
    level.applicable = true;
    if (featureHistory == nullptr) {
        return;
    }
    unsigned long fromMs = 0;
    unsigned long toMs = 0;
    spanWindow(occurrence, config.maxWindowMs, fromMs, toMs);
    const MagnitudeWindow window = featureHistory->getWindow(config.stream, fromMs, toMs, toMs);
    if (!window.hasValues) {
        return;
    }
    level.measured = true;
    level.p90 = window.p90;
    level.levelClass =
        window.p90 >= config.floorLevel && window.p90 <= config.loudBound ? StrengthClass::Strong : StrengthClass::None;
}

// ECHO-002: the timbre, the median flatness of the span's loud blocks.
void OccurrenceInspector::annotateFlatness(InspectedOccurrence& out, const Occurrence& occurrence,
                                           const FeatureHistory* featureHistory,
                                           const FlatnessInspectionConfig& config) const {
    OccurrenceTimbre& timbre = out.occurrence.timbre;
    timbre = OccurrenceTimbre{};
    timbre.applicable = true;
    if (featureHistory == nullptr) {
        return;
    }
    unsigned long fromMs = 0;
    unsigned long toMs = 0;
    spanWindow(occurrence, config.maxWindowMs, fromMs, toMs);
    const MagnitudeWindow window = featureHistory->getWindow(config.stream, fromMs, toMs, toMs);
    timbre.values = static_cast<uint32_t>(window.valueCount);
    if (!window.hasValues || window.valueCount < config.minValues) {
        return;  // Unknown: too few loud blocks to say
    }
    timbre.measured = true;
    timbre.flatness = window.median;
    timbre.timbreClass = window.median >= config.minFlatness && window.median <= config.maxFlatness
        ? StrengthClass::Strong
        : StrengthClass::None;
}

// ECHO-002: the modulation index, RMS of the bins around their moving mean against the window's mean.
void OccurrenceInspector::annotateModulation(InspectedOccurrence& out, const Occurrence& occurrence,
                                             const FeatureHistory* featureHistory,
                                             const ModulationInspectionConfig& config) const {
    OccurrenceModulation& mod = out.occurrence.modulation;
    mod = OccurrenceModulation{};
    mod.applicable = true;
    if (featureHistory == nullptr) {
        return;
    }
    unsigned long fromMs = 0;
    unsigned long toMs = 0;
    spanWindow(occurrence, config.maxWindowMs, fromMs, toMs);
    static constexpr size_t kMaxBins = 128;
    float v[kMaxBins];
    const unsigned long stepMs =
        config.stepMs > FeatureHistory::kBinDurationMs ? config.stepMs : FeatureHistory::kBinDurationMs;
    const size_t n = featureHistory->copyValues(config.stream, fromMs, toMs, stepMs, v, kMaxBins);
    const size_t half = static_cast<size_t>(config.slowMs / stepMs / 2U);
    if (n < 2 * half + 3) {
        return;  // Unknown: too short to see syllables
    }
    double mean = 0.0;
    for (size_t i = 0; i < n; ++i) {
        mean += v[i];
    }
    mean /= static_cast<double>(n);
    if (mean <= 0.0) {
        return;
    }
    double sumSq = 0.0;
    size_t used = 0;
    for (size_t i = half; i + half < n; ++i) {
        double slow = 0.0;
        for (size_t k = i - half; k <= i + half; ++k) {
            slow += v[k];
        }
        slow /= static_cast<double>(2 * half + 1);
        const double d = v[i] - slow;
        sumSq += d * d;
        ++used;
    }
    mod.measured = true;
    mod.index = static_cast<float>(sqrt(sumSq / static_cast<double>(used)) / mean);
    mod.modulationClass = mod.index >= config.strongIndex ? StrengthClass::Strong
        : mod.index >= config.mediumIndex                 ? StrengthClass::Medium
        : mod.index >= config.weakIndex                   ? StrengthClass::Weak
                                                          : StrengthClass::None;
}

}  // namespace detection

#endif  // ECHO_INSPECTION
