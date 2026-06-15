#include "AnalyzerApp.h"

#include <Arduino.h>
#include <esp_system.h>
#include <stdlib.h>
#include <string.h>

#include "../../RuntimeDefaults.h"
#include "../../AudioDebugConfig.h"
#include "../../TimingUtils.h"
#include "AnalyzerTextUtils.h"
#include "AnalyzerHealthHelpers.h"
#include "../../detection/patterns/PatternNames.h"
#include "../../detection/occurrences/Occurrence.h"
#include "AnalyzerClassifier.h"
#include "../../detection/inspector/OccurrenceInspector.h"

/*
AnalyzerApp

This file owns analyzer-mode orchestration, not the detector internals.

File structure:
- local utility helpers
- construction and setup
- runtime loop and diagnostic probe state
- console and emitter control
- raw-trigger helpers
- sequence orchestration
- canonical reporting and summary output
*/
constexpr int kMaxSamplesPerLoop = 512;
constexpr long kLateOnsetMinMs = 200L;
constexpr long kCleanDurationMinMs = 80L;
constexpr long kCleanDurationMaxMs = 180L;
constexpr long kSmearedDurationMinMs = 181L;
constexpr long kSmearedDurationMaxMs = 240L;
constexpr long kTooLongDurationMinMs = 241L;
constexpr long kNearMaxDurationMinMs = 220L;
constexpr long kAudioZeroishAbsThreshold = 8L;
constexpr long kAudioLargeJumpAbsThreshold = 12000L;
constexpr float kAudioRmsTooLowThreshold = 40.0f;
constexpr float kAudioRmsTooHighThreshold = 30000.0f;
constexpr unsigned long kAudioFlatlineStreakFrames = 32UL;
constexpr unsigned long kRawFlatlineMaxAbsThreshold = 8UL;
constexpr unsigned long kRawDcStuckRangeThreshold = 16UL;
constexpr unsigned long kRawDcStuckMeanAbsThreshold = 6UL;
constexpr unsigned long kRawClipThreshold = 32760UL;
constexpr unsigned long kRawZeroishAbsThreshold = 4UL;

RTC_DATA_ATTR static uint32_t g_analyzerBootCount = 0;


uint32_t sampleOffsetUs(uint32_t sampleOffset, uint32_t sampleRateHz) {
    if (sampleRateHz == 0) {
        return 0;
    }

    return static_cast<uint32_t>((static_cast<uint64_t>(sampleOffset) * 1000000ULL) / static_cast<uint64_t>(sampleRateHz));
}

void printHeapStatus(const char* when) {
    const uint32_t free8 = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_8BIT));
    const uint32_t largest8 = static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    const uint32_t min8 = static_cast<uint32_t>(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
    const uint32_t freeInternal = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    const uint32_t largestInternal = static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    const uint32_t minInternal = static_cast<uint32_t>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
    Serial.print("HEAP_STATUS when=");
    Serial.print(when != nullptr ? when : "unknown");
    Serial.print(" free_8bit=");
    Serial.print(free8);
    Serial.print(" largest_8bit=");
    Serial.print(largest8);
    Serial.print(" min_8bit=");
    Serial.print(min8);
    Serial.print(" free_internal=");
    Serial.println(freeInternal);
    Serial.print(" largest_internal=");
    Serial.print(largestInternal);
    Serial.print(" min_internal=");
    Serial.println(minInternal);
}

void printRuntimeSize() {
    Serial.println("MEMORY_INVENTORY");
    Serial.print("RUNTIME_SIZE detection_runtime_bytes=");
    Serial.println(static_cast<unsigned long>(sizeof(detection::DetectionRuntime)));
    Serial.print("SIZE DetectionRuntime=");
    Serial.println(static_cast<unsigned long>(sizeof(detection::DetectionRuntime)));
    Serial.print("  SIZE FeatureHistory=");
    Serial.println(static_cast<unsigned long>(sizeof(detection::FeatureHistory)));
    Serial.print("    SIZE FeatureBin=");
    Serial.println(static_cast<unsigned long>(detection::FeatureHistory::debugFeatureBinSize()));
    Serial.print("  SIZE Occurrence=");
    Serial.println(static_cast<unsigned long>(sizeof(detection::Occurrence)));
    Serial.print("  SIZE InspectedOccurrence=");
    Serial.println(static_cast<unsigned long>(sizeof(detection::InspectedOccurrence)));
    Serial.print("  SIZE PatternMatcher=");
    Serial.println(static_cast<unsigned long>(sizeof(detection::PatternMatcher)));
    Serial.print("  SIZE PatternResult=");
    Serial.println(static_cast<unsigned long>(sizeof(detection::PatternResult)));
    Serial.print("SIZE AudioSignal=");
    Serial.println(static_cast<unsigned long>(sizeof(AudioSignal)));
    Serial.print("  SIZE RawSampleHistory=");
    Serial.println(static_cast<unsigned long>(sizeof(RawSampleHistory)));
    Serial.print("SIZE AnalyzerApp::SequenceTest=");
    Serial.println(static_cast<unsigned long>(AnalyzerApp::debugSequenceTestSize()));
    Serial.print("  SIZE CurveSnapshot sampleHistory=");
    Serial.println(static_cast<unsigned long>(sizeof(CurveSnapshot) * AnalyzerApp::debugSequenceTestSampleHistoryCapacity()));
    Serial.print("  SIZE CurveSnapshot sampleHistoryPending=");
    Serial.println(static_cast<unsigned long>(sizeof(CurveSnapshot)));
    Serial.print("  SIZE CurveSnapshot sampleRows=");
    Serial.println(static_cast<unsigned long>(sizeof(CurveSnapshot) * AnalyzerApp::debugSequenceTestSampleRowsCapacity()));
    Serial.print("SIZE AnalyzerReport=");
    Serial.println(static_cast<unsigned long>(sizeof(AnalyzerReport)));
    const uint32_t free8 = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_8BIT));
    const uint32_t largest8 = static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    const uint32_t min8 = static_cast<uint32_t>(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
    const uint32_t freeInternal = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    const uint32_t largestInternal = static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    const uint32_t minInternal = static_cast<uint32_t>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
    const uint32_t freeDma = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_DMA));
    const uint32_t largestDma = static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
    const uint32_t minDma = static_cast<uint32_t>(heap_caps_get_minimum_free_size(MALLOC_CAP_DMA));
    Serial.print("HEAP_CAPS cap=8BIT free=");
    Serial.print(free8);
    Serial.print(" largest=");
    Serial.print(largest8);
    Serial.print(" min=");
    Serial.println(min8);
    Serial.print("HEAP_CAPS cap=INTERNAL free=");
    Serial.print(freeInternal);
    Serial.print(" largest=");
    Serial.print(largestInternal);
    Serial.print(" min=");
    Serial.println(minInternal);
    Serial.print("HEAP_CAPS cap=DMA free=");
    Serial.print(freeDma);
    Serial.print(" largest=");
    Serial.print(largestDma);
    Serial.print(" min=");
    Serial.println(minDma);
}

