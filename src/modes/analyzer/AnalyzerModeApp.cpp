#include "AnalyzerModeApp.h"

#include <Arduino.h>
#include <esp_system.h>
#include <stdlib.h>
#include <string.h>

#include "../../app/RuntimeDefaults.h"
#include "../../app/TimingUtils.h"
#include "../../detection/detectors/DetectorNames.h"
#include "../../detection/analyzer/AnalyzerText.h"
#include "../../detection/analyzer/AnalyzerPassRules.h"
#include "../../detection/analyzer/tools/AnalyzerRawHealth.h"
#include "../../detection/inspection/InspectionNames.h"
#include "../../detection/occurrences/OccurrenceNames.h"
#include "../../detection/patterns/PatternNames.h"
#include "../../detection/occurrences/Occurrence.h"
#include "../../detection/analyzer/AnalyzerTrialClassifier.h"
#include "../../detection/inspection/OccurrenceInspector.h"

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

float sequenceSampleDumpInputValue(detection::DetectorSelection detectorSelection,
                                   detection::FeatureStreamId observedStream,
                                   const AudioSamplePacket& audioSamplePacket,
                                   const detection::FrequencyBandMeasurementPacket& frequencyEvidence) {
    switch (detectorSelection) {
        case detection::DetectorSelection::FrequencyMatch:
            return frequencyEvidence.targetBandValue;
        case detection::DetectorSelection::ScalarTransient:
            switch (observedStream) {
                case detection::FeatureStreamId::AmpMagnitude:
                    return static_cast<float>(audioSamplePacket.audioMagnitudeValue);
                case detection::FeatureStreamId::AmpEnvelope:
                    return static_cast<float>(audioSamplePacket.smoothedLevel);
                case detection::FeatureStreamId::FrequencyTarget:
                    return frequencyEvidence.targetBandValue;
                case detection::FeatureStreamId::FrequencyContrast:
                    return frequencyEvidence.targetBandContrastValue;
                case detection::FeatureStreamId::Unknown:
                default:
                    return audioSamplePacket.audioMagnitudeValue;
            }
    }

    return audioSamplePacket.audioMagnitudeValue;
}


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
    Serial.print("  SIZE DetectorInputSample sampleHistory=");
    Serial.println(static_cast<unsigned long>(sizeof(DetectorInputSample) * AnalyzerApp::debugSequenceTestSampleHistoryCapacity()));
    Serial.print("  SIZE DetectorInputSample sampleHistoryPending=");
    Serial.println(static_cast<unsigned long>(sizeof(DetectorInputSample)));
    Serial.print("  SIZE DetectorInputSample sampleRows=");
    Serial.println(static_cast<unsigned long>(sizeof(DetectorInputSample) * AnalyzerApp::debugSequenceTestSampleRowsCapacity()));
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
    const long baselineCorrectedSample = static_cast<long>(audioSamplePacket.baselineCorrectedValue);
    const long rawSample = static_cast<long>(audioSamplePacket.rawAudioValue);
    const unsigned long absCentered = static_cast<unsigned long>(baselineCorrectedSample >= 0 ? baselineCorrectedSample : -baselineCorrectedSample);
    const unsigned long absRaw = static_cast<unsigned long>(rawSample >= 0 ? rawSample : -rawSample);
    const unsigned long delta = diagnostics.audioHasLastCenteredSample
        ? static_cast<unsigned long>(labs(baselineCorrectedSample - diagnostics.audioLastCenteredSample))
        : 0UL;

    ++diagnostics.audioFrames;
    diagnostics.audioSumSquares += static_cast<uint64_t>(absCentered) * static_cast<uint64_t>(absCentered);
    if (absCentered <= static_cast<unsigned long>(kAudioZeroishAbsThreshold)) {
        ++diagnostics.audioZeroishFrames;
    }
    if (diagnostics.audioHasLastCenteredSample && baselineCorrectedSample == diagnostics.audioLastCenteredSample) {
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
    diagnostics.audioLastCenteredSample = baselineCorrectedSample;
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
        case SeqOutputMode::Detail:
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
        case SeqOutputMode::Detail:
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

bool AnalyzerApp::shouldPrintSequenceDetail(const AnalyzerReport& report) const {
    return _sequenceTest.outputConfig.diagnosticsEnabled &&
           _sequenceTest.outputConfig.mode == SeqOutputMode::Detail &&
           sequenceOutputWhenEnabled(_sequenceTest.outputConfig.when, report.classification.result);
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
        case AnalyzerApp::SeqOutputMode::Detail:
            return "detail";
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
    if (equalsIgnoreCase(token, "detail") || equalsIgnoreCase(token, "explain")) {
        return AnalyzerApp::SeqOutputMode::Detail;
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
      _i2sSource(runtime::kDefaultAudioI2SSckPin,
                  runtime::kDefaultAudioI2SWsPin,
                  runtime::kDefaultAudioI2SDataPin,
                  static_cast<int>(runtime::kDefaultAudioI2SSampleRateHz),
                  static_cast<int>(runtime::kDefaultAudioI2SBitsPerSample)),
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

detection::DetectionProfile const& AnalyzerApp::effectiveSequenceProfile() const {
    //PARAM TUNING TEMPORARY
    _sequenceProfileScratch = detection::detectionProfileForKind(_sequenceTest.profileKind);
    _sequenceProfileScratch.frequencyMatch.attackScoreMin = _analyzerTuning.frequencyMatch.attackScoreMin;
    _sequenceProfileScratch.frequencyMatch.releaseScoreMin = _analyzerTuning.frequencyMatch.releaseScoreMin;
    _sequenceProfileScratch.frequencyMatch.attackContrastMin = _analyzerTuning.frequencyMatch.attackContrastMin;
    _sequenceProfileScratch.frequencyMatch.releaseContrastMin = _analyzerTuning.frequencyMatch.releaseContrastMin;
    if (_sequenceTest.profileKind == detection::DetectionProfileKind::TonalPulseScalar) {
        _sequenceProfileScratch.scalarTransient = _analyzerTuning.scalarTransient;
    }
    return _sequenceProfileScratch;
}

void AnalyzerApp::begin() {
    beginEmitterControl();
    ++g_analyzerBootCount;

    configureParameters();
    _audioSource.begin();
    _audioSignal.begin();
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
    Serial.println("EVT analyzer_help type='HELP', 'PARAM freqScore=18000 freqContrast=50.0 freqReleaseScore=12000 freqReleaseContrast=50.0', 'PARAM STATUS', 'RAW trigger f=3200 dur=100 post=1000 dump=csv|dump=raw|dump=text|dump=chunks|dump=bin', 'SEQ MODE quiet|trial|inspect|source|system|detail WHEN off|miss|all VERBOSE 0|1|2 STATUS REPORT', 'DET PROFILE TonalPulseFreq|TonalPulseScalar|AmpExperimental'");
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
    if (requested == SeqOutputMode::Source) {
        return configured == requested;
    }
    if (requested == SeqOutputMode::Inspect) {
        return configured == requested || configured == SeqOutputMode::Detail;
    }
    if (requested == SeqOutputMode::Detail) {
        return configured == SeqOutputMode::Detail;
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
                _freqBandStream.observeCenteredSample(audioSamplePacket.baselineCorrectedValue, audioSamplePacket.timeMs);
            }
            const bool needSequenceFrequencyPacket = _sequenceTest.sampleDumpEnabled || (_sequenceTest.active && _sequenceTest.currentTrial > 0);
            detection::FrequencyBandMeasurementPacket& runtimeFrequencyMeasurementPacket = _runtimeFrequencyMeasurementPacketScratch;
            runtimeFrequencyMeasurementPacket = {};
            if (needSequenceFrequencyPacket) {
                if (_sequenceTest.outputConfig.frequencyBandEnabled) {
                    runtimeFrequencyMeasurementPacket = captureFrequencyMeasurementPacket(audioSamplePacket);
                } else {
                    runtimeFrequencyMeasurementPacket.observedAtMs = audioSamplePacket.timeMs;
                }
                if (_sequenceTest.sampleDumpEnabled) {
                    const float detectorInputValue = sequenceSampleDumpInputValue(
                        _sequenceTest.sampleDumpDetectorSelection,
                        _sequenceTest.sampleDumpObservedStream,
                        audioSamplePacket,
                        runtimeFrequencyMeasurementPacket
                    );
                    recordSequenceDetectorInputSample(audioSamplePacket.timeMs, detectorInputValue);
                }
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
                _detection.observeFrame(audioSamplePacket, runtimeFrequencyMeasurementPacket, audioSamplePacket.timeMs);
                detection::DetectionPipelineEvent& runtimePipelineEvent = _runtimePipelineEventScratch;
                while (_detection.popPipelineEvent(runtimePipelineEvent)) {
                    if (runtimePipelineEvent.hasPatternResult) {
                        _sequenceTest.currentTrialDiagnostics.runtimePatternCaptured = true;
                    }
                    handleSequencePending(
                        runtimePipelineEvent,
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
    const detection::PatternResult* reportPatternResult = selectedTrial.patternResult;
    const detection::InspectedOccurrence* reportInspectedOccurrence = selectedTrial.inspectedOccurrence;
    const detection::DetectorReport* selectedDetectorReport = selectedTrial.detectorReport;
    const bool hasPatternResult = reportPatternResult != nullptr;
    const bool hasInspectedOccurrence = reportInspectedOccurrence != nullptr && reportInspectedOccurrence->occurrence.present;
    const detection::FieldState* runtimeFieldState = &_detection.fieldState();
    const detection::DetectionProfile& selectedProfile = detection::detectionProfileForKind(_sequenceTest.profileKind);
    report.sourceSelection = selectedTrial.kind == SequenceTrialSelection::Kind::Miss || selectedTrial.kind == SequenceTrialSelection::Kind::Unexpected
        ? "none"
        : (selectedTrial.kind == SequenceTrialSelection::Kind::RejectedSourceCandidate ||
           (reportInspectedOccurrence != nullptr && reportInspectedOccurrence->decision == detection::OccurrenceDecision::Rejected)
            ? "selected_reject"
            : "selected_occurrence");
    report.sourceOccurrenceId = selectedTrial.occurrenceId;
    report.sourceCandidateId = selectedTrial.candidateId;
    report.sourceReportMatched = selectedTrial.reportMatched;
    report.sourceReportReason = detection::analyzer::sourceReportReason(
        selectedTrial.reportMatched,
        selectedDetectorReport != nullptr);
    report.detectorReport = selectedTrial.reportMatched ? selectedDetectorReport : nullptr;
    const bool trialHasPipelineEvidence = (hasPatternResult || hasInspectedOccurrence || _sequenceTest.selectedSourceRejectCaptured)
        && _sequenceTest.sourceCandidateCount > 0;
    const long reportPatternDtMs = hasPatternResult
        ? selectedTrial.dtMs
        : dtMs;
    const unsigned long reportPatternDurationMs = hasPatternResult
        ? selectedTrial.durationMs
        : (durMs >= 0 ? static_cast<unsigned long>(durMs) : 0UL);
    const float reportPatternStrength = hasPatternResult
        ? selectedTrial.strength
        : strength;
    const auto artifactReason = [&]() -> const char* {
        if (hasPatternResult || hasInspectedOccurrence || selectedDetectorReport != nullptr) {
            return "captured_from_runtime_pipeline";
        }
        return "missing_pipeline_result";
    }();
    const bool startupArtifact = result == AnalyzerResult::Miss
        && _sequenceTest.currentTrial == 1
        && !trialHasPipelineEvidence
        && selectedDetectorReport == nullptr
        && strcmp(artifactReason, "missing_pipeline_result") == 0;

    AnalyzerSequenceClassificationInput classificationInput;
    classificationInput.result = result;
    classificationInput.dtMs = reportPatternDtMs;
    classificationInput.sourceCandidateCount = _sequenceTest.sourceCandidateCount;
    classificationInput.sourceAcceptedCount = _sequenceTest.sourceAcceptedCount;
    classificationInput.sourceRejectedCount = _sequenceTest.sourceRejectedCount;
    classificationInput.inspectedOccurrenceCount = _sequenceTest.inspectedOccurrenceCount;
    classificationInput.patternResultCount = _sequenceTest.patternResultCount;
    classificationInput.pipelineQueueOverflowCount = _detection.pipelineEventOverflowCount() > _sequenceTest.trialOverflowCountAtStart
        ? _detection.pipelineEventOverflowCount() - _sequenceTest.trialOverflowCountAtStart
        : 0UL;
    classificationInput.bufferOverrun = bufferOverrun;
    classificationInput.patternAvailable = hasPatternResult;
    classificationInput.detectorReportAvailable = selectedDetectorReport != nullptr;
    classificationInput.detectorAcceptedPresent = selectedDetectorReport != nullptr && selectedDetectorReport->accepted.present;
    classificationInput.detectorSelectedRejectPresent = selectedDetectorReport != nullptr && selectedDetectorReport->selectedReject.present;
    report.classification = classifySequenceTrial(classificationInput);
    if (hasPatternResult && !reportPatternResult->valid) {
        report.classification.result = AnalyzerResult::Rejected;
        report.classification.reason = AnalyzerReason::PatternRejected;
        report.classification.primaryStage = AnalyzerStage::Pattern;
    }
    {
        // Analyzer formats the runtime PatternResult when one exists; it does
        // not synthesize pattern validity from occurrence acceptance.
        AnalyzerPatternObservation pattern = {};
        pattern.type = hasPatternResult ? detection::patternTypeName(reportPatternResult->type) : "none";
        pattern.accepted = hasPatternResult
            ? reportPatternResult->valid
            : false;
        pattern.patternAccepted = hasPatternResult ? reportPatternResult->patternAccepted : false;
        pattern.patternMatched = hasPatternResult ? reportPatternResult->patternMatched : false;
        pattern.supportMatched = hasPatternResult ? reportPatternResult->supportMatched : false;
        pattern.behaviorEligible = pattern.accepted;
        pattern.confidence = hasPatternResult ? reportPatternResult->confidence : 0.0f;
        pattern.dtMs = report.classification.dtMs;
        pattern.supportStrength = hasInspectedOccurrence
            ? detection::strengthClassName(reportInspectedOccurrence->occurrence.scalar.strengthClass)
            : "unknown";
        pattern.reason = hasPatternResult ? detection::patternReasonName(reportPatternResult->reasonCode) : "none";
        pattern.rejectReason = hasPatternResult ? detection::patternRejectReasonName(reportPatternResult->rejectReason) : "none";
        pattern.firstFailedTarget = hasPatternResult
            ? reportPatternResult->firstFailedRequirementTarget
            : detection::InspectionTarget::None;
        pattern.firstFailedObservedStrength = hasPatternResult
            ? detection::strengthClassName(reportPatternResult->firstFailedObservedStrength)
            : "unknown";
        pattern.firstFailedRequiredStrength = hasPatternResult
            ? detection::strengthClassName(reportPatternResult->firstFailedRequiredStrength)
            : "unknown";
        pattern.firstFailedRequirementIndex = hasPatternResult
            ? reportPatternResult->firstFailedRequirementIndex
            : 255U;
        pattern.involvedOccurrences = hasPatternResult ? reportPatternResult->occurrenceCount : 0U;
        report.primaryPattern = pattern;
    }

    report.occurrences.accepted = selectedTrial.kind == SequenceTrialSelection::Kind::ValidPattern ||
        selectedTrial.kind == SequenceTrialSelection::Kind::AcceptedOccurrence ||
        (selectedTrial.kind == SequenceTrialSelection::Kind::RejectedPattern &&
         reportInspectedOccurrence != nullptr &&
         reportInspectedOccurrence->decision == detection::OccurrenceDecision::Accepted)
        ? 1U
        : 0U;
    report.occurrences.rejected = (selectedTrial.kind == SequenceTrialSelection::Kind::RejectedPattern &&
        reportInspectedOccurrence != nullptr &&
        reportInspectedOccurrence->decision == detection::OccurrenceDecision::Rejected) ||
        selectedTrial.kind == SequenceTrialSelection::Kind::RejectedSourceCandidate
        ? 1U
        : 0U;
    report.occurrences.total = report.occurrences.accepted + report.occurrences.rejected;
    if (trialHasPipelineEvidence && hasInspectedOccurrence) {
        const detection::Occurrence& occurrence = reportInspectedOccurrence->occurrence;
        report.occurrences.kind = detection::occurrenceTypeName(occurrence.occurrenceType);
        report.occurrences.primarySource = detection::occurrenceSourceName(occurrence.detectorId);
        report.occurrences.detectorKind = detection::detectorIdName(occurrence.detectorId);
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
        report.occurrences.confidence = hasPatternResult ? reportPatternResult->confidence : occurrence.confidence;
        report.occurrences.mainRejectReason = reportInspectedOccurrence->decision == detection::OccurrenceDecision::Rejected
            ? detection::occurrenceRejectReasonName(reportInspectedOccurrence->rejectReason)
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
        if (selectedTrial.kind == SequenceTrialSelection::Kind::RejectedSourceCandidate &&
            selectedDetectorReport != nullptr &&
            selectedDetectorReport->selectedReject.present) {
            report.occurrences.primaryDtMs = selectedTrial.dtMs;
            report.occurrences.primaryDurationMs = selectedTrial.durationMs;
            report.occurrences.primaryStrength = selectedTrial.strength;
            report.occurrences.kind = detection::occurrenceTypeName(detection::OccurrenceType::Scalar);
            report.occurrences.primarySource = detection::occurrenceSourceName(selectedDetectorReport->detectorId);
            report.occurrences.detectorKind = detection::detectorIdName(selectedDetectorReport->detectorId);
        }
        report.occurrences.contrast = 0.0f;
        report.occurrences.strength = selectedTrial.kind == SequenceTrialSelection::Kind::RejectedSourceCandidate
            ? selectedTrial.strength
            : reportPatternStrength;
        report.occurrences.confidence = hasPatternResult ? reportPatternResult->confidence : 0.0f;
        if (selectedTrial.kind == SequenceTrialSelection::Kind::RejectedSourceCandidate &&
            selectedDetectorReport != nullptr &&
            selectedDetectorReport->selectedReject.present &&
            selectedDetectorReport->selectedReject.detectorReason != nullptr &&
            selectedDetectorReport->selectedReject.detectorReason[0] != '\0') {
            report.occurrences.mainRejectReason = selectedDetectorReport->selectedReject.detectorReason;
            report.occurrences.rejectReason = selectedDetectorReport->selectedReject.detectorReason;
        } else {
            report.occurrences.mainRejectReason = report.sourceReportReason;
            report.occurrences.rejectReason = report.occurrences.mainRejectReason;
        }
    }

    report.inspection.inspected = _sequenceTest.inspectedOccurrenceCount;
    report.inspection.accepted = report.occurrences.accepted;
    report.inspection.rejected = report.occurrences.rejected;
    const detection::InspectionModuleConfig* supportRequirement =
        detection::patternMatcherFirstEnabledRequirement(selectedProfile.inspectionPlan);
    if (trialHasPipelineEvidence && hasInspectedOccurrence) {
        report.inspection.primaryEvidence = detection::occurrenceSourceName(reportInspectedOccurrence->occurrence.detectorId);
        report.inspection.moduleTarget = supportRequirement != nullptr ? supportRequirement->target : detection::InspectionTarget::None;
        if (report.inspection.moduleTarget == detection::InspectionTarget::TargetScore) {
            report.inspection.moduleStrengthClass = detection::strengthClassName(reportInspectedOccurrence->occurrence.frequency.scoreStrength);
        } else if (report.inspection.moduleTarget == detection::InspectionTarget::Contrast) {
            report.inspection.moduleStrengthClass = detection::strengthClassName(reportInspectedOccurrence->occurrence.frequency.contrastQuality);
        } else {
            report.inspection.moduleStrengthClass = detection::strengthClassName(reportInspectedOccurrence->occurrence.scalar.strengthClass);
        }
        report.inspection.mainRejectReason = reportInspectedOccurrence->decision == detection::OccurrenceDecision::Rejected ? detection::occurrenceRejectReasonName(reportInspectedOccurrence->rejectReason) : "none";
    } else {
        report.inspection.primaryEvidence = "none";
        report.inspection.moduleTarget = detection::InspectionTarget::None;
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
            : _sequenceTest.sourceRejectedCount;
    } else {
        report.field.state = "unknown";
        report.field.rawActivity = 0.0f;
        report.field.validPatternActivity = 0.0f;
        report.field.recentValidPatterns = 0U;
        report.field.recentRejects = _sequenceTest.sourceRejectedCount;
    }

    report.profileDetail.namespaceName = analyzerProfileDetailNamespace(_sequenceTest.profileKind);
    report.profileDetail.summary = analyzerProfileDetailSummary(_sequenceTest.profileKind);
    report.profileDetail.emitter = detection::detectorSelectionName(selectedProfile.detectorSelection);
    report.profileDetail.inspectionAcceptance = detection::detectorSelectionName(selectedProfile.detectorSelection);
    report.profileDetail.inspectionPlan = detection::inspectionPlanName(selectedProfile.inspectionPlan);
    report.profileDetail.inspectionModules = detection::inspectionModulesName(selectedProfile.inspectionPlan);
    report.profileDetail.inspectionModuleCount = selectedProfile.inspectionPlan.count;
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
            report.profileDetail.inspectionObservationTargets[i] = selectedProfile.inspectionPlan.modules[i].target;
            report.profileDetail.inspectionObservations[i] = reportInspectedOccurrence->scalarObservations[i];
        }
    }

    report.debug.occurrences = _sequenceTest.sourceCandidateCount;
    report.debug.inspected = _sequenceTest.inspectedOccurrenceCount;
    report.debug.patterns = _sequenceTest.patternResultCount;
    report.debug.rejects = report.occurrences.rejected;
    report.debug.duplicates = duplicateCount;
    report.debug.unexpected = result == AnalyzerResult::Unexpected ? 1U : 0U;
    report.debug.pipelineQueueOverflows = classificationInput.pipelineQueueOverflowCount;
    report.debug.patternResultQueueOverflows = _detection.patternResultQueueOverflowCount() > _sequenceTest.trialPatternResultOverflowCountAtStart
        ? _detection.patternResultQueueOverflowCount() - _sequenceTest.trialPatternResultOverflowCountAtStart
        : 0UL;
    report.debug.patternInspectedQueueOverflows = _detection.patternInspectedQueueOverflowCount() > _sequenceTest.trialPatternInspectedOverflowCountAtStart
        ? _detection.patternInspectedQueueOverflowCount() - _sequenceTest.trialPatternInspectedOverflowCountAtStart
        : 0UL;
    report.debug.startupArtifact = startupArtifact;
    report.debug.bufferOverrun = bufferOverrun;
    report.debug.artifactCaptured = trialHasPipelineEvidence;
    report.debug.artifactFallback = !trialHasPipelineEvidence;
    report.debug.artifactState = startupArtifact ? "STARTUP_ARTIFACT" : (trialHasPipelineEvidence ? "CAPTURED" : "MISSING_PIPELINE");
    report.debug.artifactReason = artifactReason;
    report.debug.pipelineSource = trialHasPipelineEvidence ? "actual_pipeline" : "missing_runtime_pipeline";
    report.debug.pipelineFallback = !trialHasPipelineEvidence;
    report.debug.mainRejectReason = trialHasPipelineEvidence && reportInspectedOccurrence != nullptr
        ? (reportInspectedOccurrence->decision == detection::OccurrenceDecision::Rejected ? detection::occurrenceRejectReasonName(reportInspectedOccurrence->rejectReason) : "none")
        : analyzerReasonName(report.classification.reason);
}










