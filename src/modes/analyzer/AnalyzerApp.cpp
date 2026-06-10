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
#include "../../detection/detectors/FrequencyMatchDetector.h"
#include "../../detection/patterns/PatternAssembler.h"
#include "../../detection/patterns/PatternNames.h"
#include "../../detection/patterns/PatternRules.h"
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
- raw-trigger and value-mode helpers
- sequence, capture, and base sessions
- diagnostics and summary output
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

unsigned long sampleFramesToMs(unsigned long frames, uint32_t sampleRateHz) {
    if (frames == 0 || sampleRateHz == 0) {
        return 0UL;
    }

    return static_cast<unsigned long>((static_cast<uint64_t>(frames) * 1000ULL) / static_cast<uint64_t>(sampleRateHz));
}

bool analyzerTextIsNone(const char* value) {
    return value == nullptr || strcmp(value, "none") == 0;
}

const char* analyzerTextOrFallback(const char* value, const char* fallback) {
    return value != nullptr ? value : fallback;
}

const detection::DetectorReport& analyzerEmptyDetectorReport() {
    static const detection::DetectorReport kEmptyReport = {};
    return kEmptyReport;
}

const char* analyzerExpectedScalarOccurrenceSource(const detection::DetectionProfile& profile) {
    switch (profile.scalarTransient.observedStream) {
        case detection::FeatureStreamId::FrequencyScore:
        case detection::FeatureStreamId::FrequencyContrast:
            return "frequency";
        case detection::FeatureStreamId::AmpEnvelope:
        case detection::FeatureStreamId::Unknown:
        default:
            return "amp";
    }
}

const char* analyzerScopeFromPeakMs(bool present,
                                    unsigned long peakMs,
                                    unsigned long windowStartMs,
                                    unsigned long windowEndMs) {
    if (!present) {
        return "stale";
    }
    if (peakMs >= windowStartMs && peakMs <= windowEndMs) {
        return "in_window";
    }
    return peakMs < windowStartMs ? "before_window" : "after_window";
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
    Serial.print("  SIZE PatternAssembler=");
    Serial.println(static_cast<unsigned long>(sizeof(detection::PatternAssembler)));
    Serial.print("  SIZE PatternCandidate=");
    Serial.println(static_cast<unsigned long>(sizeof(detection::PatternCandidate)));
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
    if (_baseSession.active) {
        return _baseSession.startedAtMs;
    }
    if (_captureSession.active) {
        return _captureSession.startedAtMs;
    }
    return 0UL;
}