const char* audioHealthNameFromCounters(unsigned long zeroishFrames,
                                        unsigned long flatlineFrames,
                                        unsigned long largeJumpFrames,
                                        unsigned long rmsTooLowFrames,
                                        unsigned long rmsTooHighFrames) {
    if (rmsTooHighFrames > 0) {
        return "clipped";
    }
    if (largeJumpFrames > 0) {
        return "glitchy";
    }
    if (flatlineFrames > 0) {
        return "flatline";
    }
    if (zeroishFrames > 0 || rmsTooLowFrames > 0) {
        return "zeroish";
    }
    return "ok";
}

const char* systemResetReasonName(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON:
            return "POWERON_RESET";
        case ESP_RST_EXT:
            return "EXT_RESET";
        case ESP_RST_SW:
            return "SW_RESET";
        case ESP_RST_PANIC:
            return "PANIC_RESET";
        case ESP_RST_INT_WDT:
            return "INT_WDT_RESET";
        case ESP_RST_TASK_WDT:
            return "TASK_WDT_RESET";
        case ESP_RST_WDT:
            return "WDT_RESET";
        case ESP_RST_DEEPSLEEP:
            return "DEEPSLEEP_RESET";
        case ESP_RST_BROWNOUT:
            return "BROWNOUT_RESET";
        case ESP_RST_SDIO:
            return "SDIO_RESET";
#if defined(ESP_RST_USB)
        case ESP_RST_USB:
            return "USB_RESET";
#endif
#if defined(ESP_RST_JTAG)
        case ESP_RST_JTAG:
            return "JTAG_RESET";
#endif
#if defined(ESP_RST_EFUSE)
        case ESP_RST_EFUSE:
            return "EFUSE_RESET";
#endif
#if defined(ESP_RST_PWR_GLITCH)
        case ESP_RST_PWR_GLITCH:
            return "PWR_GLITCH_RESET";
#endif
        default:
            return "UNKNOWN_RESET";
    }
}

uint32_t AnalyzerApp::analyzerBootCount() const {
    return g_analyzerBootCount;
}

const char* AnalyzerApp::currentResetReasonName() const {
    return systemResetReasonName(esp_reset_reason());
}

void AnalyzerApp::updateSequenceAudioHealth(const AudioSamplePacket& audioSamplePacket) {
    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0 || _sequenceTest.currentTrialFinalized) {
        return;
    }

    auto& diagnostics = _sequenceTest.currentTrialDiagnostics;
    const long centeredSample = static_cast<long>(audioSamplePacket.centeredAudioValue);
    const long rawSample = static_cast<long>(audioSamplePacket.rawAudioValue);
    const unsigned long absCentered = static_cast<unsigned long>(centeredSample >= 0 ? centeredSample : -centeredSample);
    const unsigned long absRaw = static_cast<unsigned long>(rawSample >= 0 ? rawSample : -rawSample);
    const unsigned long delta = diagnostics.audioHasLastCenteredSample
        ? static_cast<unsigned long>(labs(centeredSample - diagnostics.audioLastCenteredSample))
        : 0UL;

    ++diagnostics.audioFrames;
    diagnostics.audioSumSquares += static_cast<uint64_t>(absCentered) * static_cast<uint64_t>(absCentered);
    if (absCentered <= static_cast<unsigned long>(kAudioZeroishAbsThreshold)) {
        ++diagnostics.audioZeroishFrames;
    }
    if (diagnostics.audioHasLastCenteredSample && centeredSample == diagnostics.audioLastCenteredSample) {
        ++diagnostics.audioFlatlineRunFrames;
        if (diagnostics.audioFlatlineRunFrames >= kAudioFlatlineStreakFrames) {
            ++diagnostics.audioFlatlineFrames;
        }
    } else {
        diagnostics.audioFlatlineRunFrames = 0;
    }
    if (delta > diagnostics.audioMaxAbsDelta) {
        diagnostics.audioMaxAbsDelta = delta;
    }
    if (diagnostics.audioHasLastCenteredSample && delta >= static_cast<unsigned long>(kAudioLargeJumpAbsThreshold)) {
        ++diagnostics.audioLargeJumpFrames;
    }

    ++diagnostics.rawFrames;
    diagnostics.rawSum += static_cast<int32_t>(audioSamplePacket.rawAudioValue);
    diagnostics.rawAbsSum += static_cast<uint32_t>(absRaw);
    if (diagnostics.rawFrames == 1) {
        diagnostics.rawMin = audioSamplePacket.rawAudioValue;
        diagnostics.rawMax = audioSamplePacket.rawAudioValue;
        diagnostics.rawSameValueRun = 1;
    } else {
        if ((diagnostics.rawLastSample < 0 && rawSample > 0) || (diagnostics.rawLastSample > 0 && rawSample < 0)) {
            if (diagnostics.rawZeroCrossings < 255U) {
                ++diagnostics.rawZeroCrossings;
            }
        }
        if (rawSample == diagnostics.rawLastSample) {
            if (diagnostics.rawSameValueCount < 255U) {
                ++diagnostics.rawSameValueCount;
            }
            if (diagnostics.rawSameValueRun < 255U) {
                ++diagnostics.rawSameValueRun;
            }
        } else {
            diagnostics.rawSameValueRun = 1;
        }
        if (diagnostics.rawSameValueRun > diagnostics.rawSameValueMaxRun) {
            diagnostics.rawSameValueMaxRun = diagnostics.rawSameValueRun;
        }
        if (audioSamplePacket.rawAudioValue < diagnostics.rawMin) {
            diagnostics.rawMin = audioSamplePacket.rawAudioValue;
        }
        if (audioSamplePacket.rawAudioValue > diagnostics.rawMax) {
            diagnostics.rawMax = audioSamplePacket.rawAudioValue;
        }
    }
    if (diagnostics.rawFrames == 1 && diagnostics.rawSameValueMaxRun < 1UL) {
        diagnostics.rawSameValueMaxRun = 1;
    }
    diagnostics.rawLastSample = rawSample;
    const float rms = diagnostics.audioFrames > 0
        ? static_cast<float>(sqrt(static_cast<double>(diagnostics.audioSumSquares) / static_cast<double>(diagnostics.audioFrames)))
        : 0.0f;
    diagnostics.audioRms = rms;
    if (rms < kAudioRmsTooLowThreshold) {
        ++diagnostics.audioRmsTooLowFrames;
    }
    if (rms > kAudioRmsTooHighThreshold || absRaw > 32760UL) {
        ++diagnostics.audioRmsTooHighFrames;
    }

    diagnostics.audioHealth = audioHealthNameFromCounters(
        diagnostics.audioZeroishFrames,
        diagnostics.audioFlatlineFrames,
        diagnostics.audioLargeJumpFrames,
        diagnostics.audioRmsTooLowFrames,
        diagnostics.audioRmsTooHighFrames
    );
    diagnostics.audioLastCenteredSample = centeredSample;
    diagnostics.audioHasLastCenteredSample = true;
}

unsigned long AnalyzerApp::activeRunStartMs() const {
    if (_sequenceTest.active) {
        return _sequenceTest.startedAtMs;
    }
    return 0UL;
}

unsigned long AnalyzerApp::activeRunEndMs() const {
    if (_sequenceTest.active && _sequenceTest.currentTrialEndMs > 0) {
        return _sequenceTest.currentTrialEndMs;
    }
    return millis();
}

void AnalyzerApp::resetLoopHealthWindow() {
    _loopHealth.reset();
    _loopLastUs = micros();
}

