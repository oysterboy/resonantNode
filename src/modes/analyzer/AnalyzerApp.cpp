#include "AnalyzerApp.h"

#include <Arduino.h>
#include <esp_system.h>
#include <stdlib.h>
#include <string.h>
#include <new>

#include "../../RuntimeDefaults.h"
#include "../../AudioDebugConfig.h"
#include "../../TimingUtils.h"
#include "AnalyzerTextUtils.h"
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

void printHeapStatus(const char* when) {
    const uint32_t free8 = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_8BIT));
    const uint32_t largest8 = static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    const uint32_t freeInternal = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    Serial.print("HEAP_STATUS when=");
    Serial.print(when != nullptr ? when : "unknown");
    Serial.print(" free_8bit=");
    Serial.print(free8);
    Serial.print(" largest_8bit=");
    Serial.print(largest8);
    Serial.print(" free_internal=");
    Serial.println(freeInternal);
}

void printRuntimeSize() {
    Serial.print("RUNTIME_SIZE detection_runtime_bytes=");
    Serial.println(static_cast<unsigned long>(sizeof(detection::DetectionRuntime)));
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

const char* rawAudioHealthNameFromCounters(unsigned long clipFrames,
                                           unsigned long rawFrames,
                                           unsigned long rawMaxAbs,
                                           float rawDcMean,
                                           float rawMeanAbs,
                                           int rawMin,
                                           int rawMax) {
    if (clipFrames > 0) {
        return "clipping";
    }
    if (rawFrames >= kAudioFlatlineStreakFrames && rawMaxAbs <= kRawFlatlineMaxAbsThreshold) {
        return "flatline";
    }
    const unsigned long rawRange = rawMax >= rawMin ? static_cast<unsigned long>(rawMax - rawMin) : 0UL;
    const float dcMagnitude = rawDcMean >= 0.0f ? rawDcMean : -rawDcMean;
    if (rawRange <= kRawDcStuckRangeThreshold && dcMagnitude > 64.0f && rawMeanAbs <= static_cast<float>(kRawDcStuckMeanAbsThreshold)) {
        return "dc_stuck";
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

void AnalyzerApp::updateSequenceAudioHealth(const AudioSamplePacket& frame) {
    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0 || _sequenceTest.currentTrialFinalized) {
        return;
    }

    auto& diagnostics = _sequenceTest.currentTrialDiagnostics;
    const long centeredSample = static_cast<long>(frame.centeredAudioValue);
    const long rawSample = static_cast<long>(frame.rawAudioValue);
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
    diagnostics.rawSum += static_cast<int32_t>(frame.rawAudioValue);
    diagnostics.rawAbsSum += static_cast<uint32_t>(absCentered);
    if (diagnostics.rawFrames == 1) {
        diagnostics.rawMin = frame.rawAudioValue;
        diagnostics.rawMax = frame.rawAudioValue;
    } else {
        if (frame.rawAudioValue < diagnostics.rawMin) {
            diagnostics.rawMin = frame.rawAudioValue;
        }
        if (frame.rawAudioValue > diagnostics.rawMax) {
            diagnostics.rawMax = frame.rawAudioValue;
        }
    }
    if (absCentered > diagnostics.rawMaxAbs) {
        diagnostics.rawMaxAbs = absCentered;
    }
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

void AnalyzerApp::printSystemHealth(const AnalyzerReport& report) const {
    const AudioSourceStats& sourceStats = _audioSource.stats();
    const AudioSignalStats& signalStats = _audioSignal.stats();
    const unsigned long sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
    const bool compactHealth = _sequenceTest.outputConfig.verbosity == 0U;
    const bool hasTrialWindow = _sequenceTest.active
        && _sequenceTest.currentTrial > 0
        && _sequenceTest.currentTrialEndMs >= _sequenceTest.currentTrialStartMs;
    const unsigned long windowStartMs = hasTrialWindow
        ? _sequenceTest.currentTrialStartMs
        : activeRunStartMs();
    const unsigned long windowEndMs = hasTrialWindow
        ? _sequenceTest.currentTrialEndMs
        : activeRunEndMs();
    const unsigned long windowElapsedMs = windowStartMs > 0 && windowEndMs >= windowStartMs ? windowEndMs - windowStartMs : 0UL;
    const unsigned long expectedSamples = static_cast<unsigned long>(
        (static_cast<unsigned long long>(windowElapsedMs) * static_cast<unsigned long long>(sampleRateHz)) / 1000ULL);
    const unsigned long processedSamples = hasTrialWindow
        ? _sequenceTest.currentTrialSamplesProcessed
        : (_sequenceTest.active ? _sequenceTest.samplesProcessed
            : static_cast<unsigned long>(signalStats.samplesProcessed));
    const float processedRatio = expectedSamples > 0
        ? static_cast<float>(processedSamples) / static_cast<float>(expectedSamples)
        : 0.0f;
    const unsigned long loopAvgUs = _loopHealth.count > 0 ? static_cast<unsigned long>(_loopHealth.sumUs / _loopHealth.count) : 0UL;
    const unsigned long updateLoopAvgUs = _sequenceTest.updateLoopCount > 0
        ? static_cast<unsigned long>(_sequenceTest.totalUpdateLoopUs / _sequenceTest.updateLoopCount)
        : 0UL;

    Serial.print("SYSTEM_HEALTH");
    Serial.print(" loop_avg_us=");
    Serial.print(loopAvgUs);
    Serial.print(" update_loop_avg_us=");
    Serial.print(updateLoopAvgUs);
    if (!compactHealth) {
        Serial.print(" trial=");
        Serial.print(report.context.trial);
        Serial.print(" t_ms=");
        Serial.print(report.context.timestampMs);
        Serial.print(" uptime_ms=");
        Serial.print(millis());
        Serial.print(" boot_count=");
        Serial.print(g_analyzerBootCount);
        Serial.print(" reset_reason=");
        Serial.print(systemResetReasonName(esp_reset_reason()));
        Serial.print(" window_start_ms=");
        Serial.print(windowStartMs);
        Serial.print(" window_end_ms=");
        Serial.print(windowEndMs);
        Serial.print(" window_elapsed_ms=");
        Serial.print(windowElapsedMs);
        Serial.print(" free_heap=");
        Serial.print(esp_get_free_heap_size());
        Serial.print(" loop_stats_window_ms=");
        Serial.print(windowElapsedMs);
        Serial.print(" loop_stats_scope=");
        Serial.print(_sequenceTest.active && _sequenceTest.currentTrialStartMs > 0 ? "trial_window" : "run_window");
        Serial.print(" loop_count=");
        Serial.print(_loopHealth.count);
        Serial.print(" loop_max_window_us=");
        Serial.print(_loopHealth.maxUs);
        Serial.print(" loop_over_5ms=");
        Serial.print(_loopHealth.over5ms);
        Serial.print(" loop_over_20ms=");
        Serial.print(_loopHealth.over20ms);
        Serial.print(" loop_max_since_boot_us=");
        Serial.print(_loopMaxSinceBootUs);
        Serial.print(" update_loop_count=");
        Serial.print(_sequenceTest.updateLoopCount);
        Serial.print(" update_loop_max_us=");
        Serial.print(_sequenceTest.currentTrialUpdateLoopMaxUs);
        Serial.print(" sample_work_max_us=");
        Serial.print(_sequenceTest.maxSampleWorkUs);
        Serial.print(" trial_finalize_max_us=");
        Serial.print(_sequenceTest.maxFinalizeTrialUs);
    }
    Serial.println();
    _loopHealth.reset();

    Serial.print("AUDIO_IO_HEALTH");
    Serial.print(" processed_ratio=");
    Serial.print(processedRatio, 3);
    if (!compactHealth) {
        Serial.print(" t_ms=");
        Serial.print(report.context.timestampMs);
        Serial.print(" i2s_reads=");
        Serial.print(sourceStats.reads);
        Serial.print(" i2s_bytes_read=");
        Serial.print(sourceStats.readBytes);
        Serial.print(" processed_samples=");
        Serial.print(processedSamples);
        Serial.print(" expected_samples=");
        Serial.print(expectedSamples);
        Serial.print(" frames_emitted=");
        Serial.print(signalStats.blocksProcessed);
        Serial.print(" partial_frames=");
        Serial.print(sourceStats.shortReads);
        Serial.print(" duplicate_frames=0");
        Serial.print(" gap_samples=");
        Serial.print(sourceStats.droppedBlockCount);
        Serial.print(" overlap_samples=0");
        Serial.print(" max_block_age_ms=");
        Serial.print(_sequenceTest.active ? _sequenceTest.maxBlockAgeMs : 0UL);
        Serial.print(" max_processing_lag_ms=");
        Serial.println(_sequenceTest.active ? _sequenceTest.maxProcessingLagMs : 0UL);
    } else {
        Serial.println();
    }

    const auto& rawDiagnostics = _sequenceTest.currentTrialDiagnostics;
    const float rawDcMean = rawDiagnostics.rawFrames > 0
        ? static_cast<float>(rawDiagnostics.rawSum) / static_cast<float>(rawDiagnostics.rawFrames)
        : 0.0f;
    const float rawMeanAbs = rawDiagnostics.rawFrames > 0
        ? static_cast<float>(rawDiagnostics.rawAbsSum) / static_cast<float>(rawDiagnostics.rawFrames)
        : 0.0f;
    const float rawZeroRatio = rawDiagnostics.rawFrames > 0
        ? static_cast<float>(_sequenceTest.currentTrialDiagnostics.audioZeroishFrames) / static_cast<float>(rawDiagnostics.rawFrames)
        : 0.0f;
    const char* rawHealth = rawAudioHealthNameFromCounters(
        _sequenceTest.currentTrialDiagnostics.audioRmsTooHighFrames,
        rawDiagnostics.rawFrames,
        rawDiagnostics.rawMaxAbs,
        rawDcMean,
        rawMeanAbs,
        rawDiagnostics.rawMin,
        rawDiagnostics.rawMax
    );
    if (_sequenceTest.outputConfig.verbosity >= 1U) {
        Serial.print("RAW_AUDIO_HEALTH");
        Serial.print(" verdict=");
        Serial.print(rawHealth != nullptr ? rawHealth : "unknown");
    }
    if (_sequenceTest.outputConfig.verbosity >= 2U) {
        Serial.print(" t_ms=");
        Serial.print(report.context.timestampMs);
        Serial.print(" frames=");
        Serial.print(rawDiagnostics.rawFrames);
        Serial.print(" min_sample=");
        Serial.print(rawDiagnostics.rawFrames > 0 ? rawDiagnostics.rawMin : 0);
        Serial.print(" max_sample=");
        Serial.print(rawDiagnostics.rawFrames > 0 ? rawDiagnostics.rawMax : 0);
        Serial.print(" dc_mean=");
        Serial.print(rawDcMean, 1);
        Serial.print(" mean_abs=");
        Serial.print(rawMeanAbs, 1);
        Serial.print(" max_abs=");
        Serial.print(rawDiagnostics.rawMaxAbs);
        Serial.print(" zero_ratio=");
        Serial.print(rawZeroRatio, 3);
        Serial.print(" clip_count=");
        Serial.print(_sequenceTest.currentTrialDiagnostics.audioRmsTooHighFrames);
        Serial.print(" flatline=");
        Serial.println(rawHealth != nullptr && strcmp(rawHealth, "flatline") == 0 ? 1 : 0);
    } else if (_sequenceTest.outputConfig.verbosity >= 1U) {
        Serial.println();
    }
}

void buildFrequencyFailReason(const detection::FrequencyFeatureFrame& evidence,
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
        case AnalyzerApp::SeqOutputMode::Compact:
            return "compact";
        case AnalyzerApp::SeqOutputMode::SignalCheck:
            return "signalcheck";
        case AnalyzerApp::SeqOutputMode::Full:
            return "full";
        case AnalyzerApp::SeqOutputMode::System:
            return "system";
        case AnalyzerApp::SeqOutputMode::Source:
            return "source";
        case AnalyzerApp::SeqOutputMode::Inspect:
            return "inspect";
        case AnalyzerApp::SeqOutputMode::Pattern:
            return "pattern";
        case AnalyzerApp::SeqOutputMode::Explain:
            return "dump";
        default:
            return "compact";
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
        return AnalyzerApp::SeqOutputMode::Compact;
    }
    if (equalsIgnoreCase(token, "compact") || equalsIgnoreCase(token, "trial")) {
        return AnalyzerApp::SeqOutputMode::Compact;
    }
    if (equalsIgnoreCase(token, "signalcheck")) {
        return AnalyzerApp::SeqOutputMode::SignalCheck;
    }
    if (equalsIgnoreCase(token, "full")) {
        return AnalyzerApp::SeqOutputMode::Full;
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
    if (equalsIgnoreCase(token, "pattern")) {
        return AnalyzerApp::SeqOutputMode::Pattern;
    }
    if (equalsIgnoreCase(token, "dump")) {
        return AnalyzerApp::SeqOutputMode::Explain;
    }
    if (equalsIgnoreCase(token, "quiet")) {
        return AnalyzerApp::SeqOutputMode::Quiet;
    }
    if (valid != nullptr) {
        *valid = false;
    }
    return AnalyzerApp::SeqOutputMode::Compact;
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
    if (_detection == nullptr) {
        _detection = new (std::nothrow) detection::DetectionRuntime();
    }
    if (_detection == nullptr) {
        printHeapStatus("begin_runtime_alloc_failed");
        Serial.println("ERR MEMERROR reason=detection_runtime_alloc_failed");
        return;
    }
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
    Serial.println("EVT analyzer_help type='HELP', 'BASE', 'PARAM freqScore=10000 freqContrast=50.0 freqReleaseScore=8000 freqReleaseContrast=50.0', 'TEST', 'RAW trigger f=3200 dur=100 post=1000 dump=bin', 'SEQ MODE quiet|compact|signalcheck|full|system|source|inspect|pattern|dump WHEN off|miss|all VERBOSE 0|1|2 STATUS', 'CAP', 'DET AMP', 'VAL', 'VAL OFF'");
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
    if (configured == SeqOutputMode::Explain || configured == SeqOutputMode::Full) {
        return true;
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
        for (uint16_t i = 0; i < block.sampleCount; ++i) {
            const uint32_t sampleTimeUs = block.approxStartMicros + sampleOffsetUs(static_cast<uint32_t>(i), sampleRateHz);
            AudioSamplePacket frame;
            _audioSignal.update(static_cast<int>(block.samples[i]), sampleTimeUs, frame);
            updateSequenceAudioHealth(frame);
            if (_sequenceTest.outputConfig.frequencyBandEnabled) {
                _freqBandStream.observeCenteredSample(frame.centeredAudioValue, frame.timeMs);
            }
            if (_sequenceTest.active && _sequenceTest.currentTrial > 0) {
                const unsigned long processingLagMs = millis() > frame.timeMs
                    ? millis() - frame.timeMs
                    : 0UL;
                if (processingLagMs > _sequenceTest.maxProcessingLagMs) {
                    _sequenceTest.maxProcessingLagMs = processingLagMs;
                }
                if (frame.timeMs >= _sequenceTest.currentTrialStartMs &&
                    frame.timeMs <= _sequenceTest.currentTrialEndMs) {
                    ++_sequenceTest.currentTrialSamplesProcessed;
                }
                detection::FrequencyFeatureFrame runtimeFrequencyFrame = {};
                if (_sequenceTest.outputConfig.frequencyBandEnabled) {
                    runtimeFrequencyFrame = captureFrequencyFeatureFrame(frame.timeMs);
                } else {
                    runtimeFrequencyFrame.observedAtMs = frame.timeMs;
                }
                _detection->observeFrame(frame, runtimeFrequencyFrame, frame.timeMs);
                while (_detection->popPatternResult(_sequenceTest.currentTrialDiagnostics.runtimePatternResult)) {
                    _sequenceTest.currentTrialDiagnostics.runtimePatternCaptured = true;
                    _sequenceTest.currentTrialDiagnostics.runtimeFieldState = _detection->fieldState();
                    handleSequenceCandidate(_sequenceTest.currentTrialDiagnostics.runtimePatternResult, &runtimeFrequencyFrame);
                }
                updateSequenceAmbientStats(frame.timeMs);
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
    if (_controlClaimPending && !_controlClaimSent && timing::atOrAfter(now, _controlClaimAtMs)) {
        sendEmitterCommand("MODE REMOTE");
        _controlClaimSent = true;
        _controlClaimPending = false;
    }
    processPendingSequenceStart();
    updateSequenceTest(now);
    updateCaptureSession(now);
    pollUsbConsole();
    pollEmitterSerial();
    if (_valMode) {
        printValueFrame(now);
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
        printBaseSummary();
        stopBaseSession();
        Serial.println("BASE stopped");
    }
}


// -----------------------------------------------------------------------------
// Raw trigger and value-mode helpers
// -----------------------------------------------------------------------------

void AnalyzerApp::printValueModeBanner() const {
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

    PendingSequenceStart pending = _pendingSequenceStart;
    _pendingSequenceStart.active = false;

    startSequenceTest(
        pending.totalTrials,
        pending.periodMs,
        pending.windowEndOffsetMs,
        pending.toneHz,
        pending.durationMs,
        pending.quiet,
        pending.showDetails,
        pending.diagMode,
        pending.setupLabel,
        pending.sampleDumpEnabled,
        pending.sampleDumpFirstTrials,
        pending.sampleDumpEveryNth,
        pending.sampleDumpLeadMs,
        pending.sampleDumpTailMs,
        pending.sampleDumpStepMs,
        pending.sampleDumpMaxRows,
        pending.startupDelayMs,
        pending.profileKind,
        pending.externalEmitter);
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
        case detection::DetectionProfileKind::TonalPulse:
        default:
            return "generic tonal pulse profile view";
    }
}

AnalyzerReport* AnalyzerApp::sequenceReportScratch() {
    if (_sequenceReportScratch == nullptr) {
        _sequenceReportScratch = new (std::nothrow) AnalyzerReport();
        if (_sequenceReportScratch != nullptr) {
            *_sequenceReportScratch = makeEmptyAnalyzerReport();
        }
    }

    if (_sequenceReportScratch == nullptr) {
        return nullptr;
    }

    *_sequenceReportScratch = makeEmptyAnalyzerReport();
    return _sequenceReportScratch;
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

    const detection::DetectionPipelineResult* pipelineResult = _detection != nullptr && _detection->hasLatestPipelineResult()
        ? &_detection->latestPipelineResult()
        : nullptr;
    const bool runtimeReceivedOccurrence = pipelineResult != nullptr && pipelineResult->hasOccurrence;
    const bool actualPipelineAvailable = pipelineResult != nullptr && pipelineResult->hasPattern;
    const detection::PatternResult* runtimePatternResult = actualPipelineAvailable ? &pipelineResult->pattern : nullptr;
    const detection::InspectedOccurrence* runtimeInspectedOccurrence = nullptr;
    if (runtimePatternResult != nullptr && runtimePatternResult->inspectedOccurrence != nullptr) {
        runtimeInspectedOccurrence = runtimePatternResult->inspectedOccurrence;
    }
    const detection::FieldState* runtimeFieldState = actualPipelineAvailable && pipelineResult->hasField
        ? &pipelineResult->field
        : nullptr;
    const detection::DetectionProfile& selectedProfile = detection::detectionProfileForKind(_sequenceTest.profileKind);
    const bool trialHasPipelineEvidence = actualPipelineAvailable
        && runtimePatternResult != nullptr
        && diagnostics.rawCandidateCount > 0;
    const auto artifactReason = [&]() -> const char* {
        if (actualPipelineAvailable) {
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
    classificationInput.dtMs = dtMs;
    classificationInput.rawCandidateCount = diagnostics.rawCandidateCount;
    classificationInput.audioOverflow = audioOverflow;
    classificationInput.patternAvailable = actualPipelineAvailable && runtimePatternResult != nullptr;
    report.classification = classifySequenceTrial(classificationInput);
    {
        // Analyzer consumes the PatternResult produced by DetectionRuntime.
        // Analyzer does not re-run occurrence inspection or pattern interpretation.
        AnalyzerPatternObservation pattern = {};
        pattern.type = trialHasPipelineEvidence ? detection::patternTypeName(runtimePatternResult->type) : "unknown";
        pattern.accepted = trialHasPipelineEvidence
            ? runtimePatternResult->valid
            : false;
        pattern.candidateAccepted = trialHasPipelineEvidence ? runtimePatternResult->patternCandidateAccepted : false;
        pattern.patternMatched = trialHasPipelineEvidence ? runtimePatternResult->patternMatched : false;
        pattern.supportMatched = trialHasPipelineEvidence ? runtimePatternResult->supportMatched : false;
        pattern.behaviorEligible = pattern.accepted;
        pattern.confidence = trialHasPipelineEvidence ? runtimePatternResult->confidence : 0.0f;
        pattern.dtMs = report.classification.dtMs;
        pattern.ampStrength = trialHasPipelineEvidence ? strengthClassName(runtimePatternResult->ampStrength) : "unknown";
        pattern.reason = trialHasPipelineEvidence ? detection::patternReasonName(runtimePatternResult->reasonCode) : analyzerReasonName(report.classification.reason);
        pattern.rejectReason = trialHasPipelineEvidence ? detection::patternRejectReasonName(runtimePatternResult->rejectReason) : analyzerReasonName(report.classification.reason);
        pattern.involvedOccurrences = trialHasPipelineEvidence ? runtimePatternResult->occurrenceCount : 0U;
        report.primaryPattern = pattern;
    }

    report.occurrences.total = diagnostics.rawCandidateCount;
    report.occurrences.accepted = trialHasPipelineEvidence && runtimePatternResult->valid ? 1U : 0U;
    report.occurrences.rejected = diagnostics.rawCandidateCount > report.occurrences.accepted ? diagnostics.rawCandidateCount - report.occurrences.accepted : 0U;
    if (trialHasPipelineEvidence && runtimeInspectedOccurrence != nullptr && runtimeInspectedOccurrence->occurrence.present) {
        const detection::Occurrence& occurrence = runtimeInspectedOccurrence->occurrence;
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
        report.occurrences.mainRejectReason = runtimeInspectedOccurrence->decision == detection::OccurrenceDecision::Rejected
            ? occurrenceRejectReasonName(runtimeInspectedOccurrence->rejectReason)
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
        report.occurrences.primaryDtMs = dtMs;
        report.occurrences.primaryDurationMs = durMs >= 0 ? static_cast<unsigned long>(durMs) : 0UL;
        report.occurrences.primaryStrength = strength;
        report.occurrences.score = runtimePatternResult != nullptr ? runtimePatternResult->freq.score : 0.0f;
        report.occurrences.contrast = runtimePatternResult != nullptr ? runtimePatternResult->freq.spectralContrast : 0.0f;
        report.occurrences.strength = strength;
        report.occurrences.confidence = trialHasPipelineEvidence ? runtimePatternResult->confidence : 0.0f;
        report.occurrences.mainRejectReason = analyzerReasonName(report.classification.reason);
        report.occurrences.rejectReason = report.occurrences.mainRejectReason;
    }

    report.inspection.inspected = diagnostics.rawCandidateCount;
    report.inspection.accepted = report.occurrences.accepted;
    report.inspection.rejected = diagnostics.rawCandidateCount > report.inspection.accepted ? diagnostics.rawCandidateCount - report.inspection.accepted : 0U;
    if (trialHasPipelineEvidence && runtimeInspectedOccurrence != nullptr && runtimeInspectedOccurrence->occurrence.present) {
        report.inspection.primaryEvidence = occurrenceSourceName(runtimeInspectedOccurrence->occurrence.source);
        switch (selectedProfile.patternRulesConfig.requiredSupportTarget) {
            case detection::EvidenceTarget::FrequencyScoreStrength:
                report.inspection.moduleTarget = "frequency_score";
                report.inspection.moduleStrengthClass = strengthClassName(runtimeInspectedOccurrence->occurrence.frequencyScoreStrength);
                break;
            case detection::EvidenceTarget::TargetBandStrength:
                report.inspection.moduleTarget = "target_band";
                report.inspection.moduleStrengthClass = strengthClassName(runtimeInspectedOccurrence->occurrence.targetBandStrength);
                break;
            case detection::EvidenceTarget::AmpStrength:
            default:
                report.inspection.moduleTarget = "amp_strength";
                report.inspection.moduleStrengthClass = strengthClassName(runtimeInspectedOccurrence->occurrence.ampStrength);
                break;
        }
        report.inspection.mainRejectReason = runtimeInspectedOccurrence->decision == detection::OccurrenceDecision::Rejected ? occurrenceRejectReasonName(runtimeInspectedOccurrence->rejectReason) : "none";
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
    report.profileDetail.emitter = detection::occurrenceSourceKindName(selectedProfile.occurrenceSource);
    report.profileDetail.inspectionAcceptance = detection::occurrenceSourceKindName(selectedProfile.occurrenceSource);
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
        report.profileDetail.freqScore = trialHasPipelineEvidence ? runtimePatternResult->freq.score : 0.0f;
        report.profileDetail.freqContrast = trialHasPipelineEvidence ? runtimePatternResult->freq.spectralContrast : 0.0f;
    }
    report.profileDetail.freqScoreMin = selectedProfile.frequencyMatch.attackScoreMin;
    report.profileDetail.freqContrastMin = selectedProfile.frequencyMatch.attackContrastMin;
    report.profileDetail.ampCenteredMagnitude = report.occurrences.primaryStrength;
    report.profileDetail.ampLevel = report.profileDetail.ampCenteredMagnitude;
    report.profileDetail.ampBase = diagnostics.acceptedAmbientBaseline;
    report.profileDetail.ampLift = report.profileDetail.ampCenteredMagnitude - report.profileDetail.ampBase;
    const detection::ScalarInspectionObservation emptyScalarObservation{};
    const detection::ScalarInspectionObservation& selectedScalarObservation =
        trialHasPipelineEvidence && runtimeInspectedOccurrence != nullptr && runtimeInspectedOccurrence->occurrence.scalarEvidence.available
            ? runtimeInspectedOccurrence->occurrence.scalarEvidence
            : emptyScalarObservation;
    report.profileDetail.scalarObservation = selectedScalarObservation;
    report.profileDetail.inspectionObservationCount = 0;
    if (trialHasPipelineEvidence && runtimeInspectedOccurrence != nullptr) {
        const size_t availableCount = runtimeInspectedOccurrence->scalarObservationCount;
        const size_t moduleCount = selectedProfile.inspectionPlan.count;
        const size_t copyCount = availableCount < moduleCount ? availableCount : moduleCount;
        report.profileDetail.inspectionObservationCount = copyCount;
        for (size_t i = 0; i < copyCount; ++i) {
            report.profileDetail.inspectionObservations[i] = runtimeInspectedOccurrence->scalarObservations[i];
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
    report.debug.mainRejectReason = trialHasPipelineEvidence && runtimeInspectedOccurrence != nullptr
        ? (runtimeInspectedOccurrence->decision == detection::OccurrenceDecision::Rejected ? occurrenceRejectReasonName(runtimeInspectedOccurrence->rejectReason) : "none")
        : analyzerReasonName(report.classification.reason);

    const bool diagnosticsRequested = _sequenceTest.outputConfig.when != AnalyzerApp::SeqOutputWhen::Off &&
        _sequenceTest.outputConfig.diagnosticsEnabled;
    const detection::DetectionDiagnostics* runtimeDiag = nullptr;
    const FrequencyMatchDetector* frequencyDetector = nullptr;
    if (diagnosticsRequested && _detection != nullptr) {
        _detection->captureDiagnostics();
        runtimeDiag = &_detection->diagnostics();
        frequencyDetector = &_detection->frequencyEmitter().detector();
    }

    report.frequency.currentTrialId = report.context.trial;
    report.frequency.windowStartMs = _sequenceTest.currentTrialStartMs;
    report.frequency.windowEndMs = _sequenceTest.currentTrialEndMs;
    report.frequency.expectedWindowMs = _sequenceTest.currentTrialEndMs >= _sequenceTest.currentTrialStartMs
        ? _sequenceTest.currentTrialEndMs - _sequenceTest.currentTrialStartMs
        : 0UL;
    report.frequency.expectedFrameCountEstimate =
        static_cast<unsigned long>((report.frequency.expectedWindowMs
            * static_cast<unsigned long>(_audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL)) / 1000UL);

    report.frequency.acceptedPresent = report.occurrences.present
        && report.occurrences.valid
        && report.primaryPattern.accepted;
    report.frequency.acceptedTrialId = report.frequency.acceptedPresent ? report.context.trial : 0UL;
    report.frequency.acceptedSource = report.frequency.acceptedPresent
        ? (report.occurrences.primarySource != nullptr ? report.occurrences.primarySource : "unknown")
        : "none";
    report.frequency.acceptedDtMs = report.frequency.acceptedPresent ? report.occurrences.primaryDtMs : -1;
    report.frequency.acceptedStartMs = report.frequency.acceptedPresent ? report.occurrences.startMs : 0UL;
    report.frequency.acceptedPeakMs = report.frequency.acceptedPresent ? report.occurrences.peakMs : 0UL;
    report.frequency.acceptedReleaseMs = report.frequency.acceptedPresent ? report.occurrences.releaseMs : 0UL;
    report.frequency.acceptedDurationMs = report.frequency.acceptedPresent ? report.occurrences.primaryDurationMs : 0UL;
    report.frequency.acceptedStrength = report.frequency.acceptedPresent ? report.occurrences.primaryStrength : 0.0f;
    report.frequency.acceptedScore = report.frequency.acceptedPresent ? report.occurrences.score : 0.0f;
    report.frequency.acceptedContrast = report.frequency.acceptedPresent ? report.occurrences.contrast : 0.0f;
    report.frequency.freshFrames = _freqBandStream.profileComputeCalls();
    report.frequency.heldFrames = _freqBandStream.profileObserveCalls() > _freqBandStream.profileComputeCalls()
        ? _freqBandStream.profileObserveCalls() - _freqBandStream.profileComputeCalls()
        : 0UL;
    if (_detection != nullptr) {
        report.frequency.historyScoreRecords = _detection->featureHistory().sampleCount(detection::FeatureStreamId::FrequencyScore);
        report.frequency.historyContrastRecords = _detection->featureHistory().sampleCount(detection::FeatureStreamId::FrequencyContrast);
    }

    bool hasCurrentSourceEvidence = false;
    if (runtimeDiag != nullptr) {
        report.frequency.frames = runtimeDiag->frequencyFrames;
        report.frequency.validFrames = runtimeDiag->frequencyValidFrames;
        report.frequency.scoreOkFrames = runtimeDiag->frequencyScoreOkFrames;
        report.frequency.contrastOkFrames = runtimeDiag->frequencyContrastOkFrames;
        report.frequency.bothOkFrames = runtimeDiag->frequencyBothOkFrames;
        report.frequency.matchFrames = runtimeDiag->frequencyMatchFrames;
        report.frequency.rejectFrames = runtimeDiag->frequencyRejectFrames;
        report.frequency.releaseScoreOkFrames = runtimeDiag->frequencyReleaseScoreOkFrames;
        report.frequency.releaseContrastOkFrames = runtimeDiag->frequencyReleaseContrastOkFrames;
        report.frequency.releaseBothOkFrames = runtimeDiag->frequencyReleaseBothOkFrames;
        report.frequency.releaseScoreTooLowFrames = runtimeDiag->frequencyReleaseScoreTooLowFrames;
        report.frequency.releaseContrastTooLowFrames = runtimeDiag->frequencyReleaseContrastTooLowFrames;
        report.frequency.releaseScoreAndContrastTooLowFrames = runtimeDiag->frequencyReleaseScoreAndContrastTooLowFrames;
        report.frequency.releaseNoEvidenceFrames = runtimeDiag->frequencyReleaseNoEvidenceFrames;
        report.frequency.diagLongestMatchStreakFrames = runtimeDiag->frequencyDiagLongestMatchStreakFrames;
        report.frequency.diagLongestMatchStreakMs = sampleFramesToMs(
            runtimeDiag->frequencyDiagLongestMatchStreakFrames,
            _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL
        );
        report.frequency.audioHealth = diagnostics.audioHealth != nullptr ? diagnostics.audioHealth : "unknown";
        report.frequency.audioZeroishFrames = diagnostics.audioZeroishFrames;
        report.frequency.audioFlatlineFrames = diagnostics.audioFlatlineFrames;
        report.frequency.audioLargeJumpFrames = diagnostics.audioLargeJumpFrames;
        report.frequency.audioRmsTooLowFrames = diagnostics.audioRmsTooLowFrames;
        report.frequency.audioRmsTooHighFrames = diagnostics.audioRmsTooHighFrames;
        report.frequency.audioMaxAbsDelta = diagnostics.audioMaxAbsDelta;
        report.frequency.meanScore = runtimeDiag->frequencyScoreMean;
        report.frequency.meanContrast = runtimeDiag->frequencyContrastMean;
        report.frequency.sumScore = report.frequency.meanScore * static_cast<float>(report.frequency.frames);
        report.frequency.sumContrast = report.frequency.meanContrast * static_cast<float>(report.frequency.frames);
        report.frequency.scoreThreshold = runtimeDiag->frequencyScoreThreshold;
        report.frequency.contrastThreshold = runtimeDiag->frequencyContrastThreshold;
        report.frequency.maxScore = runtimeDiag->frequencyScoreMax;
        report.frequency.maxScoreMs = runtimeDiag->frequencyScoreMaxMs;
        report.frequency.maxContrast = runtimeDiag->frequencyContrastMax;
        report.frequency.maxContrastMs = runtimeDiag->frequencyContrastMaxMs;
        report.frequency.ampPeak = diagnostics.ambientBaselineSamples > 0
            ? static_cast<float>(diagnostics.maxSignalLevel)
            : 0.0f;
        report.frequency.ampMean = diagnostics.ambientBaselineSamples > 0
            ? diagnostics.ambientBaselineSum / static_cast<float>(diagnostics.ambientBaselineSamples)
            : 0.0f;
        report.frequency.ampPeakMs = diagnostics.ambientBaselineSamples > 0
            ? diagnostics.ampPeakMs
            : 0UL;
        report.frequency.minScore = runtimeDiag->frequencyScoreMin;
        report.frequency.minContrast = runtimeDiag->frequencyContrastMin;
        report.frequency.peakScore = runtimeDiag->frequencyPeakScore;
        report.frequency.peakContrast = runtimeDiag->frequencyPeakContrast;
        report.frequency.peakWindowSampleCount = runtimeDiag->frequencyPeakWindowSampleCount;
        report.frequency.sourceSummary.present = runtimeDiag->sourceSummary.present;
        report.frequency.sourceSummary.origin = "runtime_frequency_diag";
        report.frequency.sourceSummary.candidateCount = runtimeDiag->sourceSummary.candidateCount;
        report.frequency.sourceSummary.rejectCount = runtimeDiag->sourceSummary.rejectCount;
        report.frequency.sourceSummary.bestDurationMs = runtimeDiag->sourceSummary.bestDurationMs;
        report.frequency.sourceSummary.secondBestDurationMs = runtimeDiag->sourceSummary.secondBestDurationMs;
        report.frequency.sourceSummary.bestOpenMs = runtimeDiag->sourceSummary.bestOpenMs;
        report.frequency.sourceSummary.bestPeakMs = runtimeDiag->sourceSummary.bestPeakMs;
        report.frequency.sourceSummary.bestLastMatchMs = runtimeDiag->sourceSummary.bestLastMatchMs;
        report.frequency.sourceSummary.bestCloseMs = runtimeDiag->sourceSummary.bestCloseMs;
        report.frequency.sourceSummary.bestPeakPrimary = runtimeDiag->sourceSummary.bestPeakPrimary;
        report.frequency.sourceSummary.bestPeakSecondary = runtimeDiag->sourceSummary.bestPeakSecondary;
        report.frequency.sourceSummary.bestRejectReason = runtimeDiag->sourceSummary.bestRejectReason;
        report.frequency.sourceSummary.bestGateReason = runtimeDiag->sourceSummary.bestGateReason;
        report.frequency.sourceSummary.closeCause = runtimeDiag->sourceSummary.closeCause;
        report.frequency.sourceSummary.scoreTooLowFrames = runtimeDiag->sourceSummary.scoreTooLowFrames;
        report.frequency.sourceSummary.contrastTooLowFrames = runtimeDiag->sourceSummary.contrastTooLowFrames;
        report.frequency.sourceSummary.scoreAndContrastTooLowFrames = runtimeDiag->sourceSummary.scoreAndContrastTooLowFrames;
        report.frequency.sourceSummary.maxPeakPrimary = runtimeDiag->sourceSummary.maxPeakPrimary;
        report.frequency.sourceSummary.maxPeakPrimaryMs = runtimeDiag->sourceSummary.maxPeakPrimaryMs;
        report.frequency.sourceSummary.maxPeakSecondary = runtimeDiag->sourceSummary.maxPeakSecondary;
        report.frequency.sourceSummary.maxPeakSecondaryMs = runtimeDiag->sourceSummary.maxPeakSecondaryMs;
        report.frequency.sourceSummary.totalMatchMs = runtimeDiag->sourceSummary.totalMatchMs;
        report.frequency.sourceSummary.totalGapMs = runtimeDiag->sourceSummary.totalGapMs;
        report.frequency.sourceSummary.maxGapMs = runtimeDiag->sourceSummary.maxGapMs;
        report.frequency.sourceSummary.islandCount = runtimeDiag->sourceSummary.islandCount;
        report.frequency.sourceLastCandidate.present = runtimeDiag->sourceLastCandidate.present;
        report.frequency.sourceLastCandidate.peakMs = runtimeDiag->sourceLastCandidate.peakMs;
        report.frequency.sourceLastCandidate.durationMs = runtimeDiag->sourceLastCandidate.durationMs;
        report.frequency.sourceLastCandidate.windowSamples = runtimeDiag->sourceLastCandidate.windowSamples;
        report.frequency.sourceLastCandidate.peakPrimary = runtimeDiag->sourceLastCandidate.peakPrimary;
        report.frequency.sourceLastCandidate.peakSecondary = runtimeDiag->sourceLastCandidate.peakSecondary;
        report.frequency.sourceLastCandidate.reason = runtimeDiag->sourceLastCandidate.reason;
        report.frequency.sourceLastCandidate.gateReason = runtimeDiag->sourceLastCandidate.gateReason;
        report.frequency.sourceLastCandidate.scope = report.frequency.sourceLastCandidate.present
            ? (report.frequency.sourceLastCandidate.peakMs >= report.frequency.windowStartMs && report.frequency.sourceLastCandidate.peakMs <= report.frequency.windowEndMs
                ? "in_window"
                : (report.frequency.sourceLastCandidate.peakMs < report.frequency.windowStartMs ? "before_window" : "after_window"))
            : "stale";
        hasCurrentSourceEvidence = report.frequency.acceptedPresent || report.frequency.sourceSummary.present;
        if (!hasCurrentSourceEvidence) {
            report.frequency.sourceLastCandidate.present = false;
            report.frequency.sourceLastCandidate.peakMs = 0;
            report.frequency.sourceLastCandidate.durationMs = 0;
            report.frequency.sourceLastCandidate.windowSamples = 0;
            report.frequency.sourceLastCandidate.peakPrimary = 0.0f;
            report.frequency.sourceLastCandidate.peakSecondary = 0.0f;
            report.frequency.sourceLastCandidate.reason = "none";
            report.frequency.sourceLastCandidate.gateReason = "none";
            report.frequency.sourceLastCandidate.scope = "unknown";
        }
        report.frequency.liveFreqReason = runtimeDiag->frequencyRejectReason != nullptr ? runtimeDiag->frequencyRejectReason : "none";
        report.frequency.liveFreqWould = runtimeDiag->frequencyWouldCandidateReason != nullptr ? runtimeDiag->frequencyWouldCandidateReason : "none";
        report.frequency.liveFreqState = runtimeDiag->frequencyCandidateState != nullptr ? runtimeDiag->frequencyCandidateState : "none";
        report.frequency.liveFreqReady = runtimeDiag->frequencyReadyOk;
        report.frequency.liveFreqGate = runtimeDiag->frequencyGateOpen;
        report.frequency.liveFreqPresent = runtimeDiag->frequencyPresent;
        report.frequency.liveFreqValid = runtimeDiag->frequencyValidWindow;
        report.frequency.liveFreqMatch = runtimeDiag->frequencyMatched;
        report.frequency.analyzerMissReason = report.classification.result == AnalyzerResult::Miss
            ? analyzerReasonName(report.classification.reason)
            : "none";
        report.frequency.nearMiss = runtimeDiag->frequencyNearMiss;
        report.frequency.nearMissReason = runtimeDiag->frequencyNearMissReason != nullptr ? runtimeDiag->frequencyNearMissReason : "none";
    }

    report.frequencyDetector = frequencyDetector;

    if (frequencyDetector != nullptr) {
        report.frequency.sourceOccurrenceEmitted = frequencyDetector->candidateEmitted;
        report.frequency.runtimeEvidenceSeen = runtimeDiag != nullptr ? runtimeDiag->frequencyPresent : false;
        report.frequency.runtimeOccurrenceReceived = report.frequency.sourceOccurrenceEmitted && runtimeReceivedOccurrence;
        report.frequency.sourceLastRejectReason = runtimeDiag != nullptr && runtimeDiag->frequencyRejectReason != nullptr
            ? runtimeDiag->frequencyRejectReason
            : "none";
        report.frequency.selectedRejectReason = runtimeDiag != nullptr && report.frequency.sourceOccurrenceEmitted && !report.frequency.acceptedPresent && runtimeDiag->frequencyNoEmitReason != nullptr
            ? runtimeDiag->frequencyNoEmitReason
            : "none";
        report.frequency.selectedRejectGateReason = runtimeDiag != nullptr && runtimeDiag->frequencyGateReason != nullptr
            ? runtimeDiag->frequencyGateReason
            : "none";
        report.frequency.fmOpened = runtimeDiag != nullptr ? runtimeDiag->frequencyOpened : false;
        report.frequency.fmReleased = runtimeDiag != nullptr ? runtimeDiag->frequencyReleased : false;
        report.frequency.fmEmitted = runtimeDiag != nullptr ? runtimeDiag->frequencyEmitted : false;
        report.frequency.fmDurationOk = runtimeDiag != nullptr ? runtimeDiag->frequencyValidRelease : false;
        report.frequency.fmValidRelease = runtimeDiag != nullptr ? runtimeDiag->frequencyValidRelease : false;
        report.frequency.fmEmitAllowed = runtimeDiag != nullptr ? runtimeDiag->frequencyEmitAllowed : false;
        report.frequency.fmCloseCause = runtimeDiag != nullptr && runtimeDiag->sourceSummary.closeCause != nullptr
            ? runtimeDiag->sourceSummary.closeCause
            : "none";
        report.frequency.fmOpenMs = runtimeDiag != nullptr ? runtimeDiag->frequencyOpenMs : 0UL;
        report.frequency.fmPeakMs = runtimeDiag != nullptr ? runtimeDiag->frequencyPeakMs : 0UL;
        report.frequency.fmReleaseMs = runtimeDiag != nullptr ? runtimeDiag->frequencyReleaseMs : 0UL;
        report.frequency.fmDurationMs = runtimeDiag != nullptr ? runtimeDiag->frequencyDurationMs : 0UL;
        report.frequency.fmMinDurationMs = runtimeDiag != nullptr ? runtimeDiag->frequencyMinDurationMs : 0UL;
        report.frequency.fmMaxDurationMs = runtimeDiag != nullptr ? runtimeDiag->frequencyMaxDurationMs : 0UL;
        report.frequency.diagFirstFrameMs = report.frequency.fmOpenMs;
        report.frequency.diagLastFrameMs = report.frequency.fmReleaseMs;
        if (!hasCurrentSourceEvidence) {
            report.frequency.diagFirstFrameMs = 0;
            report.frequency.diagLastFrameMs = 0;
            report.frequency.fmOpenMs = 0;
            report.frequency.fmPeakMs = 0;
            report.frequency.fmReleaseMs = 0;
            report.frequency.fmDurationMs = 0;
            report.frequency.fmMinDurationMs = 0;
            report.frequency.fmMaxDurationMs = 0;
            report.frequency.fmDurationOk = false;
            report.frequency.fmOpened = false;
            report.frequency.fmReleased = false;
            report.frequency.fmEmitted = false;
            report.frequency.fmValidRelease = false;
            report.frequency.fmEmitAllowed = false;
            report.frequency.fmCloseCause = "none";
        }
        report.frequency.diagFrameCountOk = report.frequency.expectedFrameCountEstimate == 0
            ? report.frequency.frames == 0
            : report.frequency.frames > 0;
        report.frequency.detectionGateBlocked = !runtimeDiag->frequencyGateOpen || !runtimeDiag->frequencyReadyOk;
        if (!runtimeDiag->frequencyReadyOk) {
            report.frequency.detectionGateReason = "not_ready";
        } else if (!runtimeDiag->frequencyGateOpen) {
            report.frequency.detectionGateReason = report.frequency.selectedRejectGateReason != nullptr && report.frequency.selectedRejectGateReason[0] != '\0'
                ? report.frequency.selectedRejectGateReason
                : "unknown";
        } else {
            report.frequency.detectionGateReason = "none";
        }
    }
    report.frequency.inconsistent = report.classification.result == AnalyzerResult::Miss && report.frequency.acceptedPresent;
    report.frequency.freqEvidenceClass = frequencyEvidenceClassLabel(classifyFrequencyEvidence(report));
    if (report.frequency.analyzerMissReason == nullptr || report.frequency.analyzerMissReason[0] == '\0') {
        report.frequency.analyzerMissReason = report.classification.result == AnalyzerResult::Miss
            ? "no_accepted_occurrence"
            : "none";
    }
    if (report.classification.result == AnalyzerResult::Miss && !report.frequency.acceptedPresent && report.frequency.analyzerMissReason != nullptr && strcmp(report.frequency.analyzerMissReason, "occurrence_emitted") == 0) {
        report.frequency.analyzerMissReason = "unknown_or_stale_reason";
        report.frequency.inconsistent = true;
    }
    report.frequency.analyzerSeenOccurrence = report.frequency.acceptedPresent;
    if (!report.frequency.sourceOccurrenceEmitted) {
        report.frequency.runtimeOccurrenceReceived = false;
    }

    const bool scalarProfile = selectedProfile.occurrenceSource == detection::OccurrenceSourceKind::ScalarTransient;
    if (scalarProfile) {
        report.scalar.currentTrialId = report.context.trial;
        report.scalar.windowStartMs = _sequenceTest.currentTrialStartMs;
        report.scalar.windowEndMs = _sequenceTest.currentTrialEndMs;
        report.scalar.expectedWindowMs = report.scalar.windowEndMs >= report.scalar.windowStartMs
            ? report.scalar.windowEndMs - report.scalar.windowStartMs
            : 0UL;
        report.scalar.expectedFrameCountEstimate =
            static_cast<unsigned long>((report.scalar.expectedWindowMs
                * static_cast<unsigned long>(_audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL)) / 1000UL);
        report.scalar.diagFrameCountOk = report.scalar.expectedWindowMs > 0 && report.scalar.expectedFrameCountEstimate > 0;

        report.scalar.acceptedPresent = report.occurrences.present
            && report.occurrences.valid
            && report.primaryPattern.accepted
            && report.occurrences.primarySource != nullptr
            && strcmp(report.occurrences.primarySource, "amp") == 0;
        report.scalar.acceptedTrialId = report.scalar.acceptedPresent ? report.context.trial : 0UL;
        report.scalar.acceptedSource = report.scalar.acceptedPresent
            ? (report.occurrences.primarySource != nullptr ? report.occurrences.primarySource : "unknown")
            : "none";
        report.scalar.acceptedDtMs = report.scalar.acceptedPresent ? report.occurrences.primaryDtMs : -1;
        report.scalar.acceptedStartMs = report.scalar.acceptedPresent ? report.occurrences.startMs : 0UL;
        report.scalar.acceptedPeakMs = report.scalar.acceptedPresent ? report.occurrences.peakMs : 0UL;
        report.scalar.acceptedReleaseMs = report.scalar.acceptedPresent ? report.occurrences.releaseMs : 0UL;
        report.scalar.acceptedDurationMs = report.scalar.acceptedPresent ? report.occurrences.primaryDurationMs : 0UL;
        report.scalar.acceptedStrength = report.scalar.acceptedPresent ? report.occurrences.primaryStrength : 0.0f;
        report.scalar.acceptedScore = report.scalar.acceptedPresent ? report.occurrences.score : 0.0f;
        report.scalar.acceptedContrast = report.scalar.acceptedPresent ? report.occurrences.contrast : 0.0f;

        if (runtimeDiag != nullptr) {
            report.scalar.scalarRejectReason = runtimeDiag->scalarRejectReason != nullptr ? runtimeDiag->scalarRejectReason : "unknown";
            report.scalar.scalarNoEmitReason = runtimeDiag->scalarNoEmitReason != nullptr ? runtimeDiag->scalarNoEmitReason : "none";
            report.scalar.scalarGateReason = runtimeDiag->scalarGateReason != nullptr ? runtimeDiag->scalarGateReason : "none";
            report.scalar.scalarOpened = runtimeDiag->scalarOpened;
            report.scalar.scalarReleased = runtimeDiag->scalarReleased;
            report.scalar.scalarValidRelease = runtimeDiag->scalarValidRelease;
            report.scalar.scalarEmitAllowed = runtimeDiag->scalarEmitAllowed;
            report.scalar.scalarOpenMs = runtimeDiag->scalarOpenMs;
            report.scalar.scalarPeakMs = runtimeDiag->scalarPeakMs;
            report.scalar.scalarReleaseMs = runtimeDiag->scalarReleaseMs;
            report.scalar.scalarDurationMs = runtimeDiag->scalarDurationMs;
            report.scalar.scalarMinDurationMs = runtimeDiag->scalarMinDurationMs;
            report.scalar.scalarMaxDurationMs = runtimeDiag->scalarMaxDurationMs;
            report.scalar.scalarPeakStrength = runtimeDiag->scalarPeakStrength;
        report.scalar.sourceOccurrenceEmitted = report.occurrences.present;
        report.scalar.sourceSummary.present = !report.scalar.acceptedPresent
            && (report.scalar.scalarOpened
                || report.scalar.scalarReleased
                || (report.scalar.scalarRejectReason != nullptr && strcmp(report.scalar.scalarRejectReason, "none") != 0));
        report.scalar.sourceSummary.origin = "synthesized_scalar_lifecycle";
        report.scalar.sourceSummary.candidateCount = report.scalar.sourceSummary.present ? 1UL : 0UL;
        report.scalar.sourceSummary.rejectCount = report.scalar.sourceSummary.candidateCount;
        report.scalar.sourceSummary.bestDurationMs = report.scalar.scalarDurationMs;
        report.scalar.sourceSummary.secondBestDurationMs = 0UL;
        report.scalar.sourceSummary.bestOpenMs = report.scalar.scalarOpenMs;
        report.scalar.sourceSummary.bestPeakMs = report.scalar.scalarPeakMs;
        report.scalar.sourceSummary.bestLastMatchMs = report.scalar.scalarReleaseMs;
        report.scalar.sourceSummary.bestCloseMs = report.scalar.scalarReleaseMs;
        report.scalar.sourceSummary.bestPeakPrimary = report.scalar.scalarPeakStrength;
        report.scalar.sourceSummary.bestPeakSecondary = 0.0f;
        report.scalar.sourceSummary.bestRejectReason = report.scalar.scalarRejectReason != nullptr ? report.scalar.scalarRejectReason : "none";
        report.scalar.sourceSummary.bestGateReason = report.scalar.scalarGateReason != nullptr ? report.scalar.scalarGateReason : "none";
        report.scalar.sourceSummary.scoreTooLowFrames = 0;
        report.scalar.sourceSummary.contrastTooLowFrames = 0;
        report.scalar.sourceSummary.scoreAndContrastTooLowFrames = 0;
        report.scalar.sourceSummary.maxPeakPrimary = runtimeDiag->sourceSummary.maxPeakPrimary;
        report.scalar.sourceSummary.maxPeakPrimaryMs = runtimeDiag->sourceSummary.maxPeakPrimaryMs;
        report.scalar.sourceSummary.maxPeakSecondary = 0.0f;
        report.scalar.sourceSummary.maxPeakSecondaryMs = 0UL;
        report.scalar.sourceSummary.totalMatchMs = report.scalar.scalarDurationMs;
        report.scalar.sourceSummary.totalGapMs = runtimeDiag->sourceSummary.totalGapMs;
        report.scalar.sourceSummary.maxGapMs = runtimeDiag->sourceSummary.maxGapMs;
        report.scalar.sourceSummary.islandCount = report.scalar.sourceSummary.present ? 1UL : 0UL;
        report.scalar.sourceLastCandidate.present = report.scalar.scalarOpened
            || report.scalar.scalarReleased
            || report.scalar.scalarEmitAllowed;
        report.scalar.sourceLastCandidate.peakMs = report.scalar.scalarPeakMs;
        report.scalar.sourceLastCandidate.durationMs = report.scalar.scalarDurationMs;
        report.scalar.sourceLastCandidate.windowSamples = 0UL;
        report.scalar.sourceLastCandidate.peakPrimary = report.scalar.scalarPeakStrength;
        report.scalar.sourceLastCandidate.peakSecondary = 0.0f;
        report.scalar.sourceLastCandidate.reason = report.scalar.scalarRejectReason != nullptr ? report.scalar.scalarRejectReason : "none";
        report.scalar.sourceLastCandidate.gateReason = report.scalar.scalarGateReason != nullptr ? report.scalar.scalarGateReason : "none";
        report.scalar.sourceLastCandidate.scope = report.scalar.scalarOpened
            ? (report.scalar.scalarPeakMs >= report.scalar.windowStartMs && report.scalar.scalarPeakMs <= report.scalar.windowEndMs
                ? "in_window"
                : (report.scalar.scalarPeakMs < report.scalar.windowStartMs ? "before_window" : "after_window"))
            : "stale";
            report.scalar.runtimeEvidenceSeen = runtimeDiag->scalarOpened
                || runtimeDiag->scalarReleased
                || (runtimeDiag->scalarRejectReason != nullptr && strcmp(runtimeDiag->scalarRejectReason, "none") != 0);
            report.scalar.runtimeOccurrenceReceived = report.scalar.sourceOccurrenceEmitted;
            report.scalar.analyzerSeenOccurrence = report.scalar.acceptedPresent;
            report.scalar.liveScalarReason = runtimeDiag->scalarRejectReason != nullptr ? runtimeDiag->scalarRejectReason : "none";
            report.scalar.liveScalarWould = runtimeDiag->scalarNoEmitReason != nullptr ? runtimeDiag->scalarNoEmitReason : "none";
            report.scalar.liveScalarReady = runtimeDiag->scalarOpened;
            report.scalar.liveScalarGate = runtimeDiag->scalarEmitAllowed;
            report.scalar.liveScalarPresent = report.occurrences.present;
            report.scalar.liveScalarValid = report.occurrences.valid;
            report.scalar.liveScalarMatch = report.primaryPattern.accepted;
            report.scalar.liveScalarState = runtimeDiag->scalarOpened
                ? (runtimeDiag->scalarReleased ? "released" : "active")
                : "idle";
            report.scalar.detectionGateBlocked = !report.scalar.acceptedPresent
                && (report.scalar.scalarOpened
                    || report.scalar.scalarReleased
                    || (report.scalar.scalarRejectReason != nullptr && strcmp(report.scalar.scalarRejectReason, "none") != 0));
            if (!report.scalar.acceptedPresent) {
                if (report.scalar.scalarRejectReason != nullptr && strcmp(report.scalar.scalarRejectReason, "none") != 0) {
                    report.scalar.detectionGateReason = report.scalar.scalarRejectReason;
                } else if (report.scalar.scalarOpened && !report.scalar.scalarReleased) {
                    report.scalar.detectionGateReason = "opened_not_released";
                } else if (!report.scalar.scalarOpened) {
                    report.scalar.detectionGateReason = "no_evidence";
                } else {
                    report.scalar.detectionGateReason = "none";
                }
            } else {
                report.scalar.detectionGateReason = "none";
            }
        }

        if (report.scalar.acceptedPresent) {
            report.scalar.scalarRejectReason = "none";
            report.scalar.scalarNoEmitReason = "none";
            report.scalar.scalarGateReason = "none";
            report.scalar.scalarOpened = true;
            report.scalar.scalarReleased = true;
            report.scalar.scalarValidRelease = true;
            report.scalar.scalarEmitAllowed = true;
            report.scalar.scalarOpenMs = report.scalar.acceptedStartMs;
            report.scalar.scalarPeakMs = report.scalar.acceptedPeakMs;
            report.scalar.scalarReleaseMs = report.scalar.acceptedReleaseMs;
            report.scalar.scalarDurationMs = report.scalar.acceptedDurationMs;
            report.scalar.sourceOccurrenceEmitted = true;
            report.scalar.runtimeEvidenceSeen = true;
            report.scalar.runtimeOccurrenceReceived = true;
            report.scalar.analyzerSeenOccurrence = true;
            report.scalar.liveScalarReason = "none";
            report.scalar.liveScalarWould = "none";
            report.scalar.liveScalarReady = true;
            report.scalar.liveScalarGate = true;
            report.scalar.liveScalarPresent = true;
            report.scalar.liveScalarValid = true;
            report.scalar.liveScalarMatch = true;
            report.scalar.liveScalarState = "released";
            report.scalar.detectionGateBlocked = false;
            report.scalar.detectionGateReason = "none";
        }

        report.scalar.inconsistent = report.classification.result == AnalyzerResult::Miss && report.scalar.acceptedPresent;
    }

    const bool frequencySource = selectedProfile.occurrenceSource == detection::OccurrenceSourceKind::FrequencyMatch;
    report.source.sourceKind = frequencySource ? "frequency_match" : "scalar_transient";
    report.source.sourceName = detection::occurrenceSourceKindName(selectedProfile.occurrenceSource);
    if (frequencySource) {
        report.source.acceptedPresent = report.frequency.acceptedPresent;
        report.source.sourceOccurrenceEmitted = report.frequency.sourceOccurrenceEmitted;
        report.source.runtimeEvidenceSeen = report.frequency.runtimeEvidenceSeen;
        report.source.runtimeOccurrenceReceived = report.frequency.runtimeOccurrenceReceived;
        report.source.analyzerSeen = report.frequency.analyzerSeenOccurrence;
        report.source.detectionGateBlocked = report.frequency.detectionGateBlocked;
        report.source.detectionGateReason = report.frequency.detectionGateReason;
        report.source.sourceSummary = report.frequency.sourceSummary;
        report.source.lastCandidate = report.frequency.sourceLastCandidate;
        report.source.activeAtTrialStart = report.frequency.fmOpened;
        report.source.activeAtTrialEnd = report.frequency.fmReleased;
        report.source.openedThisTrial = report.frequency.fmOpened;
        report.source.closedThisTrial = report.frequency.fmReleased;
        report.source.emittedThisTrial = report.frequency.fmEmitted;
        report.source.rejectedThisTrial = report.frequency.sourceSummary.present && !report.frequency.acceptedPresent;
        report.source.frequencyMatch = report.frequency;
        report.source.frequencyMatch.scoreOkUpdates = report.frequency.scoreOkFrames;
        report.source.frequencyMatch.contrastOkUpdates = report.frequency.contrastOkFrames;
        report.source.frequencyMatch.bothOkUpdates = report.frequency.bothOkFrames;
        report.source.frequencyMatch.freqEvidenceClass = report.frequency.freqEvidenceClass;
        report.source.scalarTransient = report.scalar;
    } else {
        report.source.acceptedPresent = report.scalar.acceptedPresent;
        report.source.sourceOccurrenceEmitted = report.scalar.sourceOccurrenceEmitted;
        report.source.runtimeEvidenceSeen = report.scalar.runtimeEvidenceSeen;
        report.source.runtimeOccurrenceReceived = report.scalar.runtimeOccurrenceReceived;
        report.source.analyzerSeen = report.scalar.analyzerSeenOccurrence;
        report.source.detectionGateBlocked = report.scalar.detectionGateBlocked;
        report.source.detectionGateReason = report.scalar.detectionGateReason;
        report.source.sourceSummary = report.scalar.sourceSummary;
        report.source.lastCandidate = report.scalar.sourceLastCandidate;
        report.source.activeAtTrialStart = report.scalar.scalarOpened;
        report.source.activeAtTrialEnd = report.scalar.scalarReleased;
        report.source.openedThisTrial = report.scalar.scalarOpened;
        report.source.closedThisTrial = report.scalar.scalarReleased;
        report.source.emittedThisTrial = report.scalar.scalarEmitted;
        report.source.rejectedThisTrial = report.scalar.sourceSummary.present && !report.scalar.acceptedPresent;
        report.source.frequencyMatch = report.frequency;
        report.source.frequencyMatch.scoreOkUpdates = report.frequency.scoreOkFrames;
        report.source.frequencyMatch.contrastOkUpdates = report.frequency.contrastOkFrames;
        report.source.frequencyMatch.bothOkUpdates = report.frequency.bothOkFrames;
        report.source.frequencyMatch.freqEvidenceClass = report.frequency.freqEvidenceClass;
        report.source.scalarTransient = report.scalar;
    }

}