unsigned long AnalyzerApp::activeRunEndMs() const {
    if (_sequenceTest.active && _sequenceTest.currentTrialEndMs > 0) {
        return _sequenceTest.currentTrialEndMs;
    }
    if (_captureSession.active && _captureSession.currentTrialEndMs > 0) {
        return _captureSession.currentTrialEndMs;
    }
    if (_baseSession.active) {
        return millis();
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
    return !_valMode && _sequenceTest.outputConfig.mode != SeqOutputMode::Quiet;
}

bool AnalyzerApp::shouldPrintSequenceSource(const AnalyzerReport& report) const {
    if (_valMode || !_sequenceTest.outputConfig.diagnosticsEnabled) {
        return false;
    }

    switch (_sequenceTest.outputConfig.mode) {
        case SeqOutputMode::Source:
            return sequenceOutputWhenEnabled(_sequenceTest.outputConfig.when, report.classification.result);
        case SeqOutputMode::Full:
            return true;
        default:
            return false;
    }
}

bool AnalyzerApp::shouldPrintSequenceInspect(const AnalyzerReport& report) const {
    if (_valMode || !_sequenceTest.outputConfig.diagnosticsEnabled) {
        return false;
    }

    switch (_sequenceTest.outputConfig.mode) {
        case SeqOutputMode::Inspect:
        case SeqOutputMode::Explain:
            return sequenceOutputWhenEnabled(_sequenceTest.outputConfig.when, report.classification.result);
        case SeqOutputMode::Full:
            return true;
        default:
            return false;
    }
}

bool AnalyzerApp::shouldPrintSequenceSystem(const AnalyzerReport& report) const {
    if (_valMode || !_sequenceTest.outputConfig.diagnosticsEnabled) {
        return false;
    }

    switch (_sequenceTest.outputConfig.mode) {
        case SeqOutputMode::System:
            return sequenceOutputWhenEnabled(_sequenceTest.outputConfig.when, report.classification.result);
        case SeqOutputMode::Full:
            return _sequenceTest.outputConfig.verbosity >= 2U;
        default:
            return false;
    }
}

bool AnalyzerApp::shouldPrintSequenceExplain(const AnalyzerReport& report) const {
    return !_valMode &&
           _sequenceTest.outputConfig.diagnosticsEnabled &&
           _sequenceTest.outputConfig.mode == SeqOutputMode::Explain &&
           sequenceOutputWhenEnabled(_sequenceTest.outputConfig.when, report.classification.result);
}

void buildFrequencyFailReason(const detection::FrequencyBandMeasurementPacket& evidence,
                              const FrequencyMatchEvaluation::Values& tuning,
                              char* out,
                              size_t outSize) {
    FrequencyMatchEvaluation::buildFailReason(evidence, tuning, out, outSize);
}

const char* occurrenceKindName(detection::OccurrenceKind kind) {
    switch (kind) {
        case detection::OccurrenceKind::AmpTransient:
            return "amp_transient";
        case detection::OccurrenceKind::FrequencyMatch:
            return "frequency_match";
        case detection::OccurrenceKind::BroadbandTransient:
            return "broadband_transient";
        case detection::OccurrenceKind::None:
        default:
            return "none";
    }
}

const char* occurrenceSourceName(detection::OccurrenceSource source) {
    switch (source) {
        case detection::OccurrenceSource::Amp:
            return "amp";
        case detection::OccurrenceSource::Frequency:
            return "frequency";
        case detection::OccurrenceSource::Broadband:
            return "broadband";
        case detection::OccurrenceSource::None:
        default:
            return "none";
    }
}

const char* occurrenceDetectorKindName(detection::OccurrenceDetectorKind kind) {
    switch (kind) {
        case detection::OccurrenceDetectorKind::Transient:
            return "transient";
        case detection::OccurrenceDetectorKind::FrequencyMatch:
            return "frequency_match";
        case detection::OccurrenceDetectorKind::Dip:
            return "dip";
        case detection::OccurrenceDetectorKind::Plateau:
            return "plateau";
        case detection::OccurrenceDetectorKind::ThresholdCrossing:
            return "threshold_crossing";
        case detection::OccurrenceDetectorKind::Unknown:
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
        case detection::EvidenceTarget::AmpStrength:
            return "AmpStrength";
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
            case detection::EvidenceTarget::AmpStrength:
            default:
                return "amp_strength";
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
    // Legacy alias surface: keep the old SEQ names for compatibility until the
    // canonical output contract fully replaces them.
    switch (mode) {
        case AnalyzerApp::SeqOutputMode::Quiet:
            return "quiet";
        case AnalyzerApp::SeqOutputMode::Trial:
            return "LEG_trial";
        case AnalyzerApp::SeqOutputMode::Full:
            return "LEG_full";
        case AnalyzerApp::SeqOutputMode::System:
            return "system";
        case AnalyzerApp::SeqOutputMode::Source:
            return "source";
        case AnalyzerApp::SeqOutputMode::Inspect:
            return "inspect";
        case AnalyzerApp::SeqOutputMode::Explain:
            return "explain";
        default:
            return "LEG_trial";
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
                case AnalyzerResult::InvalidAudio:
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
    // Legacy alias surface: accept the old and LEG_* spellings during the
    // migration window so existing scripts keep working.
    if (equalsIgnoreCase(token, "compact") || equalsIgnoreCase(token, "trial") || equalsIgnoreCase(token, "LEG_compact") || equalsIgnoreCase(token, "LEG_trial")) {
        return AnalyzerApp::SeqOutputMode::Trial;
    }
    if (equalsIgnoreCase(token, "full") || equalsIgnoreCase(token, "LEG_full")) {
        return AnalyzerApp::SeqOutputMode::Full;
    }
    if (equalsIgnoreCase(token, "system") || equalsIgnoreCase(token, "LEG_system")) {
        return AnalyzerApp::SeqOutputMode::System;
    }
    if (equalsIgnoreCase(token, "source") || equalsIgnoreCase(token, "LEG_source")) {
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
    _frequencyEvidenceTuning.attackScoreMin = detection::detectionProfileForKind(detection::DetectionProfileKind::TonalPulse).frequencyMatch.attackScoreMin;
    _frequencyEvidenceTuning.releaseScoreMin = detection::detectionProfileForKind(detection::DetectionProfileKind::TonalPulse).frequencyMatch.releaseScoreMin;
    _frequencyEvidenceTuning.attackContrastMin = detection::detectionProfileForKind(detection::DetectionProfileKind::TonalPulse).frequencyMatch.attackContrastMin;
    _frequencyEvidenceTuning.releaseContrastMin = detection::detectionProfileForKind(detection::DetectionProfileKind::TonalPulse).frequencyMatch.releaseContrastMin;
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
    _lastPrintMs = 0;
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
    Serial.println("EVT analyzer_help type='HELP', 'BASE', 'PARAM freqScore=18000 freqContrast=50.0 freqReleaseScore=12000 freqReleaseContrast=50.0', 'TEST', 'RAW trigger f=3200 dur=100 post=1000 dump=bin', 'SEQ MODE quiet|inspect|source|system|explain|LEG_trial|LEG_compact|LEG_full|LEG_system|LEG_source WHEN off|miss|all VERBOSE 0|1|2 STATUS REPORT', 'CAP', 'DET AMP', 'VAL', 'VAL OFF'");
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
                    _sequenceTest.currentTrialDiagnostics.runtimePatternCaptured = true;
                    handleSequenceCandidate(runtimePatternResult, &runtimeFrequencyMeasurementPacket);
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

    updateBaseSession(now);
    // Drain emitter markers before SEQ finalization so trial latches reflect the latest observed state.
    pollEmitterSerial();
    if (_controlClaimPending && !_controlClaimSent && timing::atOrAfter(now, _controlClaimAtMs)) {
        sendEmitterCommand("MODE REMOTE");
        _controlClaimSent = true;
        _controlClaimPending = false;
    }
    updateSequenceTest(now);
    updateCaptureSession(now);
    pollUsbConsole();
    pollEmitterSerial();
    if (_valMode) {
        legacyPrintValueFrame(now);
    }

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

void AnalyzerApp::startBaseSession(unsigned long durationMs, bool quiet) {
    if (durationMs == 0) {
        durationMs = 1;
    }

    stopSequenceTest();
    stopCaptureSession();
    _baseSession.active = true;
    _baseSession.quiet = quiet;
    _baseSession.durationMs = durationMs;
    _baseSession.startedAtMs = millis();
    _baseSession.lastStatusPrintMs = _baseSession.startedAtMs;
    _baseSession.ignoredRawSamples = 0;
    _baseSession.samples = 0;
    _baseSession.rawSum = 0;
    _baseSession.rawMin = 0;
    _baseSession.rawMax = 0;
    _baseSession.deltaSum = 0.0f;
    _baseSession.deltaMin = 0.0f;
    _baseSession.deltaMax = 0.0f;
    _baseSession.baselineSum = 0.0f;
    _baseSession.baselineMin = 0.0f;
    _baseSession.baselineMax = 0.0f;

    sendEmitterCommand("MODE REMOTE");
    delay(100);
    _audioSignal.rebase();
    resetAudioSignalState();
    _audioSignal.resetStats();
    _audioSource.resetStats();
    Serial.println("AUDIO stats reset");

    Serial.print("BASE start dur_ms=");
    Serial.println(durationMs);
    if (!_baseSession.quiet) {
        Serial.println("BASE running");
    }
}

void AnalyzerApp::stopBaseSession() {
    _baseSession.active = false;
}

void AnalyzerApp::updateBaseSession(unsigned long now) {
    if (_valMode) {
        return;
    }
    if (!_baseSession.active) {
        return;
    }

    const int raw = _audioSignal.rawSignal();
    const float delta = static_cast<float>(_audioSignal.centeredSignal());
    const float baseline = _audioSignal.baseline();

    if (raw <= 0) {
        _baseSession.ignoredRawSamples++;
        return;
    }

    if (_baseSession.samples == 0) {
        _baseSession.rawMin = raw;
        _baseSession.rawMax = raw;
        _baseSession.deltaMin = delta;
        _baseSession.deltaMax = delta;
        _baseSession.baselineMin = baseline;
        _baseSession.baselineMax = baseline;
    } else {
        if (raw < _baseSession.rawMin) {
            _baseSession.rawMin = raw;
        }
        if (raw > _baseSession.rawMax) {
            _baseSession.rawMax = raw;
        }
        if (delta < _baseSession.deltaMin) {
            _baseSession.deltaMin = delta;
        }
        if (delta > _baseSession.deltaMax) {
            _baseSession.deltaMax = delta;
        }
        if (baseline < _baseSession.baselineMin) {
            _baseSession.baselineMin = baseline;
        }
        if (baseline > _baseSession.baselineMax) {
            _baseSession.baselineMax = baseline;
        }
    }

    _baseSession.samples++;
    _baseSession.rawSum += static_cast<unsigned long>(raw);
    _baseSession.deltaSum += delta;
    _baseSession.baselineSum += baseline;

    if (AUDIO_VERBOSE_DEBUG && !_baseSession.quiet && timing::elapsedSince(now, _baseSession.lastStatusPrintMs, 5000UL)) {
        const unsigned long avgRaw = _baseSession.samples > 0 ? _baseSession.rawSum / _baseSession.samples : 0;
        const float avgDelta = _baseSession.samples > 0 ? _baseSession.deltaSum / static_cast<float>(_baseSession.samples) : 0.0f;
        const float avgBaseline = _baseSession.samples > 0 ? _baseSession.baselineSum / static_cast<float>(_baseSession.samples) : 0.0f;
        const float baselineDrift = _baseSession.baselineMax - _baseSession.baselineMin;

        Serial.print("BASE status t=");
        Serial.print(now);
        Serial.print(" elapsed_ms=");
        Serial.print(now - _baseSession.startedAtMs);
        Serial.print(" samples=");
        Serial.print(_baseSession.samples);
        Serial.print(" rawSample_avg=");
        Serial.print(avgRaw);
        Serial.print(" rawSample_peak=");
        Serial.print(_baseSession.rawMax);
        Serial.print(" centeredSample_avg=");
        Serial.print(avgDelta, 1);
        Serial.print(" centeredSample_max=");
        Serial.print(_baseSession.deltaMax, 1);
        Serial.print(" baseline_avg=");
        Serial.print(avgBaseline, 1);
        Serial.print(" baseline_drift=");
        Serial.println(baselineDrift, 1);
        _baseSession.lastStatusPrintMs = now;
    }

    if (timing::elapsedSince(now, _baseSession.startedAtMs, _baseSession.durationMs)) {
    legacyPrintBaseSummary();
        stopBaseSession();
        Serial.println("BASE stopped");
    }
}


// -----------------------------------------------------------------------------
// Raw trigger and value-mode helpers
// -----------------------------------------------------------------------------

void AnalyzerApp::legacyPrintValueModeBanner() const {
    if (_valMode) {
        return;
    }
    Serial.print("EVT analyzer_val on source=");
    Serial.print("I2S");
    Serial.println(" probe=AMP");
    printDetectionParameters();
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
        case detection::DetectionProfileKind::Amp:
            return "amp";
        case detection::DetectionProfileKind::ChirpExperimental:
            return "chirp_experimental";
        case detection::DetectionProfileKind::ScalarFreqExperimental:
            return "scalar_freq_experimental";
        case detection::DetectionProfileKind::TonalPulse:
        default:
            return "tonal_pulse";
    }
}

const char* analyzerProfileDetailSummary(detection::DetectionProfileKind profileKind) {
    switch (profileKind) {
        case detection::DetectionProfileKind::Amp:
            return "amp scalar profile view";
        case detection::DetectionProfileKind::ChirpExperimental:
            return "chirp_experimental profile view";
        case detection::DetectionProfileKind::ScalarFreqExperimental:
            return "scalar_freq_experimental experimental profile view";
        case detection::DetectionProfileKind::TonalPulse:
        default:
            return "generic tonal pulse profile view";
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
                                              bool audioOverflow,
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

    const detection::DetectionPipelineResult* pipelineResult = _detection.hasLatestPipelineResult()
        ? &_detection.latestPipelineResult()
        : nullptr;
    const bool runtimeReceivedOccurrence = pipelineResult != nullptr && pipelineResult->hasOccurrence;
    const bool actualPipelineAvailable = pipelineResult != nullptr && pipelineResult->hasPattern;
    const detection::PatternResult* selectedTrialPatternResult = nullptr;
    if (_sequenceTest.primaryValidPatternCaptured) {
        selectedTrialPatternResult = &_sequenceTest.primaryValidPattern;
    } else if (_sequenceTest.rejectedInWindowCount > 0) {
        selectedTrialPatternResult = &_sequenceTest.firstRejectedInWindow;
    }
    // Canonical Analyzer path: only the PatternResult snapshot captured for
    // this finalized trial is allowed onto the clean inspect/explain path.
    // Latest-runtime fallbacks stay out of the canonical trial truth model.
    const detection::PatternResult* reportPatternResult = selectedTrialPatternResult;
    const detection::InspectedOccurrence* reportInspectedOccurrence = nullptr;
    if (reportPatternResult != nullptr && reportPatternResult->inspectedOccurrence.occurrence.present) {
        reportInspectedOccurrence = &reportPatternResult->inspectedOccurrence;
    }
    const detection::FieldState* runtimeFieldState = actualPipelineAvailable && pipelineResult->hasField
        ? &pipelineResult->field
        : nullptr;
    const detection::DetectionProfile& selectedProfile = detection::detectionProfileForKind(_sequenceTest.profileKind);
    const detection::DetectorReport& activeDetectorReport = _detection.activeDetectorReport();
    const bool activeDetectorReportAvailable = activeDetectorReport.detectorId != detection::DetectorId::Unknown;
    const detection::DetectorReport* scalarDetectorReportPtr = _detection.detectorReport(detection::DetectorId::ScalarTransient);
    const bool scalarDetectorReportAvailable = scalarDetectorReportPtr != nullptr;
    const detection::DetectorReport& scalarDetectorReport = scalarDetectorReportAvailable
        ? *scalarDetectorReportPtr
        : analyzerEmptyDetectorReport();
    const detection::DetectorReport* frequencyDetectorReportPtr = _detection.detectorReport(detection::DetectorId::FrequencyMatch);
    const bool frequencyDetectorReportAvailable = frequencyDetectorReportPtr != nullptr;
    const detection::DetectorReport& frequencyDetectorReport = frequencyDetectorReportAvailable
        ? *frequencyDetectorReportPtr
        : analyzerEmptyDetectorReport();
    if (activeDetectorReportAvailable) {
        report.detectorReport = &activeDetectorReport;
    }
    const bool trialHasPipelineEvidence = reportPatternResult != nullptr
        && diagnostics.rawCandidateCount > 0;
    const long reportPatternDtMs = reportPatternResult != nullptr
        ? static_cast<long>(reportPatternResult->primaryStartMs) - static_cast<long>(_sequenceTest.currentTrialScheduledAtMs)
        : dtMs;
    const unsigned long reportPatternDurationMs = reportPatternResult != nullptr
        ? reportPatternResult->primaryDurationMs
        : (durMs >= 0 ? static_cast<unsigned long>(durMs) : 0UL);
    const float reportPatternStrength = reportPatternResult != nullptr
        ? reportPatternResult->primaryStrength
        : strength;
    const auto artifactReason = [&]() -> const char* {
        if (reportPatternResult != nullptr || actualPipelineAvailable) {
            return "captured_from_runtime_pipeline";
        }
        return "missing_pipeline_result";
    }();
    const bool startupArtifact = result == AnalyzerResult::Miss
        && _sequenceTest.currentTrial == 1
        && !trialHasPipelineEvidence
        && !actualPipelineAvailable
        && strcmp(artifactReason, "missing_pipeline_result") == 0;

    AnalyzerSequenceClassificationInput classificationInput;
    classificationInput.result = result;
    classificationInput.dtMs = reportPatternDtMs;
    classificationInput.rawCandidateCount = diagnostics.rawCandidateCount;
    classificationInput.audioOverflow = audioOverflow;
    classificationInput.patternAvailable = reportPatternResult != nullptr;
    classificationInput.detectorReportAvailable = report.detectorReport != nullptr;
    classificationInput.detectorAcceptedPresent = report.detectorReport != nullptr && report.detectorReport->accepted.present;
    classificationInput.detectorSelectedRejectPresent = report.detectorReport != nullptr && report.detectorReport->selectedReject.present;
    report.classification = classifySequenceTrial(classificationInput);
    {
        // Analyzer consumes the PatternResult produced by DetectionRuntime.
        // Analyzer does not re-run occurrence inspection or pattern interpretation.
        AnalyzerPatternObservation pattern = {};
        pattern.type = trialHasPipelineEvidence ? detection::patternTypeName(reportPatternResult->type) : "none";
        pattern.accepted = trialHasPipelineEvidence
            ? reportPatternResult->valid
            : false;
        pattern.candidateAccepted = trialHasPipelineEvidence ? reportPatternResult->patternCandidateAccepted : false;
        pattern.patternMatched = trialHasPipelineEvidence ? reportPatternResult->patternMatched : false;
        pattern.supportMatched = trialHasPipelineEvidence ? reportPatternResult->supportMatched : false;
        pattern.behaviorEligible = pattern.accepted;
        pattern.confidence = trialHasPipelineEvidence ? reportPatternResult->confidence : 0.0f;
        pattern.dtMs = report.classification.dtMs;
        pattern.ampStrength = trialHasPipelineEvidence ? strengthClassName(reportPatternResult->ampStrength) : "unknown";
        pattern.reason = trialHasPipelineEvidence ? detection::patternReasonName(reportPatternResult->reasonCode) : "none";
        pattern.rejectReason = trialHasPipelineEvidence ? detection::patternRejectReasonName(reportPatternResult->rejectReason) : "none";
        pattern.involvedOccurrences = trialHasPipelineEvidence ? reportPatternResult->occurrenceCount : 0U;
        report.primaryPattern = pattern;
    }

    report.occurrences.total = diagnostics.rawCandidateCount;
    report.occurrences.accepted = trialHasPipelineEvidence && reportPatternResult->valid ? 1U : 0U;
    report.occurrences.rejected = diagnostics.rawCandidateCount > report.occurrences.accepted ? diagnostics.rawCandidateCount - report.occurrences.accepted : 0U;
    if (trialHasPipelineEvidence && reportInspectedOccurrence != nullptr && reportInspectedOccurrence->occurrence.present) {
        const detection::Occurrence& occurrence = reportInspectedOccurrence->occurrence;
        report.occurrences.kind = occurrenceKindName(occurrence.kind);
        report.occurrences.primarySource = occurrenceSourceName(occurrence.source);
        report.occurrences.detectorKind = occurrenceDetectorKindName(occurrence.detectorKind);
        report.occurrences.present = occurrence.present;
        report.occurrences.valid = occurrence.valid;
        report.occurrences.startMs = occurrence.startMs;
        report.occurrences.peakMs = occurrence.peakMs;
        report.occurrences.releaseMs = occurrence.releaseMs;
        report.occurrences.primaryDtMs = static_cast<long>(occurrence.startMs) - static_cast<long>(_sequenceTest.currentTrialScheduledAtMs);
        report.occurrences.primaryDurationMs = occurrence.durationMs;
        report.occurrences.primaryStrength = occurrence.strength;
        report.occurrences.score = occurrence.score;
        report.occurrences.contrast = occurrence.contrast;
        report.occurrences.strength = occurrence.strength;
        report.occurrences.confidence = occurrence.confidence;
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
        report.occurrences.score = 0.0f;
        report.occurrences.contrast = 0.0f;
        report.occurrences.strength = reportPatternStrength;
        report.occurrences.confidence = trialHasPipelineEvidence ? reportPatternResult->confidence : 0.0f;
        report.occurrences.mainRejectReason = analyzerReasonName(report.classification.reason);
        report.occurrences.rejectReason = report.occurrences.mainRejectReason;
    }

    report.inspection.inspected = diagnostics.rawCandidateCount;
    report.inspection.accepted = report.occurrences.accepted;
    report.inspection.rejected = diagnostics.rawCandidateCount > report.inspection.accepted ? diagnostics.rawCandidateCount - report.inspection.accepted : 0U;
    if (trialHasPipelineEvidence && reportInspectedOccurrence != nullptr && reportInspectedOccurrence->occurrence.present) {
        report.inspection.primaryEvidence = occurrenceSourceName(reportInspectedOccurrence->occurrence.source);
        switch (selectedProfile.patternRulesConfig.requiredSupportTarget) {
            case detection::EvidenceTarget::FrequencyScoreStrength:
                report.inspection.moduleTarget = "frequency_score";
                report.inspection.moduleStrengthClass = strengthClassName(reportInspectedOccurrence->occurrence.frequencyScoreStrength);
                break;
            case detection::EvidenceTarget::TargetBandStrength:
                report.inspection.moduleTarget = "target_band";
                report.inspection.moduleStrengthClass = strengthClassName(reportInspectedOccurrence->occurrence.targetBandStrength);
                break;
            case detection::EvidenceTarget::AmpStrength:
            default:
                report.inspection.moduleTarget = "amp_strength";
                report.inspection.moduleStrengthClass = strengthClassName(reportInspectedOccurrence->occurrence.ampStrength);
                break;
        }
        report.inspection.mainRejectReason = reportInspectedOccurrence->decision == detection::OccurrenceDecision::Rejected ? occurrenceRejectReasonName(reportInspectedOccurrence->rejectReason) : "none";
    } else {
        report.inspection.primaryEvidence = "none";
        report.inspection.moduleTarget = "unknown";
        report.inspection.moduleStrengthClass = "unsupported";
        report.inspection.mainRejectReason = analyzerReasonName(report.classification.reason);
    }

    if (actualPipelineAvailable && runtimeFieldState != nullptr) {
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
        report.field.recentRejects = diagnostics.rawCandidateCount;
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
        selectedProfile.patternRulesConfig.requiredSupportTarget,
        selectedProfile.patternRulesConfig.requireSupportForAcceptance
    );
    report.profileDetail.ampStrength = selectedProfile.patternRulesConfig.requireSupportForAcceptance ? "enabled" : "disabled";
    report.profileDetail.ampStrengthMin = strengthClassName(selectedProfile.patternRulesConfig.minimumSupportStrength);
    report.profileDetail.requireSupportForAcceptance = selectedProfile.patternRulesConfig.requireSupportForAcceptance;
    if (report.occurrences.present) {
        report.profileDetail.freqScore = report.occurrences.score;
        report.profileDetail.freqContrast = report.occurrences.contrast;
    } else {
        report.profileDetail.freqScore = 0.0f;
        report.profileDetail.freqContrast = 0.0f;
    }
    report.profileDetail.freqScoreMin = selectedProfile.frequencyMatch.attackScoreMin;
    report.profileDetail.freqContrastMin = selectedProfile.frequencyMatch.attackContrastMin;
    report.profileDetail.ampCenteredMagnitude = report.occurrences.primaryStrength;
    report.profileDetail.ampLevel = report.profileDetail.ampCenteredMagnitude;
    report.profileDetail.ampBase = diagnostics.acceptedAmbientBaseline;
    report.profileDetail.ampLift = report.profileDetail.ampCenteredMagnitude - report.profileDetail.ampBase;
    const detection::ScalarInspectionObservation emptyScalarObservation{};
    const detection::ScalarInspectionObservation& selectedScalarObservation =
        trialHasPipelineEvidence && reportInspectedOccurrence != nullptr && reportInspectedOccurrence->occurrence.scalarEvidence.available
            ? reportInspectedOccurrence->occurrence.scalarEvidence
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

    report.debug.occurrences = diagnostics.rawCandidateCount;
    report.debug.inspected = diagnostics.rawCandidateCount;
    report.debug.patterns = diagnostics.patternAccepted ? 1U : 0U;
    report.debug.rejects = report.occurrences.rejected;
    report.debug.duplicates = duplicateCount;
    report.debug.unexpected = result == AnalyzerResult::Unexpected ? 1U : 0U;
    report.debug.startupArtifact = startupArtifact;
    report.debug.artifactCaptured = trialHasPipelineEvidence;
    report.debug.artifactFallback = !trialHasPipelineEvidence;
    report.debug.artifactState = startupArtifact ? "STARTUP_ARTIFACT" : (trialHasPipelineEvidence ? "CAPTURED" : "MISSING_PIPELINE");
    report.debug.artifactReason = artifactReason;
    report.debug.pipelineSource = trialHasPipelineEvidence ? "actual_pipeline" : "missing_runtime_pipeline";
    report.debug.pipelineFallback = !trialHasPipelineEvidence;
    report.debug.mainRejectReason = trialHasPipelineEvidence && reportInspectedOccurrence != nullptr
        ? (reportInspectedOccurrence->decision == detection::OccurrenceDecision::Rejected ? occurrenceRejectReasonName(reportInspectedOccurrence->rejectReason) : "none")
        : analyzerReasonName(report.classification.reason);

    const bool diagnosticsRequested = _sequenceTest.outputConfig.when != AnalyzerApp::SeqOutputWhen::Off &&
        _sequenceTest.outputConfig.diagnosticsEnabled;
    const detection::DetectionDiagnostics* runtimeDiag = nullptr;
    const FrequencyMatchDetector* frequencyDetector = nullptr;
    if (diagnosticsRequested) {
        // LEGACY_DIAGNOSTICS_COMPAT
        //
        // The clean Analyzer paths above consume PatternResult +
        // DetectorReport + expected-window facts only.
        //
        // Everything below this capture point exists to populate legacy
        // analyzer compatibility structs and printers. Do not route new
        // canonical output through DetectionDiagnostics.
        _detection.captureDiagnostics();
        runtimeDiag = &_detection.diagnostics();
        frequencyDetector = &_detection.frequencyDetector();
    }
    report.debug.patternResultQueueOverflowCount = runtimeDiag != nullptr
        ? runtimeDiag->patternResultQueueOverflowCount
        : 0UL;

    // Legacy analyzer compatibility adapter:
    // canonical/runtime facts -> legacy source/detector summary structs.
    report.source.frequencyMatch.currentTrialId = report.context.trial;
    report.source.frequencyMatch.windowStartMs = _sequenceTest.currentTrialStartMs;
    report.source.frequencyMatch.windowEndMs = _sequenceTest.currentTrialEndMs;
    report.source.frequencyMatch.expectedWindowMs = _sequenceTest.currentTrialEndMs >= _sequenceTest.currentTrialStartMs
        ? _sequenceTest.currentTrialEndMs - _sequenceTest.currentTrialStartMs
        : 0UL;
    report.source.frequencyMatch.expectedFrameCountEstimate =
        static_cast<unsigned long>((report.source.frequencyMatch.expectedWindowMs
            * static_cast<unsigned long>(_audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL)) / 1000UL);

    report.source.frequencyMatch.acceptedPresent = report.occurrences.present
        && report.occurrences.valid
        && report.primaryPattern.accepted;
    report.source.frequencyMatch.acceptedTrialId = report.source.frequencyMatch.acceptedPresent ? report.context.trial : 0UL;
    report.source.frequencyMatch.acceptedSource = report.source.frequencyMatch.acceptedPresent
        ? (report.occurrences.primarySource != nullptr ? report.occurrences.primarySource : "unknown")
        : "none";
    report.source.frequencyMatch.acceptedDtMs = report.source.frequencyMatch.acceptedPresent ? report.occurrences.primaryDtMs : -1;
    report.source.frequencyMatch.acceptedStartMs = report.source.frequencyMatch.acceptedPresent ? report.occurrences.startMs : 0UL;
    report.source.frequencyMatch.acceptedPeakMs = report.source.frequencyMatch.acceptedPresent ? report.occurrences.peakMs : 0UL;
    report.source.frequencyMatch.acceptedReleaseMs = report.source.frequencyMatch.acceptedPresent ? report.occurrences.releaseMs : 0UL;
    report.source.frequencyMatch.acceptedDurationMs = report.source.frequencyMatch.acceptedPresent ? report.occurrences.primaryDurationMs : 0UL;
    report.source.frequencyMatch.acceptedStrength = report.source.frequencyMatch.acceptedPresent ? report.occurrences.primaryStrength : 0.0f;
    report.source.frequencyMatch.acceptedScore = report.source.frequencyMatch.acceptedPresent ? report.occurrences.score : 0.0f;
    report.source.frequencyMatch.acceptedContrast = report.source.frequencyMatch.acceptedPresent ? report.occurrences.contrast : 0.0f;
    report.source.frequencyMatch.freshFrames = _freqBandStream.profileComputeCalls();
    report.source.frequencyMatch.heldFrames = _freqBandStream.profileObserveCalls() > _freqBandStream.profileComputeCalls()
        ? _freqBandStream.profileObserveCalls() - _freqBandStream.profileComputeCalls()
        : 0UL;
    report.source.frequencyMatch.historyScoreRecords = _detection.featureHistory().sampleCount(detection::FeatureStreamId::FrequencyScore);
    report.source.frequencyMatch.historyContrastRecords = _detection.featureHistory().sampleCount(detection::FeatureStreamId::FrequencyContrast);
    const auto& frequencyAccepted = frequencyDetectorReport.accepted;
    const auto& frequencySelectedReject = frequencyDetectorReport.selectedReject;
    const auto& frequencyDetail = frequencyDetectorReport.frequency;
    if (frequencyDetectorReportAvailable) {
        report.source.frequencyMatch.acceptedPresent = frequencyAccepted.present;
        report.source.frequencyMatch.acceptedTrialId = report.source.frequencyMatch.acceptedPresent ? report.context.trial : 0UL;
        report.source.frequencyMatch.acceptedSource = report.source.frequencyMatch.acceptedPresent
            ? (report.occurrences.primarySource != nullptr ? report.occurrences.primarySource : "frequency")
            : "none";
        report.source.frequencyMatch.acceptedDtMs = report.source.frequencyMatch.acceptedPresent
            ? static_cast<long>(frequencyAccepted.startMs) - static_cast<long>(_sequenceTest.currentTrialScheduledAtMs)
            : -1L;
        report.source.frequencyMatch.acceptedStartMs = report.source.frequencyMatch.acceptedPresent ? frequencyAccepted.startMs : 0UL;
        report.source.frequencyMatch.acceptedPeakMs = report.source.frequencyMatch.acceptedPresent ? frequencyAccepted.peakMs : 0UL;
        report.source.frequencyMatch.acceptedReleaseMs = report.source.frequencyMatch.acceptedPresent ? frequencyAccepted.endMs : 0UL;
        report.source.frequencyMatch.acceptedDurationMs = report.source.frequencyMatch.acceptedPresent ? frequencyAccepted.durationMs : 0UL;
        report.source.frequencyMatch.acceptedStrength = report.source.frequencyMatch.acceptedPresent ? frequencyAccepted.strength : 0.0f;
        report.source.frequencyMatch.acceptedScore = report.source.frequencyMatch.acceptedPresent ? frequencyDetail.accepted.score : 0.0f;
        report.source.frequencyMatch.acceptedContrast = report.source.frequencyMatch.acceptedPresent ? frequencyDetail.accepted.contrast : 0.0f;
    }

    bool hasCurrentSourceEvidence = false;
    if (runtimeDiag != nullptr) {
        report.source.frequencyMatch.frames = runtimeDiag->frequencyFrames;
        report.source.frequencyMatch.validFrames = runtimeDiag->frequencyValidFrames;
        report.source.frequencyMatch.scoreOkUpdates = runtimeDiag->frequencyScoreOkFrames;
        report.source.frequencyMatch.contrastOkUpdates = runtimeDiag->frequencyContrastOkFrames;
        report.source.frequencyMatch.bothOkUpdates = runtimeDiag->frequencyBothOkFrames;
        report.source.frequencyMatch.matchFrames = runtimeDiag->frequencyMatchFrames;
        report.source.frequencyMatch.rejectFrames = runtimeDiag->frequencyRejectFrames;
        report.source.frequencyMatch.releaseScoreOkFrames = runtimeDiag->frequencyReleaseScoreOkFrames;
        report.source.frequencyMatch.releaseContrastOkFrames = runtimeDiag->frequencyReleaseContrastOkFrames;
        report.source.frequencyMatch.releaseBothOkFrames = runtimeDiag->frequencyReleaseBothOkFrames;
        report.source.frequencyMatch.releaseScoreTooLowFrames = runtimeDiag->frequencyReleaseScoreTooLowFrames;
        report.source.frequencyMatch.releaseContrastTooLowFrames = runtimeDiag->frequencyReleaseContrastTooLowFrames;
        report.source.frequencyMatch.releaseScoreAndContrastTooLowFrames = runtimeDiag->frequencyReleaseScoreAndContrastTooLowFrames;
        report.source.frequencyMatch.releaseNoEvidenceFrames = runtimeDiag->frequencyReleaseNoEvidenceFrames;
        report.source.frequencyMatch.diagLongestMatchStreakFrames = runtimeDiag->frequencyDiagLongestMatchStreakFrames;
        report.source.frequencyMatch.diagLongestMatchStreakMs = sampleFramesToMs(
            runtimeDiag->frequencyDiagLongestMatchStreakFrames,
            _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL
        );
        const unsigned long sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
        report.source.frequencyMatch.windowMs = sampleFramesToMs(_freqBandStream.windowSizeSamples(), sampleRateHz);
        report.source.frequencyMatch.updateStepMs = sampleFramesToMs(_freqBandStream.frequencyUpdateEverySamples(), sampleRateHz);
        report.source.frequencyMatch.overlapRatio = _freqBandStream.windowSizeSamples() > 0
            ? (1.0f - (static_cast<float>(_freqBandStream.frequencyUpdateEverySamples()) / static_cast<float>(_freqBandStream.windowSizeSamples())))
            : 0.0f;
        if (report.source.frequencyMatch.overlapRatio < 0.0f) {
            report.source.frequencyMatch.overlapRatio = 0.0f;
        }
        report.source.frequencyMatch.freshUpdateCount = _freqBandStream.profileComputeCalls();
        report.source.frequencyMatch.heldUpdateCount = _freqBandStream.profileObserveCalls() > _freqBandStream.profileComputeCalls()
            ? _freqBandStream.profileObserveCalls() - _freqBandStream.profileComputeCalls()
            : 0UL;
        report.source.frequencyMatch.bucketCount = report.source.frequencyMatch.frames;
        report.source.frequencyMatch.valueCount = report.source.frequencyMatch.freshUpdateCount;
        report.source.frequencyMatch.matchedUpdateCount = runtimeDiag->frequencyMatchFrames;
        report.source.frequencyMatch.candidateDurationMs = runtimeDiag->frequencyDurationMs;
        report.source.frequencyMatch.spanMs = runtimeDiag->frequencyDurationMs;
        report.source.frequencyMatch.matchedSpanMs = report.frequency.diagLongestMatchStreakMs;
        report.source.frequencyMatch.matchedCoverageMs = runtimeDiag->sourceSummary.totalMatchMs;
        report.source.frequencyMatch.latestValueAgeMs = sampleFramesToMs(_freqBandStream.lastPacketAgeSamples(), sampleRateHz);
        report.source.frequencyMatch.freshCoverageRatio = report.source.frequencyMatch.frames > 0
            ? static_cast<float>(report.source.frequencyMatch.freshUpdateCount) / static_cast<float>(report.source.frequencyMatch.frames)
            : 0.0f;
        report.source.frequencyMatch.audioHealth = diagnostics.audioHealth != nullptr ? diagnostics.audioHealth : "unknown";
        report.source.frequencyMatch.audioZeroishFrames = diagnostics.audioZeroishFrames;
        report.source.frequencyMatch.audioFlatlineFrames = diagnostics.audioFlatlineFrames;
        report.source.frequencyMatch.audioLargeJumpFrames = diagnostics.audioLargeJumpFrames;
        report.source.frequencyMatch.audioRmsTooLowFrames = diagnostics.audioRmsTooLowFrames;
        report.source.frequencyMatch.audioRmsTooHighFrames = diagnostics.audioRmsTooHighFrames;
        report.source.frequencyMatch.audioMaxAbsDelta = diagnostics.audioMaxAbsDelta;
        report.source.frequencyMatch.meanScore = runtimeDiag->frequencyScoreMean;
        report.source.frequencyMatch.meanContrast = runtimeDiag->frequencyContrastMean;
        report.source.frequencyMatch.sumScore = report.source.frequencyMatch.meanScore * static_cast<float>(report.source.frequencyMatch.frames);
        report.source.frequencyMatch.sumContrast = report.source.frequencyMatch.meanContrast * static_cast<float>(report.source.frequencyMatch.frames);
        report.source.frequencyMatch.scoreThreshold = runtimeDiag->frequencyScoreThreshold;
        report.source.frequencyMatch.contrastThreshold = runtimeDiag->frequencyContrastThreshold;
        report.source.frequencyMatch.maxScore = runtimeDiag->frequencyScoreMax;
        report.source.frequencyMatch.maxScoreMs = runtimeDiag->frequencyScoreMaxMs;
        report.source.frequencyMatch.maxContrast = runtimeDiag->frequencyContrastMax;
        report.source.frequencyMatch.maxContrastMs = runtimeDiag->frequencyContrastMaxMs;
        report.source.frequencyMatch.targetPowerMean = runtimeDiag->frequencyTargetPowerMean;
        report.source.frequencyMatch.lowerPowerMean = runtimeDiag->frequencyLowerPowerMean;
        report.source.frequencyMatch.upperPowerMean = runtimeDiag->frequencyUpperPowerMean;
        report.source.frequencyMatch.neighborPowerMean = runtimeDiag->frequencyNeighborPowerMean;
        report.source.frequencyMatch.neighborPowerMaxMean = runtimeDiag->frequencyNeighborPowerMaxMean;
        report.source.frequencyMatch.targetPowerMax = runtimeDiag->frequencyTargetPowerMax;
        report.source.frequencyMatch.lowerPowerMax = runtimeDiag->frequencyLowerPowerMax;
        report.source.frequencyMatch.upperPowerMax = runtimeDiag->frequencyUpperPowerMax;
        report.source.frequencyMatch.neighborPowerMeanMax = runtimeDiag->frequencyNeighborPowerMeanMax;
        report.source.frequencyMatch.neighborPowerMaxMax = runtimeDiag->frequencyNeighborPowerMaxMax;
        report.source.frequencyMatch.targetPowerMaxMs = runtimeDiag->frequencyTargetPowerMaxMs;
        report.source.frequencyMatch.lowerPowerMaxMs = runtimeDiag->frequencyLowerPowerMaxMs;
        report.source.frequencyMatch.upperPowerMaxMs = runtimeDiag->frequencyUpperPowerMaxMs;
        report.source.frequencyMatch.neighborPowerMeanMaxMs = runtimeDiag->frequencyNeighborPowerMeanMaxMs;
        report.source.frequencyMatch.neighborPowerMaxMaxMs = runtimeDiag->frequencyNeighborPowerMaxMaxMs;
        report.source.frequencyMatch.lowerScoreMean = runtimeDiag->frequencyLowerScoreMean;
        report.source.frequencyMatch.upperScoreMean = runtimeDiag->frequencyUpperScoreMean;
        report.source.frequencyMatch.lowerScoreMax = runtimeDiag->frequencyLowerScoreMax;
        report.source.frequencyMatch.upperScoreMax = runtimeDiag->frequencyUpperScoreMax;
        report.source.frequencyMatch.lowerScoreMaxMs = runtimeDiag->frequencyLowerScoreMaxMs;
        report.source.frequencyMatch.upperScoreMaxMs = runtimeDiag->frequencyUpperScoreMaxMs;
        report.source.frequencyMatch.ampPeak = diagnostics.ambientBaselineSamples > 0
            ? static_cast<float>(diagnostics.maxSignalLevel)
            : 0.0f;
        report.source.frequencyMatch.ampMean = diagnostics.ambientBaselineSamples > 0
            ? diagnostics.ambientBaselineSum / static_cast<float>(diagnostics.ambientBaselineSamples)
            : 0.0f;
        report.source.frequencyMatch.ampPeakMs = diagnostics.ambientBaselineSamples > 0
            ? diagnostics.ampPeakMs
            : 0UL;
        report.source.frequencyMatch.minScore = runtimeDiag->frequencyScoreMin;
        report.source.frequencyMatch.minContrast = runtimeDiag->frequencyContrastMin;
        report.source.frequencyMatch.peakScore = runtimeDiag->frequencyPeakScore;
        report.source.frequencyMatch.peakContrast = runtimeDiag->frequencyPeakContrast;
        report.source.frequencyMatch.peakSampleCount = runtimeDiag->frequencyPeakSampleCount;
        report.source.frequencyMatch.targetPresent = report.source.frequencyMatch.matchedUpdateCount > 0
            || report.source.frequencyMatch.fmOpened
            || report.source.frequencyMatch.fmEmitted;
        const float targetScoreThreshold = report.source.frequencyMatch.scoreThreshold > 0.0f
            ? report.source.frequencyMatch.scoreThreshold
            : 0.0f;
        const float targetPartialThreshold = targetScoreThreshold > 0.0f
            ? targetScoreThreshold * 0.75f
            : 0.0f;
        const float targetNoiseFloor = targetScoreThreshold > 0.0f
            ? targetScoreThreshold * 0.15f
            : 0.0f;
        const bool targetPartial = !report.source.frequencyMatch.targetPresent
            && report.source.frequencyMatch.maxScore >= targetPartialThreshold
            && report.source.frequencyMatch.maxScore < targetScoreThreshold;
        report.source.frequencyMatch.weakTarget = !report.source.frequencyMatch.targetPresent
            && report.source.frequencyMatch.maxScore > targetNoiseFloor
            && report.source.frequencyMatch.maxScore < targetPartialThreshold;
        report.source.frequencyMatch.noTarget = !report.source.frequencyMatch.targetPresent
            && !targetPartial
            && !report.source.frequencyMatch.weakTarget;
        if (report.source.frequencyMatch.targetPresent) {
            report.source.frequencyMatch.targetEvidenceClass = "present";
        } else if (targetPartial) {
            report.source.frequencyMatch.targetEvidenceClass = "partial";
        } else if (report.source.frequencyMatch.weakTarget) {
            report.source.frequencyMatch.targetEvidenceClass = "weak";
        } else {
            report.source.frequencyMatch.targetEvidenceClass = "none";
        }
        report.source.frequencyMatch.sourceSummary.present = runtimeDiag->sourceSummary.present;
        report.source.frequencyMatch.sourceSummary.origin = "runtime_frequency_diag";
        report.source.frequencyMatch.sourceSummary.candidateCount = runtimeDiag->sourceSummary.candidateCount;
        report.source.frequencyMatch.sourceSummary.rejectCount = runtimeDiag->sourceSummary.rejectCount;
        report.source.frequencyMatch.sourceSummary.bestDurationMs = runtimeDiag->sourceSummary.bestDurationMs;
        report.source.frequencyMatch.sourceSummary.secondBestDurationMs = runtimeDiag->sourceSummary.secondBestDurationMs;
        report.source.frequencyMatch.sourceSummary.bestOpenMs = runtimeDiag->sourceSummary.bestOpenMs;
        report.source.frequencyMatch.sourceSummary.bestPeakMs = runtimeDiag->sourceSummary.bestPeakMs;
        report.source.frequencyMatch.sourceSummary.bestLastMatchMs = runtimeDiag->sourceSummary.bestLastMatchMs;
        report.source.frequencyMatch.sourceSummary.bestCloseMs = runtimeDiag->sourceSummary.bestCloseMs;
        report.source.frequencyMatch.sourceSummary.bestPeakPrimary = runtimeDiag->sourceSummary.bestPeakPrimary;
        report.source.frequencyMatch.sourceSummary.bestPeakSecondary = runtimeDiag->sourceSummary.bestPeakSecondary;
        report.source.frequencyMatch.sourceSummary.bestRejectReason = runtimeDiag->sourceSummary.bestRejectReason;
        report.source.frequencyMatch.sourceSummary.bestGateReason = runtimeDiag->sourceSummary.bestGateReason;
        report.source.frequencyMatch.sourceSummary.closeCause = runtimeDiag->sourceSummary.closeCause;
        report.source.frequencyMatch.sourceSummary.scoreTooLowFrames = runtimeDiag->sourceSummary.scoreTooLowFrames;
        report.source.frequencyMatch.sourceSummary.contrastTooLowFrames = runtimeDiag->sourceSummary.contrastTooLowFrames;
        report.source.frequencyMatch.sourceSummary.scoreAndContrastTooLowFrames = runtimeDiag->sourceSummary.scoreAndContrastTooLowFrames;
        report.source.frequencyMatch.sourceSummary.maxPeakPrimary = runtimeDiag->sourceSummary.maxPeakPrimary;
        report.source.frequencyMatch.sourceSummary.maxPeakPrimaryMs = runtimeDiag->sourceSummary.maxPeakPrimaryMs;
        report.source.frequencyMatch.sourceSummary.maxPeakSecondary = runtimeDiag->sourceSummary.maxPeakSecondary;
        report.source.frequencyMatch.sourceSummary.maxPeakSecondaryMs = runtimeDiag->sourceSummary.maxPeakSecondaryMs;
        report.source.frequencyMatch.sourceSummary.totalMatchMs = runtimeDiag->sourceSummary.totalMatchMs;
        report.source.frequencyMatch.sourceSummary.totalGapMs = runtimeDiag->sourceSummary.totalGapMs;
        report.source.frequencyMatch.sourceSummary.maxGapMs = runtimeDiag->sourceSummary.maxGapMs;
        report.source.frequencyMatch.sourceSummary.islandCount = runtimeDiag->sourceSummary.islandCount;
        report.source.frequencyMatch.sourceLastCandidate.present = runtimeDiag->sourceLastCandidate.present;
        report.source.frequencyMatch.sourceLastCandidate.peakMs = runtimeDiag->sourceLastCandidate.peakMs;
        report.source.frequencyMatch.sourceLastCandidate.durationMs = runtimeDiag->sourceLastCandidate.durationMs;
        report.source.frequencyMatch.sourceLastCandidate.sampleCount = runtimeDiag->sourceLastCandidate.sampleCount;
        report.source.frequencyMatch.sourceLastCandidate.peakPrimary = runtimeDiag->sourceLastCandidate.peakPrimary;
        report.source.frequencyMatch.sourceLastCandidate.peakSecondary = runtimeDiag->sourceLastCandidate.peakSecondary;
        report.source.frequencyMatch.sourceLastCandidate.reason = runtimeDiag->sourceLastCandidate.reason;
        report.source.frequencyMatch.sourceLastCandidate.gateReason = runtimeDiag->sourceLastCandidate.gateReason;
        report.source.frequencyMatch.sourceLastCandidate.scope = report.source.frequencyMatch.sourceLastCandidate.present
            ? (report.source.frequencyMatch.sourceLastCandidate.peakMs >= report.source.frequencyMatch.windowStartMs && report.source.frequencyMatch.sourceLastCandidate.peakMs <= report.source.frequencyMatch.windowEndMs
                ? "in_window"
                : (report.source.frequencyMatch.sourceLastCandidate.peakMs < report.source.frequencyMatch.windowStartMs ? "before_window" : "after_window"))
            : "stale";
        hasCurrentSourceEvidence = report.source.frequencyMatch.acceptedPresent || report.source.frequencyMatch.sourceSummary.present;
        if (!hasCurrentSourceEvidence) {
            report.source.frequencyMatch.sourceLastCandidate.present = false;
            report.source.frequencyMatch.sourceLastCandidate.peakMs = 0;
            report.source.frequencyMatch.sourceLastCandidate.durationMs = 0;
            report.source.frequencyMatch.sourceLastCandidate.sampleCount = 0;
            report.source.frequencyMatch.sourceLastCandidate.peakPrimary = 0.0f;
            report.source.frequencyMatch.sourceLastCandidate.peakSecondary = 0.0f;
            report.source.frequencyMatch.sourceLastCandidate.reason = "none";
            report.source.frequencyMatch.sourceLastCandidate.gateReason = "none";
            report.source.frequencyMatch.sourceLastCandidate.scope = "unknown";
        }
        report.source.frequencyMatch.liveFreqReason = runtimeDiag->frequencyRejectReason != nullptr ? runtimeDiag->frequencyRejectReason : "none";
        report.source.frequencyMatch.liveFreqWould = runtimeDiag->frequencyWouldCandidateReason != nullptr ? runtimeDiag->frequencyWouldCandidateReason : "none";
        report.source.frequencyMatch.liveFreqState = runtimeDiag->frequencyCandidateState != nullptr ? runtimeDiag->frequencyCandidateState : "none";
        report.source.frequencyMatch.liveFreqReady = runtimeDiag->frequencyReadyOk;
        report.source.frequencyMatch.liveFreqGate = runtimeDiag->frequencyGateOpen;
        report.source.frequencyMatch.liveFreqPresent = runtimeDiag->frequencyPresent;
        report.source.frequencyMatch.liveFreqValid = runtimeDiag->frequencyValidWindow;
        report.source.frequencyMatch.liveFreqMatch = runtimeDiag->frequencyMatched;
        report.source.frequencyMatch.analyzerMissReason = report.classification.result == AnalyzerResult::Miss
            ? analyzerReasonName(report.classification.reason)
            : "none";
        report.source.frequencyMatch.nearMiss = runtimeDiag->frequencyNearMiss;
        report.source.frequencyMatch.nearMissReason = runtimeDiag->frequencyNearMissReason != nullptr ? runtimeDiag->frequencyNearMissReason : "none";
    }

    report.frequencyDetector = frequencyDetector;

    if (frequencyDetector != nullptr) {
        report.source.frequencyMatch.sourceOccurrenceEmitted = frequencyDetector->candidateEmitted;
        report.source.frequencyMatch.runtimeEvidenceSeen = runtimeDiag != nullptr ? runtimeDiag->frequencyPresent : false;
        report.source.frequencyMatch.runtimeOccurrenceReceived = report.source.frequencyMatch.sourceOccurrenceEmitted && runtimeReceivedOccurrence;
        report.source.frequencyMatch.sourceLastRejectReason = runtimeDiag != nullptr && runtimeDiag->frequencyRejectReason != nullptr
            ? runtimeDiag->frequencyRejectReason
            : "none";
        report.source.frequencyMatch.selectedRejectReason = runtimeDiag != nullptr
            && !report.source.frequencyMatch.acceptedPresent
            && (
                runtimeDiag->frequencySelectedRejectCandidateId > 0
                || runtimeDiag->frequencyOpened
                || runtimeDiag->frequencyReleased
                || runtimeDiag->sourceSummary.rejectCount > 0
            )
            && runtimeDiag->frequencyNoEmitReason != nullptr
                ? runtimeDiag->frequencyNoEmitReason
                : "none";
        report.source.frequencyMatch.selectedRejectGateReason = runtimeDiag != nullptr && runtimeDiag->frequencyGateReason != nullptr
            ? runtimeDiag->frequencyGateReason
            : "none";
        report.source.frequencyMatch.fmOpened = runtimeDiag != nullptr ? runtimeDiag->frequencyOpened : false;
        report.source.frequencyMatch.fmReleased = runtimeDiag != nullptr ? runtimeDiag->frequencyReleased : false;
        report.source.frequencyMatch.fmEmitted = runtimeDiag != nullptr ? runtimeDiag->frequencyEmitted : false;
        report.source.frequencyMatch.fmDurationOk = runtimeDiag != nullptr ? runtimeDiag->frequencyDurationOk : false;
        report.source.frequencyMatch.fmValidRelease = runtimeDiag != nullptr ? runtimeDiag->frequencyValidRelease : false;
        report.source.frequencyMatch.fmEmitAllowed = runtimeDiag != nullptr ? runtimeDiag->frequencyEmitAllowed : false;
        report.source.frequencyMatch.acceptedCandidateId = runtimeDiag != nullptr ? runtimeDiag->frequencyAcceptedCandidateId : 0UL;
        report.source.frequencyMatch.selectedRejectCandidateId = runtimeDiag != nullptr ? runtimeDiag->frequencySelectedRejectCandidateId : 0UL;
        report.source.frequencyMatch.lastCandidateId = runtimeDiag != nullptr ? runtimeDiag->frequencyLastCandidateId : 0UL;
        report.source.frequencyMatch.lifecycleCandidateId = runtimeDiag != nullptr ? runtimeDiag->frequencyLifecycleCandidateId : 0UL;
        report.source.frequencyMatch.candidateLastMatchMs = runtimeDiag != nullptr ? runtimeDiag->frequencyLastMatchMs : 0UL;
        report.source.frequencyMatch.fmDurationUsedMs = runtimeDiag != nullptr ? runtimeDiag->frequencyDurationUsedMs : 0UL;
        report.source.frequencyMatch.fmDurationPrintedMs = runtimeDiag != nullptr ? runtimeDiag->frequencyDurationPrintedMs : 0UL;
        report.source.frequencyMatch.fmMinDurationUsedMs = runtimeDiag != nullptr ? runtimeDiag->frequencyMinDurationUsedMs : 0UL;
        report.source.frequencyMatch.fmMinDurationReportedMs = runtimeDiag != nullptr ? runtimeDiag->frequencyMinDurationReportedMs : 0UL;
        report.source.frequencyMatch.fmDurationInconsistent = runtimeDiag != nullptr ? runtimeDiag->frequencyDurationInconsistent : false;
        report.source.frequencyMatch.fmPrintedDurationInconsistent = runtimeDiag != nullptr ? runtimeDiag->frequencyPrintedDurationInconsistent : false;
        report.source.frequencyMatch.fmCloseCause = runtimeDiag != nullptr && runtimeDiag->sourceSummary.closeCause != nullptr
            ? runtimeDiag->sourceSummary.closeCause
            : "none";
        report.source.frequencyMatch.fmOpenMs = runtimeDiag != nullptr ? runtimeDiag->frequencyOpenMs : 0UL;
        report.source.frequencyMatch.fmPeakMs = runtimeDiag != nullptr ? runtimeDiag->frequencyPeakMs : 0UL;
        report.source.frequencyMatch.fmReleaseMs = runtimeDiag != nullptr ? runtimeDiag->frequencyReleaseMs : 0UL;
        report.source.frequencyMatch.fmDurationMs = runtimeDiag != nullptr ? runtimeDiag->frequencyDurationMs : 0UL;
        report.source.frequencyMatch.fmMinDurationMs = runtimeDiag != nullptr ? runtimeDiag->frequencyMinDurationMs : 0UL;
        report.source.frequencyMatch.fmMaxDurationMs = runtimeDiag != nullptr ? runtimeDiag->frequencyMaxDurationMs : 0UL;
        report.source.frequencyMatch.diagFirstFrameMs = report.source.frequencyMatch.fmOpenMs;
        report.source.frequencyMatch.diagLastFrameMs = report.source.frequencyMatch.fmReleaseMs;
        if (!hasCurrentSourceEvidence) {
            report.source.frequencyMatch.diagFirstFrameMs = 0;
            report.source.frequencyMatch.diagLastFrameMs = 0;
            report.source.frequencyMatch.fmOpenMs = 0;
            report.source.frequencyMatch.fmPeakMs = 0;
            report.source.frequencyMatch.fmReleaseMs = 0;
            report.source.frequencyMatch.fmDurationMs = 0;
            report.source.frequencyMatch.fmMinDurationMs = 0;
            report.source.frequencyMatch.fmMaxDurationMs = 0;
            report.source.frequencyMatch.fmDurationOk = false;
            report.source.frequencyMatch.fmOpened = false;
            report.source.frequencyMatch.fmReleased = false;
            report.source.frequencyMatch.fmEmitted = false;
            report.source.frequencyMatch.fmValidRelease = false;
            report.source.frequencyMatch.fmEmitAllowed = false;
            report.source.frequencyMatch.fmCloseCause = "none";
        }
        report.source.frequencyMatch.diagFrameCountOk = report.source.frequencyMatch.expectedFrameCountEstimate == 0
            ? report.source.frequencyMatch.frames == 0
            : report.source.frequencyMatch.frames > 0;
        report.source.frequencyMatch.detectionGateBlocked = !runtimeDiag->frequencyGateOpen || !runtimeDiag->frequencyReadyOk;
        if (!runtimeDiag->frequencyReadyOk) {
            report.source.frequencyMatch.detectionGateReason = "not_ready";
        } else if (!runtimeDiag->frequencyGateOpen) {
            report.source.frequencyMatch.detectionGateReason = report.source.frequencyMatch.selectedRejectGateReason != nullptr && report.source.frequencyMatch.selectedRejectGateReason[0] != '\0'
                ? report.source.frequencyMatch.selectedRejectGateReason
                : "unknown";
        } else {
            report.source.frequencyMatch.detectionGateReason = "none";
        }
    }
    if (frequencyDetectorReportAvailable) {
        report.source.frequencyMatch.scoreOkUpdates = frequencyDetail.aggregates.scoreOkCount;
        report.source.frequencyMatch.contrastOkUpdates = frequencyDetail.aggregates.contrastOkCount;
        report.source.frequencyMatch.bothOkUpdates = frequencyDetail.aggregates.bothOkCount;
        report.source.frequencyMatch.matchFrames = frequencyDetail.aggregates.matchCount;
        report.source.frequencyMatch.scoreThreshold = frequencyDetail.thresholds.scoreThreshold;
        report.source.frequencyMatch.contrastThreshold = frequencyDetail.thresholds.contrastThreshold;
        report.source.frequencyMatch.sourceOccurrenceEmitted = frequencyDetail.inspect.emitted;
        report.source.frequencyMatch.runtimeOccurrenceReceived = report.source.frequencyMatch.sourceOccurrenceEmitted && runtimeReceivedOccurrence;
        report.source.frequencyMatch.sourceLastRejectReason = analyzerTextOrFallback(frequencyDetail.inspect.rejectReason, "none");
        report.source.frequencyMatch.selectedRejectGateReason = analyzerTextOrFallback(frequencyDetail.inspect.gateReason, "none");
        report.source.frequencyMatch.fmOpened = frequencyDetail.inspect.opened;
        report.source.frequencyMatch.fmReleased = frequencyDetail.inspect.released;
        report.source.frequencyMatch.fmEmitted = frequencyDetail.inspect.emitted;
        report.source.frequencyMatch.fmValidRelease = frequencyDetail.inspect.validRelease;
        report.source.frequencyMatch.fmEmitAllowed = frequencyDetail.inspect.emitAllowed;
        report.source.frequencyMatch.fmOpenMs = frequencyDetail.inspect.openMs;
        report.source.frequencyMatch.fmPeakMs = frequencyDetail.inspect.peakMs;
        report.source.frequencyMatch.fmReleaseMs = frequencyDetail.inspect.releaseMs;
        report.source.frequencyMatch.fmDurationMs = frequencyDetail.inspect.durationMs;
        report.source.frequencyMatch.fmMinDurationMs = frequencyDetectorReport.thresholds.minDurationMs;
        report.source.frequencyMatch.fmMaxDurationMs = frequencyDetectorReport.thresholds.maxDurationMs;
        report.source.frequencyMatch.diagFirstFrameMs = report.source.frequencyMatch.fmOpenMs;
        report.source.frequencyMatch.diagLastFrameMs = report.source.frequencyMatch.fmReleased
            ? report.source.frequencyMatch.fmReleaseMs
            : report.source.frequencyMatch.fmPeakMs;
        report.source.frequencyMatch.liveFreqReason = analyzerTextOrFallback(frequencyDetail.inspect.rejectReason, "none");
        report.source.frequencyMatch.liveFreqState = analyzerTextOrFallback(frequencyDetail.inspect.candidateState, "none");
        report.source.frequencyMatch.liveFreqReady = frequencyDetail.inspect.readyOk;
        report.source.frequencyMatch.liveFreqGate = frequencyDetail.inspect.gateOpen;
        report.source.frequencyMatch.sourceSummary.present = report.source.frequencyMatch.sourceSummary.present || frequencySelectedReject.present;
        report.source.frequencyMatch.sourceSummary.candidateCount = frequencyDetectorReport.aggregates.rejectedCount;
        report.source.frequencyMatch.sourceSummary.rejectCount = frequencyDetectorReport.aggregates.rejectedCount;
        if (frequencySelectedReject.present) {
            report.source.frequencyMatch.selectedRejectReason = analyzerTextOrFallback(frequencySelectedReject.detectorReason, "none");
            report.source.frequencyMatch.sourceSummary.origin = "frequency_detector_report";
            report.source.frequencyMatch.sourceSummary.bestDurationMs = frequencySelectedReject.durationMs;
            report.source.frequencyMatch.sourceSummary.bestOpenMs = frequencySelectedReject.startMs;
            report.source.frequencyMatch.sourceSummary.bestPeakMs = frequencySelectedReject.peakMs;
            report.source.frequencyMatch.sourceSummary.bestLastMatchMs = frequencySelectedReject.endMs;
            report.source.frequencyMatch.sourceSummary.bestCloseMs = frequencySelectedReject.endMs;
            report.source.frequencyMatch.sourceSummary.bestPeakPrimary = frequencySelectedReject.strength;
            report.source.frequencyMatch.sourceSummary.bestPeakSecondary = frequencyDetail.selectedReject.contrast;
            report.source.frequencyMatch.sourceSummary.bestRejectReason = analyzerTextOrFallback(frequencySelectedReject.detectorReason, "none");
        }
        report.source.frequencyMatch.detectionGateBlocked = !frequencyDetail.inspect.gateOpen || !frequencyDetail.inspect.readyOk;
        if (!frequencyDetail.inspect.readyOk) {
            report.source.frequencyMatch.detectionGateReason = "not_ready";
        } else if (!frequencyDetail.inspect.gateOpen) {
            report.source.frequencyMatch.detectionGateReason =
                report.source.frequencyMatch.selectedRejectGateReason != nullptr && report.source.frequencyMatch.selectedRejectGateReason[0] != '\0'
                    ? report.source.frequencyMatch.selectedRejectGateReason
                    : "unknown";
        } else {
            report.source.frequencyMatch.detectionGateReason = "none";
        }

        const float targetScoreThreshold = report.source.frequencyMatch.scoreThreshold > 0.0f
            ? report.source.frequencyMatch.scoreThreshold
            : 0.0f;
        const float targetPartialThreshold = targetScoreThreshold > 0.0f
            ? targetScoreThreshold * 0.75f
            : 0.0f;
        const float targetNoiseFloor = targetScoreThreshold > 0.0f
            ? targetScoreThreshold * 0.15f
            : 0.0f;
        report.source.frequencyMatch.targetPresent = report.source.frequencyMatch.matchFrames > 0
            || report.source.frequencyMatch.fmOpened
            || report.source.frequencyMatch.fmEmitted;
        report.source.frequencyMatch.weakTarget = !report.source.frequencyMatch.targetPresent
            && report.source.frequencyMatch.maxScore > targetNoiseFloor
            && report.source.frequencyMatch.maxScore < targetPartialThreshold;
        const bool targetPartial = !report.source.frequencyMatch.targetPresent
            && report.source.frequencyMatch.maxScore >= targetPartialThreshold
            && report.source.frequencyMatch.maxScore < targetScoreThreshold;
        report.source.frequencyMatch.noTarget = !report.source.frequencyMatch.targetPresent
            && !targetPartial
            && !report.source.frequencyMatch.weakTarget;
        if (report.source.frequencyMatch.targetPresent) {
            report.source.frequencyMatch.targetEvidenceClass = "present";
        } else if (targetPartial) {
            report.source.frequencyMatch.targetEvidenceClass = "partial";
        } else if (report.source.frequencyMatch.weakTarget) {
            report.source.frequencyMatch.targetEvidenceClass = "weak";
        } else {
            report.source.frequencyMatch.targetEvidenceClass = "none";
        }

        hasCurrentSourceEvidence = report.source.frequencyMatch.acceptedPresent || report.source.frequencyMatch.sourceSummary.present;
    }
    report.source.frequencyMatch.inconsistent = report.classification.result == AnalyzerResult::Miss && report.source.frequencyMatch.acceptedPresent;
    if (report.source.frequencyMatch.analyzerMissReason == nullptr || report.source.frequencyMatch.analyzerMissReason[0] == '\0') {
        report.source.frequencyMatch.analyzerMissReason = report.classification.result == AnalyzerResult::Miss
            ? "no_accepted_occurrence"
            : "none";
    }
    if (report.classification.result == AnalyzerResult::Miss && !report.source.frequencyMatch.acceptedPresent && report.source.frequencyMatch.analyzerMissReason != nullptr && strcmp(report.source.frequencyMatch.analyzerMissReason, "occurrence_emitted") == 0) {
        report.source.frequencyMatch.analyzerMissReason = "unknown_or_stale_reason";
        report.source.frequencyMatch.inconsistent = true;
    }
    report.source.frequencyMatch.analyzerSeenOccurrence = report.source.frequencyMatch.acceptedPresent;
    report.frequency = report.source.frequencyMatch;
    report.source.frequencyMatch.freqEvidenceClass = frequencyEvidenceClassLabel(classifyFrequencyEvidence(report));
    report.frequency = report.source.frequencyMatch;
    if (!report.source.frequencyMatch.sourceOccurrenceEmitted) {
        report.source.frequencyMatch.runtimeOccurrenceReceived = false;
    }

    const bool scalarProfile = selectedProfile.detectorSelection == detection::DetectorSelection::ScalarTransient;
    if (scalarProfile) {
        const auto& scalarAccepted = scalarDetectorReport.accepted;
        const auto& scalarAcceptedDetail = scalarDetectorReport.scalar.accepted;
        const auto& scalarDetail = scalarDetectorReport.scalar.inspect;
        const auto& scalarSelectedReject = scalarDetectorReport.selectedReject;
        const char* expectedScalarSource = analyzerExpectedScalarOccurrenceSource(selectedProfile);
        const bool scalarAcceptedPresent = report.occurrences.present
            && report.occurrences.valid
            && report.primaryPattern.accepted
            && report.occurrences.primarySource != nullptr
            && strcmp(report.occurrences.primarySource, expectedScalarSource) == 0;
        const bool scalarSelectedRejectPresent = scalarDetectorReportAvailable
            ? scalarDetectorReport.selectedReject.present
            : false;

        report.source.scalarTransient.currentTrialId = report.context.trial;
        report.source.scalarTransient.windowStartMs = _sequenceTest.currentTrialStartMs;
        report.source.scalarTransient.windowEndMs = _sequenceTest.currentTrialEndMs;
        report.source.scalarTransient.expectedWindowMs = report.source.scalarTransient.windowEndMs >= report.source.scalarTransient.windowStartMs
            ? report.source.scalarTransient.windowEndMs - report.source.scalarTransient.windowStartMs
            : 0UL;
        report.source.scalarTransient.expectedFrameCountEstimate =
            static_cast<unsigned long>((report.source.scalarTransient.expectedWindowMs
                * static_cast<unsigned long>(_audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL)) / 1000UL);
        report.source.scalarTransient.diagFrameCountOk = report.source.scalarTransient.expectedWindowMs > 0 && report.source.scalarTransient.expectedFrameCountEstimate > 0;

        report.source.scalarTransient.acceptedPresent = scalarAcceptedPresent;
        report.source.scalarTransient.acceptedTrialId = report.source.scalarTransient.acceptedPresent ? report.context.trial : 0UL;
        report.source.scalarTransient.acceptedSource = report.source.scalarTransient.acceptedPresent
            ? (report.occurrences.primarySource != nullptr ? report.occurrences.primarySource : "unknown")
            : "none";
        report.source.scalarTransient.acceptedDtMs = report.source.scalarTransient.acceptedPresent
            ? (scalarDetectorReportAvailable
                ? static_cast<long>(scalarAccepted.startMs) - static_cast<long>(_sequenceTest.currentTrialScheduledAtMs)
                : report.occurrences.primaryDtMs)
            : -1;
        report.source.scalarTransient.acceptedStartMs = report.source.scalarTransient.acceptedPresent
            ? (scalarDetectorReportAvailable ? scalarAccepted.startMs : report.occurrences.startMs)
            : 0UL;
        report.source.scalarTransient.acceptedPeakMs = report.source.scalarTransient.acceptedPresent
            ? (scalarDetectorReportAvailable ? scalarAccepted.peakMs : report.occurrences.peakMs)
            : 0UL;
        report.source.scalarTransient.acceptedReleaseMs = report.source.scalarTransient.acceptedPresent
            ? (scalarDetectorReportAvailable ? scalarAccepted.endMs : report.occurrences.releaseMs)
            : 0UL;
        report.source.scalarTransient.acceptedDurationMs = report.source.scalarTransient.acceptedPresent
            ? (scalarDetectorReportAvailable ? scalarAccepted.durationMs : report.occurrences.primaryDurationMs)
            : 0UL;
        report.source.scalarTransient.acceptedStrength = report.source.scalarTransient.acceptedPresent
            ? (scalarDetectorReportAvailable ? scalarAccepted.strength : report.occurrences.primaryStrength)
            : 0.0f;
        report.source.scalarTransient.acceptedScore = report.source.scalarTransient.acceptedPresent
            ? (scalarDetectorReportAvailable ? scalarAcceptedDetail.value : report.occurrences.score)
            : 0.0f;
        report.source.scalarTransient.acceptedContrast = report.source.scalarTransient.acceptedPresent
            ? (scalarDetectorReportAvailable ? 0.0f : report.occurrences.contrast)
            : 0.0f;

        if (scalarDetectorReportAvailable) {
            report.source.scalarTransient.scalarRejectReason = analyzerTextOrFallback(scalarDetail.rejectReason, "unknown");
            report.source.scalarTransient.scalarNoEmitReason = analyzerTextOrFallback(scalarDetail.noEmitReason, "none");
            report.source.scalarTransient.scalarGateReason = analyzerTextOrFallback(scalarDetail.gateReason, "none");
            report.source.scalarTransient.scalarOpened = scalarDetail.opened;
            report.source.scalarTransient.scalarReleased = scalarDetail.released;
            report.source.scalarTransient.scalarValidRelease = scalarDetail.validRelease;
            report.source.scalarTransient.scalarEmitAllowed = scalarDetail.emitAllowed;
            report.source.scalarTransient.scalarOpenMs = scalarDetail.openMs;
            report.source.scalarTransient.scalarPeakMs = scalarDetail.peakMs;
            report.source.scalarTransient.scalarReleaseMs = scalarDetail.releaseMs;
            report.source.scalarTransient.scalarDurationMs = scalarDetail.durationMs;
            report.source.scalarTransient.scalarMinDurationMs = scalarDetectorReport.thresholds.minDurationMs;
            report.source.scalarTransient.scalarMaxDurationMs = scalarDetectorReport.thresholds.maxDurationMs;
            report.source.scalarTransient.scalarPeakStrength = scalarDetail.peakStrength;
        } else if (runtimeDiag != nullptr) {
            report.source.scalarTransient.scalarRejectReason = analyzerTextOrFallback(runtimeDiag->scalarRejectReason, "unknown");
            report.source.scalarTransient.scalarNoEmitReason = analyzerTextOrFallback(runtimeDiag->scalarNoEmitReason, "none");
            report.source.scalarTransient.scalarGateReason = analyzerTextOrFallback(runtimeDiag->scalarGateReason, "none");
            report.source.scalarTransient.scalarOpened = runtimeDiag->scalarOpened;
            report.source.scalarTransient.scalarReleased = runtimeDiag->scalarReleased;
            report.source.scalarTransient.scalarValidRelease = runtimeDiag->scalarValidRelease;
            report.source.scalarTransient.scalarEmitAllowed = runtimeDiag->scalarEmitAllowed;
            report.source.scalarTransient.scalarOpenMs = runtimeDiag->scalarOpenMs;
            report.source.scalarTransient.scalarPeakMs = runtimeDiag->scalarPeakMs;
            report.source.scalarTransient.scalarReleaseMs = runtimeDiag->scalarReleaseMs;
            report.source.scalarTransient.scalarDurationMs = runtimeDiag->scalarDurationMs;
            report.source.scalarTransient.scalarMinDurationMs = runtimeDiag->scalarMinDurationMs;
            report.source.scalarTransient.scalarMaxDurationMs = runtimeDiag->scalarMaxDurationMs;
            report.source.scalarTransient.scalarPeakStrength = runtimeDiag->scalarPeakStrength;
        }

        report.source.scalarTransient.sourceOccurrenceEmitted = report.source.scalarTransient.acceptedPresent;
        report.source.scalarTransient.sourceSummary.present = !report.source.scalarTransient.acceptedPresent
            && (scalarSelectedRejectPresent
                || report.source.scalarTransient.scalarOpened
                || report.source.scalarTransient.scalarReleased
                || !analyzerTextIsNone(report.source.scalarTransient.scalarRejectReason));
        report.source.scalarTransient.sourceSummary.origin = scalarSelectedRejectPresent
            ? "scalar_detector_report"
            : "synthesized_scalar_lifecycle";
        report.source.scalarTransient.sourceSummary.candidateCount = report.source.scalarTransient.sourceSummary.present ? 1UL : 0UL;
        report.source.scalarTransient.sourceSummary.rejectCount = report.source.scalarTransient.sourceSummary.candidateCount;
        report.source.scalarTransient.sourceSummary.bestDurationMs = scalarSelectedRejectPresent
            ? scalarSelectedReject.durationMs
            : report.source.scalarTransient.scalarDurationMs;
        report.source.scalarTransient.sourceSummary.secondBestDurationMs = 0UL;
        report.source.scalarTransient.sourceSummary.bestOpenMs = scalarSelectedRejectPresent
            ? scalarSelectedReject.startMs
            : report.source.scalarTransient.scalarOpenMs;
        report.source.scalarTransient.sourceSummary.bestPeakMs = scalarSelectedRejectPresent
            ? scalarSelectedReject.peakMs
            : report.source.scalarTransient.scalarPeakMs;
        report.source.scalarTransient.sourceSummary.bestLastMatchMs = scalarSelectedRejectPresent
            ? scalarSelectedReject.endMs
            : report.source.scalarTransient.scalarReleaseMs;
        report.source.scalarTransient.sourceSummary.bestCloseMs = scalarSelectedRejectPresent
            ? scalarSelectedReject.endMs
            : report.source.scalarTransient.scalarReleaseMs;
        report.source.scalarTransient.sourceSummary.bestPeakPrimary = scalarSelectedRejectPresent
            ? scalarSelectedReject.strength
            : report.source.scalarTransient.scalarPeakStrength;
        report.source.scalarTransient.sourceSummary.bestPeakSecondary = 0.0f;
        report.source.scalarTransient.sourceSummary.bestRejectReason = scalarSelectedRejectPresent
            ? analyzerTextOrFallback(scalarSelectedReject.detectorReason, "none")
            : analyzerTextOrFallback(report.source.scalarTransient.scalarRejectReason, "none");
        report.source.scalarTransient.sourceSummary.bestGateReason = runtimeDiag != nullptr && runtimeDiag->sourceSummary.bestGateReason != nullptr
            ? runtimeDiag->sourceSummary.bestGateReason
            : analyzerTextOrFallback(report.source.scalarTransient.scalarGateReason, "none");
        report.source.scalarTransient.sourceSummary.scoreTooLowFrames = 0;
        report.source.scalarTransient.sourceSummary.contrastTooLowFrames = 0;
        report.source.scalarTransient.sourceSummary.scoreAndContrastTooLowFrames = 0;
        report.source.scalarTransient.sourceSummary.maxPeakPrimary = runtimeDiag != nullptr
            ? runtimeDiag->sourceSummary.maxPeakPrimary
            : report.source.scalarTransient.sourceSummary.bestPeakPrimary;
        report.source.scalarTransient.sourceSummary.maxPeakPrimaryMs = runtimeDiag != nullptr
            ? runtimeDiag->sourceSummary.maxPeakPrimaryMs
            : report.source.scalarTransient.sourceSummary.bestPeakMs;
        report.source.scalarTransient.sourceSummary.maxPeakSecondary = 0.0f;
        report.source.scalarTransient.sourceSummary.maxPeakSecondaryMs = 0UL;
        report.source.scalarTransient.sourceSummary.totalMatchMs = report.source.scalarTransient.sourceSummary.bestDurationMs;
        report.source.scalarTransient.sourceSummary.totalGapMs = runtimeDiag != nullptr ? runtimeDiag->sourceSummary.totalGapMs : 0UL;
        report.source.scalarTransient.sourceSummary.maxGapMs = runtimeDiag != nullptr ? runtimeDiag->sourceSummary.maxGapMs : 0UL;
        report.source.scalarTransient.sourceSummary.islandCount = report.source.scalarTransient.sourceSummary.present ? 1UL : 0UL;
        report.source.scalarTransient.sourceLastCandidate.present = scalarSelectedRejectPresent
            || report.source.scalarTransient.scalarOpened
            || report.source.scalarTransient.scalarReleased
            || report.source.scalarTransient.scalarEmitAllowed;
        report.source.scalarTransient.sourceLastCandidate.peakMs = scalarSelectedRejectPresent
            ? scalarSelectedReject.peakMs
            : report.source.scalarTransient.scalarPeakMs;
        report.source.scalarTransient.sourceLastCandidate.durationMs = scalarSelectedRejectPresent
            ? scalarSelectedReject.durationMs
            : report.source.scalarTransient.scalarDurationMs;
        report.source.scalarTransient.sourceLastCandidate.sampleCount = 0UL;
        report.source.scalarTransient.sourceLastCandidate.peakPrimary = scalarSelectedRejectPresent
            ? scalarSelectedReject.strength
            : report.source.scalarTransient.scalarPeakStrength;
        report.source.scalarTransient.sourceLastCandidate.peakSecondary = 0.0f;
        report.source.scalarTransient.sourceLastCandidate.reason = scalarSelectedRejectPresent
            ? analyzerTextOrFallback(scalarSelectedReject.detectorReason, "none")
            : analyzerTextOrFallback(report.source.scalarTransient.scalarRejectReason, "none");
        report.source.scalarTransient.sourceLastCandidate.gateReason = runtimeDiag != nullptr && runtimeDiag->sourceLastCandidate.gateReason != nullptr
            ? runtimeDiag->sourceLastCandidate.gateReason
            : analyzerTextOrFallback(report.source.scalarTransient.scalarGateReason, "none");
        report.source.scalarTransient.sourceLastCandidate.scope = analyzerScopeFromPeakMs(
            report.source.scalarTransient.sourceLastCandidate.present,
            report.source.scalarTransient.sourceLastCandidate.peakMs,
            report.source.scalarTransient.windowStartMs,
            report.source.scalarTransient.windowEndMs
        );
        report.source.scalarTransient.runtimeEvidenceSeen = report.source.scalarTransient.acceptedPresent
            || scalarSelectedRejectPresent
            || report.source.scalarTransient.scalarOpened
            || report.source.scalarTransient.scalarReleased
            || !analyzerTextIsNone(report.source.scalarTransient.scalarRejectReason);
        report.source.scalarTransient.runtimeOccurrenceReceived = report.source.scalarTransient.sourceOccurrenceEmitted;
        report.source.scalarTransient.analyzerSeenOccurrence = report.source.scalarTransient.acceptedPresent;
        report.source.scalarTransient.liveScalarReason = analyzerTextOrFallback(report.source.scalarTransient.scalarRejectReason, "none");
        report.source.scalarTransient.liveScalarWould = analyzerTextOrFallback(report.source.scalarTransient.scalarNoEmitReason, "none");
        report.source.scalarTransient.liveScalarReady = report.source.scalarTransient.scalarOpened;
        report.source.scalarTransient.liveScalarGate = report.source.scalarTransient.scalarEmitAllowed;
        report.source.scalarTransient.liveScalarPresent = report.occurrences.present;
        report.source.scalarTransient.liveScalarValid = report.occurrences.valid;
        report.source.scalarTransient.liveScalarMatch = report.primaryPattern.accepted;
        report.source.scalarTransient.liveScalarState = report.source.scalarTransient.scalarOpened
            ? (report.source.scalarTransient.scalarReleased ? "released" : "active")
            : "idle";
        report.source.scalarTransient.detectionGateBlocked = !report.source.scalarTransient.acceptedPresent
            && (scalarSelectedRejectPresent
                || report.source.scalarTransient.scalarOpened
                || report.source.scalarTransient.scalarReleased
                || !analyzerTextIsNone(report.source.scalarTransient.scalarRejectReason));
        if (!report.source.scalarTransient.acceptedPresent) {
            if (!analyzerTextIsNone(report.source.scalarTransient.scalarRejectReason)) {
                report.source.scalarTransient.detectionGateReason = report.source.scalarTransient.scalarRejectReason;
            } else if (report.source.scalarTransient.scalarOpened && !report.source.scalarTransient.scalarReleased) {
                report.source.scalarTransient.detectionGateReason = "opened_not_released";
            } else if (!report.source.scalarTransient.scalarOpened) {
                report.source.scalarTransient.detectionGateReason = "no_evidence";
            } else {
                report.source.scalarTransient.detectionGateReason = "none";
            }
        } else {
            report.source.scalarTransient.detectionGateReason = "none";
        }

        if (report.source.scalarTransient.acceptedPresent) {
            report.source.scalarTransient.scalarRejectReason = "none";
            report.source.scalarTransient.scalarNoEmitReason = "none";
            report.source.scalarTransient.scalarGateReason = "none";
            report.source.scalarTransient.scalarOpened = true;
            report.source.scalarTransient.scalarReleased = true;
            report.source.scalarTransient.scalarValidRelease = true;
            report.source.scalarTransient.scalarEmitAllowed = true;
            report.source.scalarTransient.scalarOpenMs = report.source.scalarTransient.acceptedStartMs;
            report.source.scalarTransient.scalarPeakMs = report.source.scalarTransient.acceptedPeakMs;
            report.source.scalarTransient.scalarReleaseMs = report.source.scalarTransient.acceptedReleaseMs;
            report.source.scalarTransient.scalarDurationMs = report.source.scalarTransient.acceptedDurationMs;
            report.source.scalarTransient.sourceOccurrenceEmitted = true;
            report.source.scalarTransient.runtimeEvidenceSeen = true;
            report.source.scalarTransient.runtimeOccurrenceReceived = true;
            report.source.scalarTransient.analyzerSeenOccurrence = true;
            report.source.scalarTransient.liveScalarReason = "none";
            report.source.scalarTransient.liveScalarWould = "none";
            report.source.scalarTransient.liveScalarReady = true;
            report.source.scalarTransient.liveScalarGate = true;
            report.source.scalarTransient.liveScalarPresent = true;
            report.source.scalarTransient.liveScalarValid = true;
            report.source.scalarTransient.liveScalarMatch = true;
            report.source.scalarTransient.liveScalarState = "released";
            report.source.scalarTransient.detectionGateBlocked = false;
            report.source.scalarTransient.detectionGateReason = "none";
        }

        report.source.scalarTransient.inconsistent = report.classification.result == AnalyzerResult::Miss && report.source.scalarTransient.acceptedPresent;
    }

    const bool frequencySource = selectedProfile.detectorSelection == detection::DetectorSelection::FrequencyMatch;
    report.source.sourceKind = frequencySource ? "frequency_match" : "scalar_transient";
    report.source.sourceName = detection::detectorSelectionName(selectedProfile.detectorSelection);
    if (frequencySource) {
        report.source.acceptedPresent = report.source.frequencyMatch.acceptedPresent;
        report.source.sourceOccurrenceEmitted = report.source.frequencyMatch.sourceOccurrenceEmitted;
        report.source.runtimeEvidenceSeen = report.source.frequencyMatch.runtimeEvidenceSeen;
        report.source.runtimeOccurrenceReceived = report.source.frequencyMatch.runtimeOccurrenceReceived;
        report.source.analyzerSeen = report.source.frequencyMatch.analyzerSeenOccurrence;
        report.source.detectionGateBlocked = report.source.frequencyMatch.detectionGateBlocked;
        report.source.detectionGateReason = report.source.frequencyMatch.detectionGateReason;
        report.source.sourceSummary = report.source.frequencyMatch.sourceSummary;
        report.source.lastCandidate = report.source.frequencyMatch.sourceLastCandidate;
        report.source.activeAtTrialStart = report.source.frequencyMatch.fmOpened;
        report.source.activeAtTrialEnd = report.source.frequencyMatch.fmReleased;
        report.source.openedThisTrial = report.source.frequencyMatch.fmOpened;
        report.source.closedThisTrial = report.source.frequencyMatch.fmReleased;
        report.source.emittedThisTrial = report.source.frequencyMatch.fmEmitted;
        report.source.rejectedThisTrial = report.source.frequencyMatch.sourceSummary.present && !report.source.frequencyMatch.acceptedPresent;
    } else {
        report.source.acceptedPresent = report.source.scalarTransient.acceptedPresent;
        report.source.sourceOccurrenceEmitted = report.source.scalarTransient.sourceOccurrenceEmitted;
        report.source.runtimeEvidenceSeen = report.source.scalarTransient.runtimeEvidenceSeen;
        report.source.runtimeOccurrenceReceived = report.source.scalarTransient.runtimeOccurrenceReceived;
        report.source.analyzerSeen = report.source.scalarTransient.analyzerSeenOccurrence;
        report.source.detectionGateBlocked = report.source.scalarTransient.detectionGateBlocked;
        report.source.detectionGateReason = report.source.scalarTransient.detectionGateReason;
        report.source.sourceSummary = report.source.scalarTransient.sourceSummary;
        report.source.lastCandidate = report.source.scalarTransient.sourceLastCandidate;
        report.source.activeAtTrialStart = report.source.scalarTransient.scalarOpened;
        report.source.activeAtTrialEnd = report.source.scalarTransient.scalarReleased;
        report.source.openedThisTrial = report.source.scalarTransient.scalarOpened;
        report.source.closedThisTrial = report.source.scalarTransient.scalarReleased;
        report.source.emittedThisTrial = report.source.scalarTransient.scalarEmitted;
        report.source.rejectedThisTrial = report.source.scalarTransient.sourceSummary.present && !report.source.scalarTransient.acceptedPresent;
    }

    report.scalar = report.source.scalarTransient;

}