bool AnalyzerApp::shouldPrintHardwareDiagnostics() const {
    return _sequenceTest.outputConfig.diagnosticsEnabled &&
           _sequenceTest.outputConfig.verbosity >= 2U &&
           _sequenceTest.outputConfig.mode == SeqOutputMode::System;
}

bool AnalyzerApp::shouldPrintSequenceTrial() const {
    return _sequenceTest.outputConfig.mode != SeqOutputMode::Quiet;
}

bool AnalyzerApp::shouldPrintSequenceSource(const AnalyzerReport& report) const {
    if (!_sequenceTest.outputConfig.diagnosticsEnabled) {
        return false;
    }

    switch (_sequenceTest.outputConfig.mode) {
        case SeqOutputMode::Source:
            return sequenceOutputWhenEnabled(_sequenceTest.outputConfig.when, report.classification.result);
        default:
            return false;
    }
}

bool AnalyzerApp::shouldPrintSequenceInspect(const AnalyzerReport& report) const {
    if (!_sequenceTest.outputConfig.diagnosticsEnabled) {
        return false;
    }

    switch (_sequenceTest.outputConfig.mode) {
        case SeqOutputMode::Inspect:
        case SeqOutputMode::Explain:
            return sequenceOutputWhenEnabled(_sequenceTest.outputConfig.when, report.classification.result);
        default:
            return false;
    }
}

bool AnalyzerApp::shouldPrintSequenceSystem(const AnalyzerReport& report) const {
    if (!_sequenceTest.outputConfig.diagnosticsEnabled) {
        return false;
    }

    switch (_sequenceTest.outputConfig.mode) {
        case SeqOutputMode::System:
            return sequenceOutputWhenEnabled(_sequenceTest.outputConfig.when, report.classification.result);
        default:
            return false;
    }
}

bool AnalyzerApp::shouldPrintSequenceExplain(const AnalyzerReport& report) const {
    return _sequenceTest.outputConfig.diagnosticsEnabled &&
           _sequenceTest.outputConfig.mode == SeqOutputMode::Explain &&
           sequenceOutputWhenEnabled(_sequenceTest.outputConfig.when, report.classification.result);
}

void buildFrequencyFailReason(const detection::FrequencyBandMeasurementPacket& evidence,
                              const FrequencyMatchEvaluation::Values& tuning,
                              char* out,
                              size_t outSize) {
    FrequencyMatchEvaluation::buildFailReason(evidence, tuning, out, outSize);
}

const char* occurrenceTypeName(detection::OccurrenceType type) {
    switch (type) {
        case detection::OccurrenceType::Scalar:
            return "scalar";
        case detection::OccurrenceType::Frequency:
            return "frequency";
        case detection::OccurrenceType::None:
        default:
            return "none";
    }
}

const char* occurrenceSourceName(detection::DetectorId detectorId) {
    switch (detectorId) {
        case detection::DetectorId::ScalarTransient:
            return "scalar";
        case detection::DetectorId::FrequencyMatch:
            return "frequency";
        case detection::DetectorId::Unknown:
        default:
            return "none";
    }
}

const char* occurrenceDetectorKindName(detection::DetectorId detectorId) {
    switch (detectorId) {
        case detection::DetectorId::ScalarTransient:
            return "scalar_transient";
        case detection::DetectorId::FrequencyMatch:
            return "frequency_match";
        case detection::DetectorId::Unknown:
        default:
            return "unknown";
    }
}

const char* occurrenceRejectReasonName(detection::OccurrenceRejectReason reason) {
    switch (reason) {
        case detection::OccurrenceRejectReason::None:
            return "none";
        case detection::OccurrenceRejectReason::TooShort:
            return "too_short";
        case detection::OccurrenceRejectReason::TooLong:
            return "too_long";
        case detection::OccurrenceRejectReason::TooWeak:
            return "too_weak";
        case detection::OccurrenceRejectReason::BelowThreshold:
            return "below_threshold";
        case detection::OccurrenceRejectReason::Cooldown:
            return "cooldown";
        case detection::OccurrenceRejectReason::MissingFrequencyEvidence:
            return "missing_frequency_evidence";
        case detection::OccurrenceRejectReason::MissingAmpSupport:
            return "missing_amp_support";
        case detection::OccurrenceRejectReason::InvalidTiming:
            return "invalid_timing";
        case detection::OccurrenceRejectReason::UnsupportedKind:
            return "unsupported_kind";
        case detection::OccurrenceRejectReason::Unknown:
        default:
            return "unknown";
    }
}

const char* strengthClassName(detection::StrengthClass value) {
    switch (value) {
        case detection::StrengthClass::None:
            return "none";
        case detection::StrengthClass::Weak:
            return "weak";
        case detection::StrengthClass::Medium:
            return "medium";
        case detection::StrengthClass::Strong:
            return "strong";
        case detection::StrengthClass::Unknown:
        default:
            return "unknown";
    }
}

const char* evidenceTargetName(detection::EvidenceTarget value) {
    switch (value) {
        case detection::EvidenceTarget::SupportStrength:
            return "SupportStrength";
        case detection::EvidenceTarget::FrequencyScoreStrength:
            return "FrequencyScoreStrength";
        case detection::EvidenceTarget::FrequencyContrastQuality:
            return "FrequencyContrastQuality";
        case detection::EvidenceTarget::TargetBandStrength:
            return "TargetBandStrength";
        case detection::EvidenceTarget::None:
        default:
            return "None";
    }
}

const char* inspectionPlanName(const detection::InspectionPlan& plan) {
    if (plan.count == 1 &&
        plan.modules[0].kind == detection::InspectionModuleKind::ScalarFeatureStrength) {
        switch (plan.modules[0].target) {
            case detection::EvidenceTarget::FrequencyScoreStrength:
                return "frequency_score";
            case detection::EvidenceTarget::TargetBandStrength:
                return "target_band";
            case detection::EvidenceTarget::SupportStrength:
            default:
                return "support_strength";
        }
    }
    if (plan.count == 2 &&
        plan.modules[0].kind == detection::InspectionModuleKind::ScalarFeatureStrength &&
        plan.modules[0].target == detection::EvidenceTarget::FrequencyScoreStrength &&
        plan.modules[1].kind == detection::InspectionModuleKind::ScalarFeatureStrength &&
        plan.modules[1].target == detection::EvidenceTarget::FrequencyContrastQuality) {
        return "frequency_score_contrast";
    }

    return "custom";
}

const char* inspectionModulesName(const detection::InspectionPlan& plan) {
    if (plan.count == 1 &&
        plan.modules[0].kind == detection::InspectionModuleKind::ScalarFeatureStrength) {
        return "ScalarFeatureStrength";
    }

    return "custom";
}

const char* inspectionEvidenceTargetsName(const detection::InspectionPlan& plan) {
    if (plan.count > 0 && plan.modules[0].kind == detection::InspectionModuleKind::ScalarFeatureStrength) {
        return evidenceTargetName(plan.modules[0].target);
    }

    return "none";
}

const char* seqOutputModeName(AnalyzerApp::SeqOutputMode mode) {
    switch (mode) {
        case AnalyzerApp::SeqOutputMode::Quiet:
            return "quiet";
        case AnalyzerApp::SeqOutputMode::Trial:
            return "trial";
        case AnalyzerApp::SeqOutputMode::System:
            return "system";
        case AnalyzerApp::SeqOutputMode::Source:
            return "source";
        case AnalyzerApp::SeqOutputMode::Inspect:
            return "inspect";
        case AnalyzerApp::SeqOutputMode::Explain:
            return "explain";
        default:
            return "trial";
    }
}

const char* seqOutputWhenName(AnalyzerApp::SeqOutputWhen value) {
    switch (value) {
        case AnalyzerApp::SeqOutputWhen::Off:
            return "off";
        case AnalyzerApp::SeqOutputWhen::All:
            return "all";
        case AnalyzerApp::SeqOutputWhen::Miss:
        default:
            return "miss";
    }
}

bool seqOutputWhenEnabled(AnalyzerApp::SeqOutputWhen configured, AnalyzerResult result) {
    switch (configured) {
        case AnalyzerApp::SeqOutputWhen::All:
            return true;
        case AnalyzerApp::SeqOutputWhen::Off:
            return false;
        case AnalyzerApp::SeqOutputWhen::Miss:
        default:
            switch (result) {
                case AnalyzerResult::Miss:
                case AnalyzerResult::Late:
                case AnalyzerResult::Duplicate:
                case AnalyzerResult::Unexpected:
                case AnalyzerResult::Rejected:
                case AnalyzerResult::Ambiguous:
                case AnalyzerResult::TooDense:
                    return true;
                case AnalyzerResult::Expected:
                case AnalyzerResult::Early:
                case AnalyzerResult::Unknown:
                default:
                    return false;
            }
    }
}

AnalyzerApp::SeqOutputMode seqOutputModeFromToken(const char* token, bool* valid) {
    if (valid != nullptr) {
        *valid = true;
    }
    if (token == nullptr || *token == '\0') {
        if (valid != nullptr) {
            *valid = false;
        }
        return AnalyzerApp::SeqOutputMode::Trial;
    }
    if (equalsIgnoreCase(token, "trial")) {
        return AnalyzerApp::SeqOutputMode::Trial;
    }
    if (equalsIgnoreCase(token, "system")) {
        return AnalyzerApp::SeqOutputMode::System;
    }
    if (equalsIgnoreCase(token, "source")) {
        return AnalyzerApp::SeqOutputMode::Source;
    }
    if (equalsIgnoreCase(token, "inspect")) {
        return AnalyzerApp::SeqOutputMode::Inspect;
    }
    if (equalsIgnoreCase(token, "explain")) {
        return AnalyzerApp::SeqOutputMode::Explain;
    }
    if (equalsIgnoreCase(token, "quiet")) {
        return AnalyzerApp::SeqOutputMode::Quiet;
    }
    if (valid != nullptr) {
        *valid = false;
    }
    return AnalyzerApp::SeqOutputMode::Trial;
}

AnalyzerApp::SeqOutputWhen seqOutputWhenFromToken(const char* token, bool* valid) {
    if (valid != nullptr) {
        *valid = true;
    }
    if (token == nullptr || *token == '\0') {
        if (valid != nullptr) {
            *valid = false;
        }
        return AnalyzerApp::SeqOutputWhen::Miss;
    }
    if (equalsIgnoreCase(token, "off")) {
        return AnalyzerApp::SeqOutputWhen::Off;
    }
    if (equalsIgnoreCase(token, "miss")) {
        return AnalyzerApp::SeqOutputWhen::Miss;
    }
    if (equalsIgnoreCase(token, "all")) {
        return AnalyzerApp::SeqOutputWhen::All;
    }
    if (valid != nullptr) {
        *valid = false;
    }
    return AnalyzerApp::SeqOutputWhen::Miss;
}

// -----------------------------------------------------------------------------
// Construction and setup
// -----------------------------------------------------------------------------

AnalyzerApp::AnalyzerApp(int inputPin)
    : _inputPin(inputPin),
      _i2sSource(14, 27, 33, 16000, 32),
      _audioSource(_i2sSource),
      _audioSignal(_audioSource),
      _freqBandStream() {
    //PARAM TUNING TEMPORARY
    const detection::DetectionProfile& freqProfile = detection::detectionProfileForKind(detection::DetectionProfileKind::TonalPulseFreq);
    _analyzerTuning.frequencyMatch.attackScoreMin = freqProfile.frequencyMatch.attackScoreMin;
    _analyzerTuning.frequencyMatch.releaseScoreMin = freqProfile.frequencyMatch.releaseScoreMin;
    _analyzerTuning.frequencyMatch.attackContrastMin = freqProfile.frequencyMatch.attackContrastMin;
    _analyzerTuning.frequencyMatch.releaseContrastMin = freqProfile.frequencyMatch.releaseContrastMin;
    _analyzerTuning.scalarTransient = detection::detectionProfileForKind(detection::DetectionProfileKind::TonalPulseScalar).scalarTransient;
}

detection::DetectionProfile AnalyzerApp::effectiveSequenceProfile() const {
    //PARAM TUNING TEMPORARY
    detection::DetectionProfile profile = detection::detectionProfileForKind(_sequenceTest.profileKind);
    profile.frequencyMatch.attackScoreMin = _analyzerTuning.frequencyMatch.attackScoreMin;
    profile.frequencyMatch.releaseScoreMin = _analyzerTuning.frequencyMatch.releaseScoreMin;
    profile.frequencyMatch.attackContrastMin = _analyzerTuning.frequencyMatch.attackContrastMin;
    profile.frequencyMatch.releaseContrastMin = _analyzerTuning.frequencyMatch.releaseContrastMin;
    if (_sequenceTest.profileKind == detection::DetectionProfileKind::TonalPulseScalar) {
        profile.scalarTransient = _analyzerTuning.scalarTransient;
    }
    return profile;
}

void AnalyzerApp::begin() {
    beginEmitterControl();
    ++g_analyzerBootCount;

    configureParameters();
    _audioSource.begin();
    _audioSignal.begin();
    _audioSignal.setCurveSampleCallback(&AnalyzerApp::sequenceCurveSampleCallback, this);
    _freqBandStream.resetState();
    _freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
    _freqBandStream.setTargetFrequencyHz(runtime::kDefaultChirpFrequencyHz);
    printRuntimeSize();
    printHeapStatus("begin_before_runtime_alloc");
    _detection.resetState();
    _loopLastUs = micros();
    _loopMaxSinceBootUs = 0;
    _loopHealth.reset();
    _usbLineLength = 0;
    _usbLineBuffer[0] = '\0';
    _emitterLineLength = 0;
    _emitterLineBuffer[0] = '\0';
    _controlClaimPending = false;
    _controlClaimSent = false;
    _controlClaimAtMs = 0;

    Serial.println("EVT analyzer_ready");
    Serial.println("EVT analyzer_help type='HELP', 'PARAM freqScore=18000 freqContrast=50.0 freqReleaseScore=12000 freqReleaseContrast=50.0', 'PARAM STATUS', 'RAW trigger f=3200 dur=100 post=1000 dump=bin', 'SEQ MODE quiet|trial|inspect|source|system|explain WHEN off|miss|all VERBOSE 0|1|2 STATUS REPORT', 'DET PROFILE TonalPulseFreq|TonalPulseScalar|AmpExperimental'");
}

void AnalyzerApp::configureParameters() {
    configureI2SParameters();
}

void AnalyzerApp::configureI2SParameters() {
    _audioSignal.setSmoothingFactor(0.5f);
    _audioSignal.setBaselineUpdateFactor(0.005f);
    _audioSignal.setBaselineTrackingQuietThreshold(20);
}

const char* AnalyzerApp::sequenceOutputModeName(SeqOutputMode mode) {
    return seqOutputModeName(mode);
}

const char* AnalyzerApp::sequenceOutputWhenName(SeqOutputWhen value) {
    return seqOutputWhenName(value);
}

bool AnalyzerApp::sequenceOutputModeEnabled(SeqOutputMode configured, SeqOutputMode requested) {
    if (configured == SeqOutputMode::Quiet) {
        return false;
    }
    if (requested == SeqOutputMode::Trial) {
        return true;
    }
    if (requested == SeqOutputMode::System) {
        return configured == SeqOutputMode::System;
    }
    if (requested == SeqOutputMode::Explain) {
        return configured == SeqOutputMode::Explain;
    }
    if (requested == SeqOutputMode::Source) {
        return configured == requested;
    }
    if (requested == SeqOutputMode::Inspect) {
        return configured == requested || configured == SeqOutputMode::Explain;
    }
    return configured == requested;
}

bool AnalyzerApp::sequenceOutputWhenEnabled(SeqOutputWhen configured, AnalyzerResult result) {
    return seqOutputWhenEnabled(configured, result);
}

AnalyzerApp::SeqOutputMode AnalyzerApp::sequenceOutputModeFromToken(const char* token, bool* valid) {
    return seqOutputModeFromToken(token, valid);
}

AnalyzerApp::SeqOutputWhen AnalyzerApp::sequenceOutputWhenFromToken(const char* token, bool* valid) {
    return seqOutputWhenFromToken(token, valid);
}

AnalyzerApp::SequenceDiagMode AnalyzerApp::sequenceDiagModeFromOutputWhen(SeqOutputWhen when) {
    switch (when) {
        case SeqOutputWhen::Off:
            return SequenceDiagMode::Off;
        case SeqOutputWhen::All:
            return SequenceDiagMode::Trial;
        case SeqOutputWhen::Miss:
        default:
            return SequenceDiagMode::Miss;
    }
}

// -----------------------------------------------------------------------------
// Runtime loop and diagnostic probe state
// -----------------------------------------------------------------------------

void AnalyzerApp::update() {
    const uint32_t nowUs = micros();
    if (_loopLastUs != 0) {
        const uint32_t loopUs = nowUs - _loopLastUs;
        _loopHealth.record(loopUs);
        if (loopUs > _loopMaxSinceBootUs) {
            _loopMaxSinceBootUs = loopUs;
        }
    }
    _loopLastUs = nowUs;

    const unsigned long updateLoopStartUs = nowUs;
    const unsigned long now = millis();

    processPendingSequenceStart();

    int processedSamples = 0;
    AudioBlock block;
    const uint32_t sampleWorkStartUs = micros();
    while (processedSamples < kMaxSamplesPerLoop) {
        const int availableBytesBeforeRead = _audioSource.availableBytes();
        if (!_i2sSource.readBlock(block)) {
            break;
        }
        if (block.sampleCount == 0 || block.samples == nullptr) {
            break;
        }

        if (_sequenceTest.active) {
            if (availableBytesBeforeRead >= 0) {
                const unsigned long availableBytes = static_cast<unsigned long>(availableBytesBeforeRead);
                _sequenceTest.availableBytesSum += static_cast<uint64_t>(availableBytes);
                _sequenceTest.availableBytesSamples++;
                if (availableBytes > _sequenceTest.maxAvailableBytes) {
                    _sequenceTest.maxAvailableBytes = availableBytes;
                }
            }
            const unsigned long blockAgeMs = micros() > block.approxStartMicros
                ? static_cast<unsigned long>((micros() - block.approxStartMicros) / 1000UL)
                : 0UL;
            if (blockAgeMs > _sequenceTest.maxBlockAgeMs) {
                _sequenceTest.maxBlockAgeMs = blockAgeMs;
            }
        }

        const uint32_t sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
        if (_sequenceTest.active && _sequenceTest.currentTrial > 0) {
            const uint8_t blockHash = static_cast<uint8_t>(rawBlockFingerprint(block.samples, block.sampleCount));
            auto& diagnostics = _sequenceTest.currentTrialDiagnostics;
            if (diagnostics.rawFrames > 0 && blockHash == diagnostics.rawLastBlockHash) {
                ++diagnostics.rawBlockHashRepeatCount;
            }
            diagnostics.rawLastBlockHash = blockHash;
        }
        for (uint16_t i = 0; i < block.sampleCount; ++i) {
            const uint32_t sampleTimeUs = block.approxStartMicros + sampleOffsetUs(static_cast<uint32_t>(i), sampleRateHz);
            AudioSamplePacket audioSamplePacket;
            _audioSignal.update(static_cast<int>(block.samples[i]), sampleTimeUs, audioSamplePacket);
            updateSequenceAudioHealth(audioSamplePacket);
            if (_sequenceTest.outputConfig.frequencyBandEnabled) {
                _freqBandStream.observeCenteredSample(audioSamplePacket.centeredAudioValue, audioSamplePacket.timeMs);
            }
            if (_sequenceTest.active && _sequenceTest.currentTrial > 0) {
                const unsigned long processingLagMs = millis() > audioSamplePacket.timeMs
                    ? millis() - audioSamplePacket.timeMs
                    : 0UL;
                if (processingLagMs > _sequenceTest.maxProcessingLagMs) {
                    _sequenceTest.maxProcessingLagMs = processingLagMs;
                }
                if (audioSamplePacket.timeMs >= _sequenceTest.currentTrialStartMs &&
                    audioSamplePacket.timeMs <= _sequenceTest.currentTrialEndMs) {
                    ++_sequenceTest.currentTrialSamplesProcessed;
                }
                detection::FrequencyBandMeasurementPacket runtimeFrequencyMeasurementPacket = {};
                if (_sequenceTest.outputConfig.frequencyBandEnabled) {
                    runtimeFrequencyMeasurementPacket = captureFrequencyMeasurementPacket(audioSamplePacket);
                } else {
                    runtimeFrequencyMeasurementPacket.observedAtMs = audioSamplePacket.timeMs;
                }
                _detection.observeFrame(audioSamplePacket, runtimeFrequencyMeasurementPacket, audioSamplePacket.timeMs);
                detection::PatternResult runtimePatternResult = {};
                while (_detection.popPatternResult(runtimePatternResult)) {
                    const detection::DetectionPipelineResult& runtimePipelineResult = _detection.latestPipelineResult();
                    const detection::InspectedOccurrence* selectedInspectedOccurrence = nullptr;
                    if (runtimePipelineResult.hasPatternInspectedOccurrence
                        && runtimePipelineResult.patternInspectedOccurrence.occurrence.present) {
                        selectedInspectedOccurrence = &runtimePipelineResult.patternInspectedOccurrence;
                    }
                    _sequenceTest.currentTrialDiagnostics.runtimePatternCaptured = true;
                    handleSequencePending(
                        runtimePatternResult,
                        selectedInspectedOccurrence,
                        &runtimeFrequencyMeasurementPacket
                    );
                }
                updateSequenceAmbientStats(audioSamplePacket.timeMs);
            }
        }
        processedSamples += static_cast<int>(block.sampleCount);
        if (processedSamples > kMaxSamplesPerLoop) {
            processedSamples = kMaxSamplesPerLoop;
        }
    }
    const unsigned long sampleWorkUs = static_cast<unsigned long>(micros() - sampleWorkStartUs);
    if (_sequenceTest.active && sampleWorkUs > _sequenceTest.maxSampleWorkUs) {
        _sequenceTest.maxSampleWorkUs = sampleWorkUs;
    }

    _sequenceTest.samplesProcessed += static_cast<unsigned long>(processedSamples);
    if (static_cast<unsigned long>(processedSamples) > _sequenceTest.maxSamplesPerLoop) {
        _sequenceTest.maxSamplesPerLoop = static_cast<unsigned long>(processedSamples);
    }
    const unsigned long updateLoopUs = micros() - updateLoopStartUs;
    if (_sequenceTest.active) {
        _sequenceTest.totalUpdateLoopUs += updateLoopUs;
        ++_sequenceTest.updateLoopCount;
        if (updateLoopUs > _sequenceTest.currentTrialUpdateLoopMaxUs) {
            _sequenceTest.currentTrialUpdateLoopMaxUs = updateLoopUs;
        }
    }
    if (_sequenceTest.active && updateLoopUs > _sequenceTest.maxUpdateLoopUs) {
        _sequenceTest.maxUpdateLoopUs = updateLoopUs;
    }

    // Drain emitter markers before SEQ finalization so trial latches reflect the latest observed state.
    pollEmitterSerial();
    if (_controlClaimPending && !_controlClaimSent && timing::atOrAfter(now, _controlClaimAtMs)) {
        sendEmitterCommand("MODE REMOTE");
        _controlClaimSent = true;
        _controlClaimPending = false;
    }
    updateSequenceTest(now);
    pollUsbConsole();
    pollEmitterSerial();

#if TEST_LOG_STRESS
    Serial.println("LOG_STRESS");
#endif
}

unsigned long AnalyzerApp::loopDelayMs() const {
    return TEST_LOOP_DELAY_MS;
}

void AnalyzerApp::resetAudioSignalState() {
    _audioSignal.resetSignalState();
}

// -----------------------------------------------------------------------------
// Sequence, capture, and base sessions
// -----------------------------------------------------------------------------

void AnalyzerApp::processPendingSequenceStart() {
    if (!_pendingSequenceStart.active) {
        return;
    }

    _pendingSequenceStart.active = false;
    startSequenceTest(_pendingSequenceStart);
}

const char* AnalyzerApp::activeAnalyzerProfileName() const {
    return detection::detectionProfileName(_sequenceTest.profileKind);
}

const char* analyzerProfileDetailNamespace(detection::DetectionProfileKind profileKind) {
    switch (profileKind) {
        case detection::DetectionProfileKind::AmpExperimental:
            return "amp_experimental";
        case detection::DetectionProfileKind::TonalPulseScalar:
            return "tonal_pulse_scalar";
        case detection::DetectionProfileKind::TonalPulseFreq:
        default:
            return "tonal_pulse_freq";
    }
}

const char* analyzerProfileDetailSummary(detection::DetectionProfileKind profileKind) {
    switch (profileKind) {
        case detection::DetectionProfileKind::AmpExperimental:
            return "amp experimental profile view";
        case detection::DetectionProfileKind::TonalPulseScalar:
            return "tonal pulse scalar profile view";
        case detection::DetectionProfileKind::TonalPulseFreq:
        default:
            return "tonal pulse freq profile view";
    }
}

AnalyzerReport* AnalyzerApp::sequenceReportScratch() {
    _sequenceReportScratch = makeEmptyAnalyzerReport();
    return &_sequenceReportScratch;
}

void AnalyzerApp::buildSequenceAnalyzerReport(AnalyzerReport& report,
                                              unsigned long trialNumber,
                                              AnalyzerResult result,
                                              long dtMs,
                                              long durMs,
                                              float strength,
                                              bool bufferOverrun,
                                              unsigned long duplicateCount,
                                              const SequenceTest::TrialDiagnostics& diagnostics) const {
    report = makeEmptyAnalyzerReport();

    report.context.profile = activeAnalyzerProfileName();
    report.context.mode = _sequenceTest.externalEmitter ? "OBS" : "SEQ";
    report.context.trial = trialNumber;
    report.context.trigger = _sequenceTest.externalEmitter ? "observe" : "chirp";
    report.context.target = "tone";
    report.context.timestampMs = _sequenceTest.currentTrialEndMs;
    report.context.build = "pass-c";

    report.expected.triggerMs = _sequenceTest.currentTrialStartMs;
    report.expected.windowStartMs = _sequenceTest.currentTrialStartMs;
    report.expected.windowEndMs = _sequenceTest.currentTrialEndMs;
    report.expected.patternType = "sequence_trial";
    report.expected.expectedSource = _sequenceTest.externalEmitter ? "external" : "local";

    const long trialOnsetAnchorMs = static_cast<long>(sequenceTrialOnsetAnchorMs());
    const SequenceTrialSelection selectedTrial = selectSequenceTrialSelection(trialOnsetAnchorMs);
    detection::PatternResult synthesizedAcceptedPattern = {};
    const detection::PatternResult* reportPatternResult = selectedTrial.patternResult;
    const detection::InspectedOccurrence* reportInspectedOccurrence = selectedTrial.inspectedOccurrence;
    if (selectedTrial.kind == SequenceTrialSelection::Kind::AcceptedOccurrence
        && reportInspectedOccurrence != nullptr
        && reportInspectedOccurrence->occurrence.present) {
        synthesizedAcceptedPattern.type = detection::PatternType::SinglePulse;
        synthesizedAcceptedPattern.reasonCode = detection::PatternReasonCode::FromOccurrence;
        synthesizedAcceptedPattern.rejectReason = detection::PatternRejectReason::None;
        synthesizedAcceptedPattern.confidence = reportInspectedOccurrence->occurrence.confidence;
        synthesizedAcceptedPattern.occurrenceCount = 1;
        synthesizedAcceptedPattern.primaryStartMs = reportInspectedOccurrence->occurrence.startMs;
        synthesizedAcceptedPattern.primaryPeakMs = reportInspectedOccurrence->occurrence.peakMs;
        synthesizedAcceptedPattern.primaryHeardAtMs = reportInspectedOccurrence->occurrence.startMs;
        synthesizedAcceptedPattern.primaryAcceptedMs = reportInspectedOccurrence->occurrence.startMs;
        synthesizedAcceptedPattern.primaryDurationMs = reportInspectedOccurrence->occurrence.durationMs;
        synthesizedAcceptedPattern.primaryStrength = reportInspectedOccurrence->occurrence.strength;
        synthesizedAcceptedPattern.primaryOnsetStrength = reportInspectedOccurrence->occurrence.scalar.onsetStrength;
        synthesizedAcceptedPattern.primaryReleaseStrength = reportInspectedOccurrence->occurrence.scalar.releaseStrength;
        synthesizedAcceptedPattern.primaryAmbientBaseline = diagnostics.acceptedAmbientBaseline;
        synthesizedAcceptedPattern.primaryAudioOverflow = reportInspectedOccurrence->occurrence.scalar.audioOverflowDuringOccurrence;
        synthesizedAcceptedPattern.patternAccepted = true;
        synthesizedAcceptedPattern.patternMatched = true;
        synthesizedAcceptedPattern.supportMatched = true;
        synthesizedAcceptedPattern.valid = true;
        reportPatternResult = &synthesizedAcceptedPattern;
    }
    const detection::FieldState* runtimeFieldState = &_detection.fieldState();
    const detection::DetectionProfile& selectedProfile = detection::detectionProfileForKind(_sequenceTest.profileKind);
    const detection::DetectorReport& activeDetectorReport = _detection.activeDetectorReport();
    const bool activeDetectorReportAvailable = activeDetectorReport.detectorId != detection::DetectorId::Unknown;
    if (activeDetectorReportAvailable) {
        report.detectorReport = &activeDetectorReport;
    }
    const bool trialHasPipelineEvidence = reportPatternResult != nullptr
        && diagnostics.rawPendingCount > 0;
    const long reportPatternDtMs = reportPatternResult != nullptr
        ? selectedTrial.dtMs
        : dtMs;
    const unsigned long reportPatternDurationMs = reportPatternResult != nullptr
        ? selectedTrial.durationMs
        : (durMs >= 0 ? static_cast<unsigned long>(durMs) : 0UL);
    const float reportPatternStrength = reportPatternResult != nullptr
        ? selectedTrial.strength
        : strength;
    const auto artifactReason = [&]() -> const char* {
        if (reportPatternResult != nullptr || report.detectorReport != nullptr) {
            return "captured_from_runtime_pipeline";
        }
        return "missing_pipeline_result";
    }();
    const bool startupArtifact = result == AnalyzerResult::Miss
        && _sequenceTest.currentTrial == 1
        && !trialHasPipelineEvidence
        && report.detectorReport == nullptr
        && strcmp(artifactReason, "missing_pipeline_result") == 0;

    AnalyzerSequenceClassificationInput classificationInput;
    classificationInput.result = result;
    classificationInput.dtMs = reportPatternDtMs;
    classificationInput.rawPendingCount = diagnostics.rawPendingCount;
    classificationInput.bufferOverrun = bufferOverrun;
    classificationInput.patternAvailable = reportPatternResult != nullptr;
    classificationInput.detectorReportAvailable = report.detectorReport != nullptr;
    classificationInput.detectorAcceptedPresent = report.detectorReport != nullptr && report.detectorReport->accepted.present;
    classificationInput.detectorSelectedRejectPresent = report.detectorReport != nullptr && report.detectorReport->selectedReject.present;
    report.classification = classifySequenceTrial(classificationInput);
    {
        // Analyzer formats the runtime PatternResult; it does not reinterpret it.
        AnalyzerPatternObservation pattern = {};
        pattern.type = trialHasPipelineEvidence ? detection::patternTypeName(reportPatternResult->type) : "none";
        pattern.accepted = trialHasPipelineEvidence
            ? reportPatternResult->valid
            : false;
        pattern.patternAccepted = trialHasPipelineEvidence ? reportPatternResult->patternAccepted : false;
        pattern.patternMatched = trialHasPipelineEvidence ? reportPatternResult->patternMatched : false;
        pattern.supportMatched = trialHasPipelineEvidence ? reportPatternResult->supportMatched : false;
        pattern.behaviorEligible = pattern.accepted;
        pattern.confidence = trialHasPipelineEvidence ? reportPatternResult->confidence : 0.0f;
        pattern.dtMs = report.classification.dtMs;
        pattern.supportStrength = trialHasPipelineEvidence && reportInspectedOccurrence != nullptr
            ? strengthClassName(reportInspectedOccurrence->occurrence.scalar.strengthClass)
            : "unknown";
        pattern.reason = trialHasPipelineEvidence ? detection::patternReasonName(reportPatternResult->reasonCode) : "none";
        pattern.rejectReason = trialHasPipelineEvidence ? detection::patternRejectReasonName(reportPatternResult->rejectReason) : "none";
        pattern.involvedOccurrences = trialHasPipelineEvidence ? reportPatternResult->occurrenceCount : 0U;
        report.primaryPattern = pattern;
    }

    report.occurrences.accepted = trialHasPipelineEvidence && reportPatternResult->valid
        ? 1U + static_cast<unsigned int>(diagnostics.duplicateCount)
        : 0U;
    report.occurrences.rejected = _sequenceTest.currentTrialRejected;
    report.occurrences.total = report.occurrences.accepted + report.occurrences.rejected;
    if (trialHasPipelineEvidence && reportInspectedOccurrence != nullptr && reportInspectedOccurrence->occurrence.present) {
        const detection::Occurrence& occurrence = reportInspectedOccurrence->occurrence;
        report.occurrences.kind = occurrenceTypeName(occurrence.occurrenceType);
        report.occurrences.primarySource = occurrenceSourceName(occurrence.detectorId);
        report.occurrences.detectorKind = occurrenceDetectorKindName(occurrence.detectorId);
        report.occurrences.present = occurrence.present;
        report.occurrences.valid = occurrence.valid;
        report.occurrences.startMs = occurrence.startMs;
        report.occurrences.peakMs = occurrence.peakMs;
        report.occurrences.releaseMs = occurrence.releaseMs;
        report.occurrences.primaryDtMs = selectedTrial.dtMs;
        report.occurrences.primaryDurationMs = selectedTrial.durationMs;
        report.occurrences.primaryStrength = selectedTrial.strength;
        report.occurrences.contrast = occurrence.occurrenceType == detection::OccurrenceType::Frequency
            ? occurrence.frequency.contrast
            : 0.0f;
        report.occurrences.strength = selectedTrial.strength;
        report.occurrences.confidence = reportPatternResult != nullptr ? reportPatternResult->confidence : occurrence.confidence;
        report.occurrences.mainRejectReason = reportInspectedOccurrence->decision == detection::OccurrenceDecision::Rejected
            ? occurrenceRejectReasonName(reportInspectedOccurrence->rejectReason)
            : "none";
        report.occurrences.rejectReason = report.occurrences.mainRejectReason;
    } else {
        report.occurrences.kind = "none";
        report.occurrences.primarySource = "unknown";
        report.occurrences.detectorKind = "unknown";
        report.occurrences.present = false;
        report.occurrences.valid = false;
        report.occurrences.startMs = 0;
        report.occurrences.peakMs = 0;
        report.occurrences.releaseMs = 0;
        report.occurrences.primaryDtMs = reportPatternDtMs;
        report.occurrences.primaryDurationMs = reportPatternDurationMs;
        report.occurrences.primaryStrength = reportPatternStrength;
        report.occurrences.contrast = 0.0f;
        report.occurrences.strength = reportPatternStrength;
        report.occurrences.confidence = trialHasPipelineEvidence ? reportPatternResult->confidence : 0.0f;
        report.occurrences.mainRejectReason = analyzerReasonName(report.classification.reason);
        report.occurrences.rejectReason = report.occurrences.mainRejectReason;
    }

    report.inspection.inspected = diagnostics.rawPendingCount;
    report.inspection.accepted = report.occurrences.accepted;
    report.inspection.rejected = report.occurrences.rejected;
    if (trialHasPipelineEvidence && reportInspectedOccurrence != nullptr && reportInspectedOccurrence->occurrence.present) {
        report.inspection.primaryEvidence = occurrenceSourceName(reportInspectedOccurrence->occurrence.detectorId);
        switch (selectedProfile.patternMatcherConfig.requiredSupportTarget) {
            case detection::EvidenceTarget::FrequencyScoreStrength:
                report.inspection.moduleTarget = "frequency_score";
                report.inspection.moduleStrengthClass = strengthClassName(reportInspectedOccurrence->occurrence.frequency.scoreStrength);
                break;
            case detection::EvidenceTarget::TargetBandStrength:
                report.inspection.moduleTarget = "target_band";
                report.inspection.moduleStrengthClass = strengthClassName(reportInspectedOccurrence->occurrence.frequency.targetBandStrength);
                break;
            case detection::EvidenceTarget::SupportStrength:
            default:
                report.inspection.moduleTarget = "support_strength";
                report.inspection.moduleStrengthClass = strengthClassName(reportInspectedOccurrence->occurrence.scalar.strengthClass);
                break;
        }
        report.inspection.mainRejectReason = reportInspectedOccurrence->decision == detection::OccurrenceDecision::Rejected ? occurrenceRejectReasonName(reportInspectedOccurrence->rejectReason) : "none";
    } else {
        report.inspection.primaryEvidence = "none";
        report.inspection.moduleTarget = "unknown";
        report.inspection.moduleStrengthClass = "unsupported";
        report.inspection.mainRejectReason = analyzerReasonName(report.classification.reason);
    }

    if (runtimeFieldState != nullptr) {
        report.field.state = runtimeFieldState->dense ? "dense" : (runtimeFieldState->active ? (runtimeFieldState->quiet ? "quiet" : "active") : "unknown");
        report.field.rawActivity = runtimeFieldState->activity;
        report.field.validPatternActivity = runtimeFieldState->density;
        report.field.recentValidPatterns = runtimeFieldState->recentPatternCount;
        report.field.recentRejects = runtimeFieldState->recentOccurrenceCount > runtimeFieldState->recentPatternCount
            ? runtimeFieldState->recentOccurrenceCount - runtimeFieldState->recentPatternCount
            : 0U;
    } else {
        report.field.state = "unknown";
        report.field.rawActivity = 0.0f;
        report.field.validPatternActivity = 0.0f;
        report.field.recentValidPatterns = 0U;
        report.field.recentRejects = diagnostics.rawPendingCount;
    }

    report.profileDetail.namespaceName = analyzerProfileDetailNamespace(_sequenceTest.profileKind);
    report.profileDetail.summary = analyzerProfileDetailSummary(_sequenceTest.profileKind);
    report.profileDetail.emitter = detection::detectorSelectionName(selectedProfile.detectorSelection);
    report.profileDetail.inspectionAcceptance = detection::detectorSelectionName(selectedProfile.detectorSelection);
    report.profileDetail.inspectionPlan = inspectionPlanName(selectedProfile.inspectionPlan);
    report.profileDetail.inspectionModules = inspectionModulesName(selectedProfile.inspectionPlan);
    report.profileDetail.inspectionModuleCount = selectedProfile.inspectionPlan.count;
    report.profileDetail.evidenceTargets = inspectionEvidenceTargetsName(selectedProfile.inspectionPlan);
    report.profileDetail.requiredSupportTarget = supportTargetDisplayName(
        selectedProfile.patternMatcherConfig.requiredSupportTarget,
        selectedProfile.patternMatcherConfig.requireSupportForAcceptance
    );
    report.profileDetail.supportGate = selectedProfile.patternMatcherConfig.requireSupportForAcceptance ? "enabled" : "disabled";
    report.profileDetail.supportStrengthMin = strengthClassName(selectedProfile.patternMatcherConfig.minimumSupportStrength);
    report.profileDetail.requireSupportForAcceptance = selectedProfile.patternMatcherConfig.requireSupportForAcceptance;
    if (report.occurrences.present &&
        reportInspectedOccurrence != nullptr &&
        reportInspectedOccurrence->occurrence.occurrenceType == detection::OccurrenceType::Frequency) {
        report.profileDetail.supportScore = reportInspectedOccurrence->occurrence.frequency.score;
        report.profileDetail.supportContrast = reportInspectedOccurrence->occurrence.frequency.contrast;
    } else {
        report.profileDetail.supportScore = 0.0f;
        report.profileDetail.supportContrast = 0.0f;
    }
    report.profileDetail.supportScoreMin = selectedProfile.frequencyMatch.attackScoreMin;
    report.profileDetail.supportContrastMin = selectedProfile.frequencyMatch.attackContrastMin;
    report.profileDetail.ampCenteredMagnitude = report.occurrences.primaryStrength;
    report.profileDetail.ampLevel = report.profileDetail.ampCenteredMagnitude;
    report.profileDetail.ampBase = diagnostics.acceptedAmbientBaseline;
    report.profileDetail.ampLift = report.profileDetail.ampCenteredMagnitude - report.profileDetail.ampBase;
    const detection::ScalarInspectionObservation emptyScalarObservation{};
    const detection::ScalarInspectionObservation& selectedScalarObservation =
        trialHasPipelineEvidence && reportInspectedOccurrence != nullptr && reportInspectedOccurrence->occurrence.scalar.evidence.available
            ? reportInspectedOccurrence->occurrence.scalar.evidence
            : emptyScalarObservation;
    report.profileDetail.scalarObservation = selectedScalarObservation;
    report.profileDetail.inspectionObservationCount = 0;
    if (trialHasPipelineEvidence && reportInspectedOccurrence != nullptr) {
        const size_t availableCount = reportInspectedOccurrence->scalarObservationCount;
        const size_t moduleCount = selectedProfile.inspectionPlan.count;
        const size_t copyCount = availableCount < moduleCount ? availableCount : moduleCount;
        report.profileDetail.inspectionObservationCount = copyCount;
        for (size_t i = 0; i < copyCount; ++i) {
            report.profileDetail.inspectionObservations[i] = reportInspectedOccurrence->scalarObservations[i];
        }
    }

    report.debug.occurrences = diagnostics.rawPendingCount;
    report.debug.inspected = diagnostics.rawPendingCount;
    report.debug.patterns = diagnostics.patternAccepted ? 1U : 0U;
    report.debug.rejects = report.occurrences.rejected;
    report.debug.duplicates = duplicateCount;
    report.debug.unexpected = result == AnalyzerResult::Unexpected ? 1U : 0U;
    report.debug.startupArtifact = startupArtifact;
    report.debug.bufferOverrun = bufferOverrun;
    report.debug.artifactCaptured = trialHasPipelineEvidence;
    report.debug.artifactFallback = !trialHasPipelineEvidence;
    report.debug.artifactState = startupArtifact ? "STARTUP_ARTIFACT" : (trialHasPipelineEvidence ? "CAPTURED" : "MISSING_PIPELINE");
    report.debug.artifactReason = artifactReason;
    report.debug.pipelineSource = trialHasPipelineEvidence ? "actual_pipeline" : "missing_runtime_pipeline";
    report.debug.pipelineFallback = !trialHasPipelineEvidence;
    report.debug.mainRejectReason = trialHasPipelineEvidence && reportInspectedOccurrence != nullptr
        ? (reportInspectedOccurrence->decision == detection::OccurrenceDecision::Rejected ? occurrenceRejectReasonName(reportInspectedOccurrence->rejectReason) : "none")
        : analyzerReasonName(report.classification.reason);
}










