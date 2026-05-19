#include "AnalyzerApp.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#include "../../AudioDebugConfig.h"
#include "../../detection/DetectorParameters.h"
#include "../../detection/inspector/FrequencyEvidenceEvaluation.h"
#include "../../detection/inspector/FrequencyWindowProbe.h"
#include "../../detection/features/FeatureExtractor.h"
#include "../../detection/features/FeatureHistory.h"
#include "../../detection/signals/RawWindow.h"
#include "../../detection/patterns/PatternAssembler.h"
#include "../../detection/patterns/PatternNames.h"
#include "../../detection/patterns/PatternRules.h"
#include "../../detection/signals/SignalCandidate.h"
#include "../../detection/inspector/SignalInspector.h"

/*
AnalyzerApp

This file owns analyzer-mode orchestration, not the detector internals.

File structure:
- local utility helpers
- construction and setup
- runtime loop and detector state
- console and emitter control
- raw-trigger and value-mode helpers
- sequence, capture, and base sessions
- diagnostics and summary output
*/
namespace {
constexpr int kMaxSamplesPerLoop = 128;
constexpr unsigned long kSequenceWarmupMs = 500;
constexpr unsigned long kRawCaptureFlushSamples = 256;
constexpr unsigned long kRawCaptureTimeoutSlackMs = 2000;
constexpr long kLateOnsetMinMs = 200L;
constexpr long kCleanDurationMinMs = 80L;
constexpr long kCleanDurationMaxMs = 180L;
constexpr long kSmearedDurationMinMs = 181L;
constexpr long kSmearedDurationMaxMs = 240L;
constexpr long kTooLongDurationMinMs = 241L;
constexpr long kNearMaxDurationMinMs = 220L;
constexpr unsigned long kLiveFrequencyReleaseDebounceMs = 20UL;
constexpr unsigned long kLiveFrequencyCooldownAfterOnsetMs = 300UL;
constexpr unsigned long kLiveFrequencyMinTransientDurationMs = 50UL;

bool startsWithToken(const char* line, const char* token) {
    return strncmp(line, token, strlen(token)) == 0;
}

bool equalsIgnoreCase(const char* a, const char* b) {
    while (*a != '\0' && *b != '\0') {
        if (toupper(static_cast<unsigned char>(*a)) != toupper(static_cast<unsigned char>(*b))) {
            return false;
        }
        ++a;
        ++b;
    }

    return *a == '\0' && *b == '\0';
}

bool startsWithTokenIgnoreCase(const char* line, const char* token) {
    while (*token != '\0') {
        if (*line == '\0') {
            return false;
        }
        if (toupper(static_cast<unsigned char>(*line)) != toupper(static_cast<unsigned char>(*token))) {
            return false;
        }
        ++line;
        ++token;
    }

    return true;
}

bool analyzerLogEnabled(uint32_t flags, AnalyzerApp::AnalyzerLogFlags flag) {
    return (flags & static_cast<uint32_t>(flag)) != 0;
}

void buildFrequencyFailReason(const detection::FrequencyEvidence& evidence,
                              const FrequencyEvidenceEvaluation::Values& tuning,
                              char* out,
                              size_t outSize) {
    FrequencyEvidenceEvaluation::buildFailReason(evidence, tuning, out, outSize);
}

const char* liveFrequencyGateReason(bool readyOk, const FrequencyEvidenceEvaluation::Evaluation& eval) {
    if (!readyOk) {
        return "live_window_not_ready";
    }
    if (!eval.present) {
        return "no_frequency_evidence";
    }
    if (!eval.validWindow) {
        return "frequency_window_invalid";
    }
    if (!eval.scoreOk && !eval.contrastOk) {
        return "freq_score_and_contrast_too_low";
    }
    if (!eval.scoreOk) {
        return "freq_score_too_low";
    }
    if (!eval.contrastOk) {
        return "freq_contrast_too_low";
    }
    return "none";
}

const char* sequenceFrequencyCandidateSourceName(bool frequencyValid) {
    if (frequencyValid) {
        return "frequency_primary";
    }
    return "comparison_only";
}

const char* signalKindName(detection::SignalKind kind) {
    switch (kind) {
        case detection::SignalKind::AmpTransient:
            return "amp_transient";
        case detection::SignalKind::FrequencyMatch:
            return "frequency_match";
        case detection::SignalKind::BroadbandTransient:
            return "broadband_transient";
        case detection::SignalKind::None:
        default:
            return "none";
    }
}

const char* signalSourceName(detection::SignalSource source) {
    switch (source) {
        case detection::SignalSource::Amp:
            return "amp";
        case detection::SignalSource::Frequency:
            return "frequency";
        case detection::SignalSource::Broadband:
            return "broadband";
        case detection::SignalSource::None:
        default:
            return "none";
    }
}

const char* signalDetectorKindName(detection::SignalDetectorKind kind) {
    switch (kind) {
        case detection::SignalDetectorKind::Transient:
            return "transient";
        case detection::SignalDetectorKind::FrequencyMatch:
            return "frequency_match";
        case detection::SignalDetectorKind::Dip:
            return "dip";
        case detection::SignalDetectorKind::Plateau:
            return "plateau";
        case detection::SignalDetectorKind::ThresholdCrossing:
            return "threshold_crossing";
        case detection::SignalDetectorKind::Unknown:
        default:
            return "unknown";
    }
}

const char* signalRejectReasonName(detection::SignalRejectReason reason) {
    switch (reason) {
        case detection::SignalRejectReason::None:
            return "none";
        case detection::SignalRejectReason::TooShort:
            return "too_short";
        case detection::SignalRejectReason::TooLong:
            return "too_long";
        case detection::SignalRejectReason::TooWeak:
            return "too_weak";
        case detection::SignalRejectReason::BelowThreshold:
            return "below_threshold";
        case detection::SignalRejectReason::DuplicateRisk:
            return "duplicate_risk";
        case detection::SignalRejectReason::Cooldown:
            return "cooldown";
        case detection::SignalRejectReason::MissingFrequencyEvidence:
            return "missing_frequency_evidence";
        case detection::SignalRejectReason::MissingAmpSupport:
            return "missing_amp_support";
        case detection::SignalRejectReason::InvalidTiming:
            return "invalid_timing";
        case detection::SignalRejectReason::UnsupportedKind:
            return "unsupported_kind";
        case detection::SignalRejectReason::Unknown:
        default:
            return "unknown";
    }
}

const char* ampSupportName(detection::AmpSupportClass value) {
    switch (value) {
        case detection::AmpSupportClass::None:
            return "none";
        case detection::AmpSupportClass::Weak:
            return "weak";
        case detection::AmpSupportClass::Medium:
            return "medium";
        case detection::AmpSupportClass::Strong:
            return "strong";
        case detection::AmpSupportClass::Unknown:
        default:
            return "unknown";
    }
}

detection::SignalCandidate makeModernFrequencySignalCandidate(const FrequencyMatchDetector& liveFrequency) {
    detection::SignalCandidate signal = {};
    signal.kind = detection::SignalKind::FrequencyMatch;
    signal.source = detection::SignalSource::Frequency;
    signal.detectorKind = detection::SignalDetectorKind::FrequencyMatch;
    signal.present = liveFrequency.present;
    signal.valid = liveFrequency.frequencyCandidate.valid;
    signal.startSample = liveFrequency.frequencyCandidate.startSample;
    signal.peakSample = liveFrequency.frequencyCandidate.peakSample;
    signal.releaseSample = liveFrequency.frequencyCandidate.releaseSample;
    signal.startMs = liveFrequency.frequencyCandidate.startMs;
    signal.peakMs = liveFrequency.frequencyCandidate.peakMs;
    signal.releaseMs = liveFrequency.frequencyCandidate.releaseMs;
    signal.endMs = signal.releaseMs;
    signal.durationMs = liveFrequency.frequencyCandidate.durationMs;
    signal.strength = liveFrequency.frequencyCandidate.strength;
    signal.score = liveFrequency.frequencyCandidate.score;
    signal.contrast = liveFrequency.frequencyCandidate.contrast;
    signal.confidence = signal.valid ? 1.0f : 0.0f;
    signal.signalConfidence = signal.confidence;
    signal.frequencyConfidence = signal.valid ? 1.0f : 0.0f;
    signal.ampEvidencePresent = true;
    // Observation-only AMP snapshot for FrequencyFirst candidates.
    signal.frequency = liveFrequency.candidateEvidence.present ? liveFrequency.candidateEvidence : liveFrequency.bestEvidence;
    return signal;
}

bool isModernDetectorCandidateAccepted(const DetectorCandidate& in) {
    return in.durationMs > 0 || in.peakStrength > 0.0f || in.releaseMillisApprox != 0;
}

detection::PatternCandidate makeModernPatternCandidate(const DetectorCandidate& in) {
    detection::PatternCandidate out;
    out.kind = detection::PatternCandidateKind::SinglePulse;
    out.lineageId = static_cast<uint32_t>(in.onsetSample & 0xFFFFFFFFu);
    out.primarySlotIndex = 0;
    out.onsetSample = in.onsetSample;
    out.peakSample = in.peakSample;
    out.releaseSample = in.releaseSample;
    out.startMs = in.onsetMillisApprox;
    out.heardAtMs = in.releaseMillisApprox != 0 ? in.releaseMillisApprox : in.onsetMillisApprox;
    out.acceptedMs = out.heardAtMs;
    out.durationMs = in.durationMs;
    out.onsetStrength = in.onsetStrength;
    out.peakStrength = in.peakStrength;
    out.releaseStrength = in.releaseStrength;
    out.ambientBaseline = in.ambientBaseline;
    out.audioOverflowDuringCandidate = in.audioOverflowDuringCandidate;
    out.transient.present = isModernDetectorCandidateAccepted(in);
    out.transient.onsetSample = in.onsetSample;
    out.transient.peakSample = in.peakSample;
    out.transient.releaseSample = in.releaseSample;
    out.transient.startMs = in.onsetMillisApprox;
    out.transient.heardAtMs = in.releaseMillisApprox != 0 ? in.releaseMillisApprox : in.onsetMillisApprox;
    out.transient.acceptedMs = out.transient.heardAtMs;
    out.transient.durationMs = in.durationMs;
    out.transient.onsetStrength = in.onsetStrength;
    out.transient.peakStrength = in.peakStrength;
    out.transient.releaseStrength = in.releaseStrength;
    out.transient.ambientBaseline = in.ambientBaseline;
    out.transient.audioOverflowDuringCandidate = in.audioOverflowDuringCandidate;
    out.frequency = {};
    out.frequencyFull = {};
    return out;
}

detection::SignalCandidate makeModernSignalCandidateFromPatternResult(const detection::PatternResult& patternResult) {
    detection::SignalCandidate out;
    out.present = true;
    out.valid = patternResult.valid;
    out.kind = patternResult.source == detection::PatternSource::FrequencyPrimary
        ? detection::SignalKind::FrequencyMatch
        : detection::SignalKind::AmpTransient;
    out.source = patternResult.source == detection::PatternSource::FrequencyPrimary
        ? detection::SignalSource::Frequency
        : detection::SignalSource::Amp;
    out.detectorKind = patternResult.source == detection::PatternSource::FrequencyPrimary
        ? detection::SignalDetectorKind::FrequencyMatch
        : detection::SignalDetectorKind::Transient;
    out.startSample = patternResult.candidate.onsetSample;
    out.peakSample = patternResult.candidate.peakSample;
    out.releaseSample = patternResult.candidate.releaseSample;
    out.startMs = patternResult.candidate.startMs;
    out.peakMs = patternResult.candidate.heardAtMs;
    out.releaseMs = patternResult.candidate.acceptedMs;
    out.endMs = patternResult.candidate.acceptedMs;
    out.durationMs = patternResult.candidate.durationMs;
    out.strength = patternResult.candidate.peakStrength;
    out.score = patternResult.freq.score;
    out.contrast = patternResult.freq.spectralContrast;
    out.confidence = patternResult.confidence;
    out.signalConfidence = patternResult.signalConfidence;
    out.frequencyConfidence = patternResult.frequencyConfidence;
    out.ampLevel = patternResult.candidate.peakStrength;
    out.ampBaseline = patternResult.candidate.ambientBaseline;
    out.ampEvidencePresent = true;
    out.ampSupport = patternResult.candidate.ampSupport;
    out.ampWindow = patternResult.ampWindow;
    out.duplicateRisk = patternResult.duplicateRisk;
    out.duplicateRiskScore = patternResult.duplicateRiskScore;
    out.transient = patternResult.candidate.transient;
    out.frequency = patternResult.freq;
    return out;
}

bool processModernDetectorCandidate(const DetectorCandidate& in,
                                     detection::PatternResult& out,
                                     unsigned long processedAtMs,
                                     const detection::FrequencyEvidence* frequencyEvidence) {
    out = {};
    out.source = detection::PatternSource::ComparisonOnly;
    out.kind = detection::PatternResultKind::Rejected;
    out.lineageId = static_cast<uint32_t>(in.onsetSample & 0xFFFFFFFFu);
    out.primarySlotIndex = 0;
    out.candidate = makeModernPatternCandidate(in);
    out.freq = out.candidate.frequency;
    out.freqFull = out.candidate.frequencyFull;
    out.processedAtMs = processedAtMs;

    if (frequencyEvidence != nullptr) {
        out.candidate.frequency = *frequencyEvidence;
    }

    if (!isModernDetectorCandidateAccepted(in)) {
        out.kind = detection::PatternResultKind::Rejected;
        out.type = detection::PatternType::Invalid;
        out.reasonCode = detection::PatternReasonCode::DetectorRejected;
        out.rejectReason = detection::PatternRejectReason::NoCandidate;
        out.confidence = 0.0f;
        out.candidateValid = false;
        out.tonalValid = false;
        out.behaviorEligible = false;
        out.valid = false;
        return false;
    }

    out.type = detection::PatternType::ValidTransient;
    out.kind = detection::PatternResultKind::Residual;
    out.reasonCode = detection::PatternReasonCode::FromAcceptedTransient;
    out.rejectReason = detection::PatternRejectReason::None;
    out.confidence = 1.0f;
    out.candidateValid = true;
    out.tonalValid = false;
    out.behaviorEligible = false;
    out.valid = true;
    return true;
}

bool evaluateModernSignalCandidateImpl(const detection::SignalCandidate& signal,
                                        const FrequencyEvidenceEvaluation::Values& tuning,
                                        const detection::FeatureHistory* featureHistory,
                                        detection::PatternResult& outResult,
                                        detection::InspectedSignal* outInspected,
                                        bool traceStages) {
    static detection::SignalInspector* inspector = nullptr;
    static detection::PatternAssembler* assembler = nullptr;
    if (inspector == nullptr) {
        inspector = new detection::SignalInspector();
    }
    if (assembler == nullptr) {
        assembler = new detection::PatternAssembler();
    }
    detection::PatternRules rules;

    inspector->reset();
    assembler->reset();
    const detection::InspectedSignal inspected = inspector->inspectWithHistory(signal, tuning, featureHistory, nullptr);
    if (outInspected != nullptr) {
        *outInspected = inspected;
    }
    if (traceStages) {
        const unsigned long probeStartMs = signal.startMs > 20UL ? signal.startMs - 20UL : 0UL;
        const unsigned long probeEndMs = signal.endMs != 0
            ? signal.endMs + 20UL
            : (signal.releaseMs != 0 ? signal.releaseMs + 20UL : signal.peakMs + 20UL);
        const size_t ampSampleCount = featureHistory != nullptr
            ? featureHistory->sampleCount(detection::FeatureStreamId::AmpEnvelope)
            : 0U;
        const size_t floorSampleCount = featureHistory != nullptr
            ? featureHistory->sampleCount(detection::FeatureStreamId::AmbientFloor)
            : 0U;
        const unsigned long ampLatestMs = featureHistory != nullptr
            ? featureHistory->latestTimeMs(detection::FeatureStreamId::AmpEnvelope)
            : 0UL;
        const unsigned long floorLatestMs = featureHistory != nullptr
            ? featureHistory->latestTimeMs(detection::FeatureStreamId::AmbientFloor)
            : 0UL;
        const detection::ScalarWindow ampWindow = featureHistory != nullptr
            ? featureHistory->getWindow(detection::FeatureStreamId::AmpEnvelope, probeStartMs, probeEndMs)
            : detection::ScalarWindow{};
        const detection::ScalarWindow floorWindow = featureHistory != nullptr
            ? featureHistory->getWindow(detection::FeatureStreamId::AmbientFloor, probeStartMs, probeEndMs)
            : detection::ScalarWindow{};

        Serial.print("SEQ_TRACE stage=SIGNAL signal=");
        Serial.print(signal.present ? 1 : 0);
        Serial.print(" kind=");
        Serial.print(signalKindName(signal.kind));
        Serial.print(" source=");
        Serial.print(signalSourceName(signal.source));
        Serial.print(" detector=");
        Serial.print(signalDetectorKindName(signal.detectorKind));
        Serial.print(" start_ms=");
        Serial.print(signal.startMs);
        Serial.print(" peak_ms=");
        Serial.print(signal.peakMs);
        Serial.print(" end_ms=");
        Serial.print(signal.endMs);
        Serial.print(" duration_ms=");
        Serial.print(signal.durationMs);
        Serial.print(" score=");
        Serial.print(signal.score, 1);
        Serial.print(" contrast=");
        Serial.print(signal.contrast, 2);
        Serial.print(" signal_confidence=");
        Serial.print(signal.signalConfidence, 2);
        Serial.print(" freq_confidence=");
        Serial.print(signal.frequencyConfidence, 2);
        Serial.print(" confidence=");
        Serial.print(signal.confidence, 2);
        Serial.print(" amp_evidence=");
        Serial.print(signal.ampEvidencePresent ? 1 : 0);
        Serial.print(" amp_level=");
        Serial.print(signal.ampLevel, 1);
        Serial.print(" amp_baseline=");
        Serial.print(signal.ampBaseline, 1);
        Serial.print(" raw_window=");
        Serial.print(0);
        Serial.print(" feature_history_amp_samples=");
        Serial.print(ampSampleCount);
        Serial.print(" feature_history_floor_samples=");
        Serial.print(floorSampleCount);
        Serial.print(" feature_history_amp_latest_ms=");
        Serial.print(ampLatestMs);
        Serial.print(" feature_history_floor_latest_ms=");
        Serial.print(floorLatestMs);
        Serial.print(" amp_window_valid=");
        Serial.print(ampWindow.valid ? 1 : 0);
        Serial.print(" floor_window_valid=");
        Serial.print(floorWindow.valid ? 1 : 0);
        Serial.println();
    }
    if (traceStages) {
        Serial.print("SEQ_TRACE stage=INSPECTED accepted=");
        Serial.print(inspected.accepted ? 1 : 0);
        Serial.print(" rejected=");
        Serial.print(inspected.rejected ? 1 : 0);
        Serial.print(" reject_reason=");
        Serial.print(signalRejectReasonName(inspected.rejectReason));
        Serial.print(" duration_ms=");
        Serial.print(inspected.durationMs);
        Serial.print(" strength=");
        Serial.print(inspected.strength, 1);
        Serial.print(" signal_confidence=");
        Serial.print(inspected.signalConfidence, 2);
        Serial.print(" frequency_confidence=");
        Serial.print(inspected.frequencyConfidence, 2);
        Serial.print(" confidence=");
        Serial.print(inspected.confidence, 2);
        Serial.print(" source=");
        Serial.print(signalSourceName(inspected.signal.source));
        Serial.print(" amp_support=");
        Serial.print(ampSupportName(inspected.ampSupport));
        Serial.print(" duplicate_risk=");
        Serial.print(inspected.duplicateRisk ? 1 : 0);
        Serial.print(" duplicate_risk_score=");
        Serial.print(inspected.duplicateRiskScore, 2);
        Serial.print(" window_source=");
        Serial.print(featureHistory != nullptr ? "history" : "snapshot");
        Serial.println();
    }
    assembler->acceptSignal(inspected);

    detection::PatternCandidate candidate = {};
    if (!assembler->popPatternCandidate(candidate)) {
        if (traceStages) {
            Serial.println("SEQ_TRACE stage=PATTERN_CANDIDATE present=0");
        }
        outResult = {};
        outResult.candidate.frequency = signal.frequency;
        outResult.candidate.frequencyFull = signal.frequency;
        outResult.candidate.transient.present = signal.present;
        outResult.candidate.transient.durationMs = signal.durationMs;
        outResult.freq = signal.frequency;
        outResult.freqFull = signal.frequency;
        outResult.valid = false;
        outResult.candidateValid = false;
        outResult.tonalValid = false;
        outResult.behaviorEligible = false;
        outResult.source = detection::PatternSource::ComparisonOnly;
        outResult.type = detection::PatternType::Invalid;
        outResult.reasonCode = detection::PatternReasonCode::DetectorRejected;
        outResult.rejectReason = detection::PatternRejectReason::NoCandidate;
        if (traceStages) {
            Serial.println("SEQ_TRACE stage=PATTERN_RESULT valid=0 source=comparison_only reject=no_candidate");
        }
        return false;
    }

    if (traceStages) {
        Serial.print("SEQ_TRACE stage=PATTERN_CANDIDATE present=1 source=");
        Serial.print(candidate.frequency.present ? "frequency" : candidate.transient.present ? "amp" : "none");
        Serial.print(" kind=");
        Serial.print(detection::patternCandidateKindName(candidate.kind));
        Serial.print(" signal_count=");
        Serial.print(candidate.signalCount);
        Serial.print(" pulse_count=");
        Serial.print(candidate.pulseCount);
        Serial.print(" first_pulse_ms=");
        Serial.print(candidate.firstPulseMs);
        Serial.print(" last_pulse_ms=");
        Serial.print(candidate.lastPulseMs);
        Serial.print(" duration_ms=");
        Serial.print(candidate.durationMs);
        Serial.print(" amp_support=");
        Serial.print(ampSupportName(candidate.ampSupport));
        Serial.print(" signal_confidence=");
        Serial.print(candidate.signalConfidence, 2);
        Serial.print(" frequency_confidence=");
        Serial.print(candidate.frequencyConfidence, 2);
        Serial.print(" window_source=");
        Serial.print(featureHistory != nullptr ? "history" : "snapshot");
        Serial.println();
    }
    outResult = rules.evaluate(candidate, signal.releaseMs != 0 ? signal.releaseMs : signal.peakMs, tuning);
    if (traceStages) {
        Serial.print("SEQ_TRACE stage=PATTERN_RESULT valid=");
        Serial.print(outResult.valid ? 1 : 0);
        Serial.print(" kind=");
        Serial.print(detection::patternResultKindName(outResult.kind));
        Serial.print(" signal_count=");
        Serial.print(outResult.signalCount);
        Serial.print(" pulse_count=");
        Serial.print(outResult.pulseCount);
        Serial.print(" first_pulse_ms=");
        Serial.print(outResult.firstPulseMs);
        Serial.print(" last_pulse_ms=");
        Serial.print(outResult.lastPulseMs);
        Serial.print(" type=");
        Serial.print(detection::patternTypeName(outResult.type));
        Serial.print(" source=");
        Serial.print(detection::patternSourceName(outResult.source));
        Serial.print(" reject=");
        Serial.print(detection::patternRejectReasonName(outResult.rejectReason));
        Serial.print(" amp_support=");
        Serial.print(ampSupportName(outResult.ampSupport));
        Serial.print(" signal_confidence=");
        Serial.print(outResult.signalConfidence, 2);
        Serial.print(" frequency_confidence=");
        Serial.print(outResult.frequencyConfidence, 2);
        Serial.print(" window_source=");
        Serial.print(featureHistory != nullptr ? "history" : "snapshot");
        Serial.println();
    }
    return true;
}

int16_t rawCaptureSampleToInt16(int sample) {
    const int32_t shifted = static_cast<int32_t>(sample) >> 16;
    if (shifted > 32767) {
        return 32767;
    }
    if (shifted < -32768) {
        return -32768;
    }
    return static_cast<int16_t>(shifted);
}

unsigned long rawCaptureChunkSize(unsigned long sampleRateHz, unsigned long decim) {
    const unsigned long baseChunk = sampleRateHz / 20UL;
    const unsigned long decimatedChunk = decim > 0 ? baseChunk / decim : baseChunk;
    return decimatedChunk > 0 ? decimatedChunk : 1UL;
}

unsigned long countSelectedSampleDumpTrials(unsigned long totalTrials, unsigned long firstTrials, unsigned long everyNth) {
    unsigned long selected = 0;
    for (unsigned long trial = 1; trial <= totalTrials; ++trial) {
        const bool firstSelected = firstTrials > 0 && trial <= firstTrials;
        const bool everySelected = everyNth > 0 && trial % everyNth == 0;
        if (firstSelected || everySelected) {
            ++selected;
        }
    }
    return selected;
}

uint32_t analyzerLogFlagsFromLevel(unsigned long level) {
    if (level == 0) {
        return AnalyzerApp::ANALYZER_LOG_NONE;
    }
    if (level == 1) {
        return AnalyzerApp::DEFAULT_ANALYZER_LOG_FLAGS;
    }
    return AnalyzerApp::ANALYZER_LOG_SUMMARY |
           AnalyzerApp::ANALYZER_LOG_TRIAL |
           AnalyzerApp::ANALYZER_LOG_CANDIDATE |
           AnalyzerApp::ANALYZER_LOG_FREQ_CLASS |
           AnalyzerApp::ANALYZER_LOG_EXPLAIN;
}

uint32_t analyzerLogFlagsFromToken(const char* token) {
    if (token == nullptr || *token == '\0') {
        return AnalyzerApp::DEFAULT_ANALYZER_LOG_FLAGS;
    }

    if (equalsIgnoreCase(token, "default")) {
        return AnalyzerApp::DEFAULT_ANALYZER_LOG_FLAGS;
    }
    if (equalsIgnoreCase(token, "quiet") || equalsIgnoreCase(token, "none")) {
        return AnalyzerApp::ANALYZER_LOG_NONE;
    }
    if (equalsIgnoreCase(token, "full")) {
        return AnalyzerApp::ANALYZER_LOG_SUMMARY |
               AnalyzerApp::ANALYZER_LOG_TRIAL |
               AnalyzerApp::ANALYZER_LOG_CANDIDATE |
               AnalyzerApp::ANALYZER_LOG_FREQ_CLASS |
               AnalyzerApp::ANALYZER_LOG_EXPLAIN;
    }

    char buffer[64];
    strncpy(buffer, token, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    uint32_t flags = AnalyzerApp::ANALYZER_LOG_NONE;
    char* savePtr = nullptr;
    char* part = strtok_r(buffer, ",+|", &savePtr);
    while (part != nullptr) {
        if (equalsIgnoreCase(part, "summary")) {
            flags |= AnalyzerApp::ANALYZER_LOG_SUMMARY;
        } else if (equalsIgnoreCase(part, "trial")) {
            flags |= AnalyzerApp::ANALYZER_LOG_TRIAL;
        } else if (equalsIgnoreCase(part, "candidate")) {
            flags |= AnalyzerApp::ANALYZER_LOG_CANDIDATE;
        } else if (equalsIgnoreCase(part, "report") || equalsIgnoreCase(part, "freq_class") || equalsIgnoreCase(part, "freq")) {
            flags |= AnalyzerApp::ANALYZER_LOG_FREQ_CLASS;
        } else if (equalsIgnoreCase(part, "explain") || equalsIgnoreCase(part, "liveraw") || equalsIgnoreCase(part, "raw_debug") || equalsIgnoreCase(part, "raw")) {
            flags |= AnalyzerApp::ANALYZER_LOG_EXPLAIN;
        } else if (equalsIgnoreCase(part, "custom") || equalsIgnoreCase(part, "ampwindow") || equalsIgnoreCase(part, "amp_window") || equalsIgnoreCase(part, "window")) {
            flags |= AnalyzerApp::ANALYZER_LOG_CUSTOM;
        } else if (equalsIgnoreCase(part, "trialbrief") || equalsIgnoreCase(part, "triallite") || equalsIgnoreCase(part, "brief")) {
            flags |= AnalyzerApp::ANALYZER_LOG_TRIAL;
            flags |= AnalyzerApp::ANALYZER_LOG_TRIAL_BRIEF;
        } else if (equalsIgnoreCase(part, "default")) {
            flags |= AnalyzerApp::DEFAULT_ANALYZER_LOG_FLAGS;
        } else if (equalsIgnoreCase(part, "full")) {
            flags |= AnalyzerApp::ANALYZER_LOG_SUMMARY |
                     AnalyzerApp::ANALYZER_LOG_TRIAL |
                     AnalyzerApp::ANALYZER_LOG_CANDIDATE |
                     AnalyzerApp::ANALYZER_LOG_FREQ_CLASS |
                     AnalyzerApp::ANALYZER_LOG_EXPLAIN;
        } else if (equalsIgnoreCase(part, "quiet") || equalsIgnoreCase(part, "none")) {
            flags = AnalyzerApp::ANALYZER_LOG_NONE;
        }
        part = strtok_r(nullptr, ",+|", &savePtr);
    }

    return flags;
}

bool analyzerLogTokenUsesLegacyExplain(const char* token) {
    if (token == nullptr || *token == '\0') {
        return false;
    }

    if (equalsIgnoreCase(token, "raw") || equalsIgnoreCase(token, "raw_debug") || equalsIgnoreCase(token, "liveraw")) {
        return true;
    }

    char buffer[64];
    strncpy(buffer, token, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    char* savePtr = nullptr;
    char* part = strtok_r(buffer, ",+|", &savePtr);
    while (part != nullptr) {
        if (equalsIgnoreCase(part, "raw") || equalsIgnoreCase(part, "raw_debug") || equalsIgnoreCase(part, "liveraw")) {
            return true;
        }
        part = strtok_r(nullptr, ",+|", &savePtr);
    }

    return false;
}

void printSequenceHelp() {
    Serial.println("CMD: SEQ help");
    Serial.println("CMD: SEQ");
    Serial.println("CMD: SEQ stop");
    Serial.println("SEQ IN: start [tries=N] [period=MS] [window=MS] [freq=HZ] [dur=MS] [test=LABEL]");
    Serial.println("SEQ IN: OBS start [tries=N] [period=2000] [window=1800] [freq=HZ] [dur=MS] [test=LABEL]");
    Serial.println("SEQ IN: [profile=freqamp|chirp] [liveFreqOnly=1|freqOnly=1|mode=livefreq]");
    Serial.println("SEQ IN: [log=default|none|quiet|summary|summary+trial|trialbrief|candidate|report|explain|custom|ampwindow]");
    Serial.println("SEQ IN: stable summary=log=summary; legacy aliases=raw|raw_debug|liveraw|freq_class|trialbrief");
    Serial.println("SEQ IN: [debug=0|1|2] [dumpSamples=0|1] [curveFormat=off|samples]");
    Serial.println("SEQ IN: [sampleFirst=N] [sampleEvery=N] [sampleLead=MS] [sampleTail=MS] [sampleStep=MS] [sampleMax=N]");
    Serial.println("SEQ OUT: SEQ start / SEQ running / SEQ_CAND / SEQ_REPORT / SEQ_TRIAL / SEQ_EXPLAIN / SEQ_CUSTOM / SEQ_SUMMARY");
    Serial.println("SEQ OUT: legacy explain = SEQ_EXPLAIN_LEGACY_*");
    Serial.println("SEQ OUT: candidate fields include onset_sample peak_sample release_sample peak_ms dur end_dt_ms freq_*");
    Serial.println("SEQ OBS: passive observe mode for an already-running external emitter");
    Serial.println("SEQ PROFILE: profile=freqamp|chirp");
    Serial.println("SEQ PARAM: freqScore=50000 freqContrast=20.0");
}

bool waitForEmitterAck(const char* expectedPrefix, unsigned long timeoutMs) {
    const unsigned long startMs = millis();
    char line[96];
    size_t lineLength = 0;

    // Wait synchronously for the emitter to acknowledge remote mode.
    while (millis() - startMs < timeoutMs) {
        while (Serial2.available() > 0) {
            const char c = static_cast<char>(Serial2.read());
            if (c == '\r') {
                continue;
            }

            if (c == '\n') {
                line[lineLength] = '\0';
                if (lineLength > 0 && strncmp(line, expectedPrefix, strlen(expectedPrefix)) == 0) {
                    return true;
                }
                lineLength = 0;
                continue;
            }

            if (lineLength < sizeof(line) - 1) {
                line[lineLength++] = c;
            }
        }
        delay(1);
    }

    return false;
}

const char* h3SequenceCandidateClassFromResult(const char* result) {
    if (strcmp(result, "expected") == 0) {
        return "expected_primary";
    }
    if (strcmp(result, "duplicate") == 0) {
        return "duplicate";
    }
    if (strcmp(result, "late") == 0) {
        return "late";
    }
    if (strcmp(result, "self_suppressed") == 0) {
        return "self_suppressed";
    }
    return "unexpected_noise";
}

const char* h3SequenceCandidateClass(bool duplicateCandidate, bool inWindow, long dtFromTriggerMs) {
    if (duplicateCandidate) {
        return "duplicate";
    }
    if (!inWindow) {
        return "unexpected_noise";
    }
    if (dtFromTriggerMs >= kLateOnsetMinMs) {
        return "late";
    }
    return "expected_primary";
}

const char* sequenceTrialDurationClass(long durMs) {
    if (durMs < 0) {
        return "-";
    }
    if (durMs <= kCleanDurationMaxMs) {
        return "normal";
    }
    if (durMs >= kNearMaxDurationMinMs) {
        return "near_max";
    }
    return "long";
}

void printH3FrequencyEvidenceFields(const detection::PatternResult& patternResult,
                                    const detection::FrequencyEvidence& frequencyEvidence,
                                    const detection::FrequencyEvidence* liveFrequencyEvidence,
                                    const FrequencyEvidenceEvaluation::Values& tuning,
                                    const char* candidateClass,
                                    long transientAgeOrDtMs,
                                    unsigned long referenceMs) {
    const auto frequencyEval = FrequencyEvidenceEvaluation::evaluate(frequencyEvidence, tuning);
    Serial.print(" candidate_class=");
    Serial.print(candidateClass);
    Serial.print(" source=");
    Serial.print(detection::patternSourceName(patternResult.source));
    Serial.print(" pattern_valid=");
    Serial.print(patternResult.valid ? 1 : 0);
    Serial.print(" pattern_type=");
    Serial.print(detection::patternTypeName(patternResult.type));
    Serial.print(" pattern_reason=");
    Serial.print(detection::patternReasonName(patternResult.reasonCode));
    Serial.print(" candidate_valid=");
    Serial.print(patternResult.candidateValid ? 1 : 0);
    Serial.print(" tonal_valid=");
    Serial.print(patternResult.tonalValid ? 1 : 0);
    Serial.print(" behavior_eligible=");
    Serial.print(patternResult.behaviorEligible ? 1 : 0);
    Serial.print(" reject_reason=");
    Serial.print(detection::patternRejectReasonName(patternResult.rejectReason));
    Serial.print(" transient_duration_ms=");
    Serial.print(patternResult.candidate.durationMs);
    Serial.print(" transient_peak_strength=");
    Serial.print(patternResult.candidate.peakStrength, 1);
    Serial.print(" transient_age_or_dt_ms=");
    if (transientAgeOrDtMs >= 0) {
        Serial.print(transientAgeOrDtMs);
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(" freq_present=");
    Serial.print(patternResult.freq.present ? 1 : 0);
    Serial.print(" freq_matched=");
    Serial.print(patternResult.freq.matched ? 1 : 0);
    Serial.print(" freq_score_ok=");
    Serial.print(frequencyEval.scoreOk ? 1 : 0);
    Serial.print(" freq_contrast_ok=");
    Serial.print(frequencyEval.contrastOk ? 1 : 0);
    Serial.print(" freq_score=");
    Serial.print(patternResult.freq.score, 1);
    Serial.print(" freq_conf=");
    Serial.print(patternResult.freq.confidence, 1);
    Serial.print(" freq_target_hz=");
    Serial.print(frequencyEvidence.targetHz);
    Serial.print(" freq_target_power=");
    Serial.print(frequencyEvidence.targetPower, 1);
    Serial.print(" freq_neighbor_power=");
    Serial.print(frequencyEvidence.neighborPower, 1);
    Serial.print(" freq_total_energy=");
    Serial.print(frequencyEvidence.totalEnergy, 1);
    Serial.print(" freq_contrast=");
    Serial.print(patternResult.freq.spectralContrast, 2);
    Serial.print(" freq_observed_at_ms=");
    Serial.print(patternResult.freq.observedAtMs);
    Serial.print(" freq_age_ms=");
    if (patternResult.freq.observedAtMs > 0 && referenceMs >= patternResult.freq.observedAtMs) {
        Serial.print(referenceMs - patternResult.freq.observedAtMs);
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(" freq_valid_window=");
    Serial.print(patternResult.freq.validWindow ? 1 : 0);
    Serial.print(" freq_eval_reason=");
    Serial.print(FrequencyEvidenceEvaluation::reasonName(frequencyEval.reason));
    if (liveFrequencyEvidence != nullptr) {
        Serial.print(" liveFreq[avail=");
        Serial.print(liveFrequencyEvidence->present ? 1 : 0);
        Serial.print(" ready=");
        Serial.print(liveFrequencyEvidence->windowAvailable ? 1 : 0);
        Serial.print(" samples=");
        Serial.print(liveFrequencyEvidence->windowSampleCount);
        Serial.print(" score=");
        Serial.print(liveFrequencyEvidence->score, 1);
        Serial.print(" target=");
        Serial.print(liveFrequencyEvidence->targetHz);
        Serial.print(" contrast=");
        Serial.print(liveFrequencyEvidence->spectralContrast, 2);
        Serial.print(" obs=");
        Serial.print(liveFrequencyEvidence->observedAtMs);
        Serial.print("]");
    }
}

}

// -----------------------------------------------------------------------------
// Construction and setup
// -----------------------------------------------------------------------------

AnalyzerApp::AnalyzerApp(int inputPin)
    : _inputPin(inputPin),
      _i2sSource(14, 27, 33, 16000, 32),
      _audioSource(_i2sSource),
      _audioOnsetDetector(),
      _audioSignal(_audioSource),
      _freqBandStream() {
    _frequencyEvidenceTuning.scoreMin = 10000.0f;
    _frequencyEvidenceTuning.contrastMin = 20.0f;
    _liveFrequencyEvidenceTuning.scoreMin = 10000.0f;
    _liveFrequencyEvidenceTuning.contrastMin = 50.0f;
}

void AnalyzerApp::begin() {
    beginEmitterControl();

    configureParameters();
    _audioSource.begin();
    _audioSignal.begin();
    _audioSignal.setCurveSampleCallback(&AnalyzerApp::sequenceCurveSampleCallback, this);
    _audioOnsetDetector.begin();
    _freqBandStream.resetState();
    if (_sequenceFeatureHistory == nullptr) {
        _sequenceFeatureHistory = new detection::FeatureHistory();
    }
    _sequenceFeatureHistory->reset();
    _audioOnsetDetector.setDiagnosticsEnabled(AUDIO_VERBOSE_DEBUG);
    _lastPrintMs = 0;
    _usbLineLength = 0;
    _usbLineBuffer[0] = '\0';
    _emitterLineLength = 0;
    _emitterLineBuffer[0] = '\0';
    _controlClaimPending = false;
    _controlClaimSent = false;
    _controlClaimAtMs = 0;

    Serial.println("EVT analyzer_ready");
    Serial.println("EVT analyzer_help type='HELP', 'BASE', 'PARAM onset=23.0 release=20.0 cooldown=50 releaseDebounce=10 minMs=90 maxMs=240 minStrength=40.0 freqScore=10000 freqContrast=20.0', 'TEST', 'RAW trigger f=3200 dur=100 post=1000 dump=bin', 'SEQ log=default|summary|summary+trial|trialbrief|candidate|freq_class|explain|custom|ampwindow dumpSamples=1 curveFormat=samples', 'CAP', 'DET AMP', 'VAL', 'VAL OFF'");
}

void AnalyzerApp::configureParameters() {
    configureSharedParameters();
    configureI2SParameters();
}

void AnalyzerApp::configureSharedParameters() {
    _audioSignal.setSmoothingFactor(0.5f);
    _audioSignal.setBaselineUpdateFactor(0.005f);
}

void AnalyzerApp::configureI2SParameters() {
    _audioSignal.setBaselineTrackingQuietThreshold(20);
    setDetectorOnsetDetectionThreshold(30.0f);
    setDetectorOnsetReleaseThreshold(20.0f);
    setDetectorCooldownAfterOnsetMs(50);
    setDetectorReleaseDebounceMs(10);
    setDetectorMinTransientDurationMs(90);
    setDetectorMaxTransientDurationMs(240);
    setDetectorMinTransientPeakStrength(40.0f);
}

// -----------------------------------------------------------------------------
// Runtime loop and detector state
// -----------------------------------------------------------------------------

void AnalyzerApp::update() {
    const unsigned long now = millis();

    int processedSamples = 0;
    AudioBlock block;
    while (processedSamples < kMaxSamplesPerLoop && _i2sSource.readBlock(block)) {
        if (block.sampleCount == 0 || block.samples == nullptr) {
            break;
        }

        const uint32_t sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
        const uint32_t samplePeriodUs = sampleRateHz > 0 ? static_cast<uint32_t>(1000000UL / sampleRateHz) : 0;
        for (uint16_t i = 0; i < block.sampleCount; ++i) {
            const uint64_t sampleIndex = block.startSampleIndex + static_cast<uint64_t>(i);
            const uint32_t sampleTimeUs = sampleRateHz > 0
                ? static_cast<uint32_t>((sampleIndex * 1000000ULL) / static_cast<uint64_t>(sampleRateHz))
                : block.approxStartMicros;
            const uint32_t approxBlockSampleMicros = samplePeriodUs > 0
                ? block.approxStartMicros + static_cast<uint32_t>(static_cast<uint32_t>(i) * samplePeriodUs)
                : block.approxStartMicros;
            const uint32_t sampleTimeMsApprox = approxBlockSampleMicros / 1000UL;
            AudioSignalFrame frame;
            _audioSignal.update(static_cast<int>(block.samples[i]), sampleTimeUs, frame);
            frame.sampleTimeMs = sampleTimeMsApprox;
            if (_sequenceFeatureHistory != nullptr) {
                detection::FeatureExtractor::observeFrame(frame, *_sequenceFeatureHistory);
            }
            _audioOnsetDetector.update(static_cast<float>(frame.level), frame.sampleTimeUs);
            _freqBandStream.observeCenteredSample(frame.centeredSample);
            if (_sequenceTest.active && _sequenceTest.currentTrial > 0 && _detection != nullptr) {
                const detection::FrequencyEvidence runtimeFrequencyEvidence = captureFrequencyEvidence();
                _detection->observeFrame(frame, runtimeFrequencyEvidence, sampleTimeMsApprox);
                detection::PatternResult runtimePatternResult;
                while (_detection->popPatternResult(runtimePatternResult)) {
                    _sequenceTest.currentTrialDiagnostics.runtimePatternResult = runtimePatternResult;
                    _sequenceTest.currentTrialDiagnostics.runtimePatternCaptured = true;
                    _sequenceTest.currentTrialDiagnostics.runtimeFieldState = _detection->fieldState();
                    if (!_sequenceTest.liveFrequencyOnly) {
                        handleSequenceCandidate(runtimePatternResult, 0, &runtimeFrequencyEvidence);
                    }
                }
            }
        }
        updateSequenceAmbientStats();

        processedSamples += static_cast<int>(block.sampleCount);
        if (processedSamples > kMaxSamplesPerLoop) {
            processedSamples = kMaxSamplesPerLoop;
        }
    }

    _sequenceTest.samplesProcessed += static_cast<unsigned long>(processedSamples);
    if (static_cast<unsigned long>(processedSamples) > _sequenceTest.maxSamplesPerLoop) {
        _sequenceTest.maxSamplesPerLoop = static_cast<unsigned long>(processedSamples);
    }

    updateBaseSession(now);
    if (_controlClaimPending && !_controlClaimSent && now >= _controlClaimAtMs) {
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

void AnalyzerApp::resetDetectorState() {
    _audioSignal.resetSignalState();
    if (_sequenceFeatureHistory != nullptr) {
        _sequenceFeatureHistory->reset();
    }
}

bool AnalyzerApp::detectorOnsetDetected() const {
    return _audioOnsetDetector.onsetDetected();
}

float AnalyzerApp::detectorOnsetStrength() const {
    return _audioOnsetDetector.onsetStrength();
}

bool AnalyzerApp::detectorTransientDetected() const {
    return _audioOnsetDetector.transientDetected();
}

float AnalyzerApp::detectorTransientStrength() const {
    return _audioOnsetDetector.transientStrength();
}

unsigned long AnalyzerApp::detectorTransientDurationMs() const {
    return _audioOnsetDetector.transientDurationMs();
}

bool AnalyzerApp::detectorTransientPeakActive() const {
    return _audioOnsetDetector.peakActive();
}

const char* AnalyzerApp::detectorOnsetRejectReasonName() const {
    return _audioOnsetDetector.lastOnsetRejectReasonName();
}

const char* AnalyzerApp::detectorTransientRejectReasonName() const {
    return _audioOnsetDetector.lastTransientRejectReasonName();
}

unsigned long AnalyzerApp::detectorTransientRejectedDurationMs() const {
    return _audioOnsetDetector.lastTransientRejectedDurationMs();
}

float AnalyzerApp::detectorTransientRejectedStrength() const {
    return _audioOnsetDetector.lastTransientRejectedStrength();
}

float AnalyzerApp::detectorOnsetDetectionThreshold() const {
    return _audioOnsetDetector.onsetDetectionThreshold();
}

float AnalyzerApp::detectorOnsetReleaseThreshold() const {
    return _audioOnsetDetector.onsetReleaseThreshold();
}

unsigned long AnalyzerApp::detectorCooldownAfterOnsetMs() const {
    return _audioOnsetDetector.cooldownAfterOnsetMs();
}

unsigned long AnalyzerApp::detectorMinTransientDurationMs() const {
    return _audioOnsetDetector.minTransientDurationMs();
}

unsigned long AnalyzerApp::detectorMaxTransientDurationMs() const {
    return _audioOnsetDetector.maxTransientDurationMs();
}

float AnalyzerApp::detectorMinTransientPeakStrength() const {
    return _audioOnsetDetector.minTransientPeakStrength();
}

unsigned long AnalyzerApp::detectorReleaseDebounceMs() const {
    return _audioOnsetDetector.releaseDebounceMs();
}

void AnalyzerApp::setDetectorOnsetDetectionThreshold(float value) {
    _audioOnsetDetector.setOnsetDetectionThreshold(value);
}

void AnalyzerApp::setDetectorOnsetReleaseThreshold(float value) {
    _audioOnsetDetector.setOnsetReleaseThreshold(value);
}

void AnalyzerApp::setDetectorCooldownAfterOnsetMs(unsigned long value) {
    _audioOnsetDetector.setCooldownAfterOnsetMs(value);
}

void AnalyzerApp::setDetectorMinTransientDurationMs(unsigned long value) {
    _audioOnsetDetector.setMinTransientDurationMs(value);
}

void AnalyzerApp::setDetectorMaxTransientDurationMs(unsigned long value) {
    _audioOnsetDetector.setMaxTransientDurationMs(value);
}

void AnalyzerApp::setDetectorMinTransientPeakStrength(float value) {
    _audioOnsetDetector.setMinTransientPeakStrength(value);
}

void AnalyzerApp::setDetectorReleaseDebounceMs(unsigned long value) {
    _audioOnsetDetector.setReleaseDebounceMs(value);
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
    resetDetectorState();
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

    if (AUDIO_VERBOSE_DEBUG && !_baseSession.quiet && now - _baseSession.lastStatusPrintMs >= 5000UL) {
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

    if (now - _baseSession.startedAtMs >= _baseSession.durationMs) {
        printBaseSummary();
        stopBaseSession();
        Serial.println("BASE stopped");
    }
}

void AnalyzerApp::beginEmitterControl() {
    Serial2.begin(_controlBaudRate, SERIAL_8N1, _controlRxPin, _controlTxPin);
    Serial.print("EVT analyzer_control rx=");
    Serial.print(_controlRxPin);
    Serial.print(" tx=");
    Serial.println(_controlTxPin);
    Serial.println("EVT analyzer_control_claim scheduled");
}

// -----------------------------------------------------------------------------
// Console and emitter control
// -----------------------------------------------------------------------------

void AnalyzerApp::pollUsbConsole() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            _usbLineBuffer[_usbLineLength] = '\0';
            if (_usbLineLength > 0) {
                handleUsbLine(_usbLineBuffer);
            }
            _usbLineLength = 0;
            continue;
        }

        if (_usbLineLength < sizeof(_usbLineBuffer) - 1) {
            _usbLineBuffer[_usbLineLength++] = c;
        }
    }
}

void AnalyzerApp::pollEmitterSerial() {
    while (Serial2.available() > 0) {
        const char c = static_cast<char>(Serial2.read());
        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            _emitterLineBuffer[_emitterLineLength] = '\0';
            // Expected acknowledgements are suppressed so the console stays readable.
            if (_emitterLineLength > 0
                && !_valMode
                && !startsWithTokenIgnoreCase(_emitterLineBuffer, "OK CHIRP")
                && !startsWithTokenIgnoreCase(_emitterLineBuffer, "OK MODE REMOTE")) {
                Serial.print("EMIT< ");
                Serial.println(_emitterLineBuffer);
            }
            _emitterLineLength = 0;
            continue;
        }

        if (_emitterLineLength < sizeof(_emitterLineBuffer) - 1) {
            _emitterLineBuffer[_emitterLineLength++] = c;
        }
    }
}

// -----------------------------------------------------------------------------
// Command parsing
// -----------------------------------------------------------------------------

void AnalyzerApp::handleUsbLine(const char* line) {
    if (equalsIgnoreCase(line, "HELP")) {
        if (_valMode) {
            return;
        }
        Serial.println("CMD: BASE dur=10000 quiet");
        Serial.println("CMD: BASE stop");
        Serial.println("CMD: PARAM onset=23.0 release=20.0 cooldown=50 releaseDebounce=10 minMs=90 maxMs=240 minStrength=40.0 freqScore=10000 freqContrast=20.0");
        Serial.println("CMD: EMIT CHIRP freq=3200 dur=100");
        Serial.println("CMD: EMIT MODE REMOTE");
        Serial.println("CMD: EMIT MODE AUTO interval=2000 freq=3200 dur=100");
        Serial.println("CMD: EMIT SWEEP start=3000 stop=3500 step=100 dur=80 pause=1000");
        Serial.println("CMD: TEST");
        Serial.println("CMD: raw trigger f=3200 dur=100 post=1000 dump=bin");
        Serial.println("CMD: SEQ");
        Serial.println("CMD: SEQ help");
        Serial.println("CMD: SEQ stop");
        Serial.println("CMD: CAP");
        Serial.println("CMD: CAP stop");
        Serial.println("CMD: VAL");
        Serial.println("CMD: VAL OFF");
        return;
    }

    if (startsWithTokenIgnoreCase(line, "PARAM")) {
        char buffer[128];
        strncpy(buffer, line, sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        char* savePtr = nullptr;
        char* token = strtok_r(buffer, " ", &savePtr);
        DetectorParameters::Values params = DetectorParameters::capture(_audioOnsetDetector);
        FrequencyEvidenceEvaluation::Values freqTuning = _frequencyEvidenceTuning;

        while ((token = strtok_r(nullptr, " ", &savePtr)) != nullptr) {
            DetectorParameters::parseToken(token, params);
            FrequencyEvidenceEvaluation::parseToken(token, freqTuning);
        }

        DetectorParameters::apply(params, _audioOnsetDetector);
        _frequencyEvidenceTuning = freqTuning;

        Serial.print("PARAM onset=");
        Serial.print(detectorOnsetDetectionThreshold(), 1);
        Serial.print(" release=");
        Serial.print(detectorOnsetReleaseThreshold(), 1);
        Serial.print(" cooldown=");
        Serial.print(detectorCooldownAfterOnsetMs());
        Serial.print(" releaseDebounce=");
        Serial.print(detectorReleaseDebounceMs());
        Serial.print(" minMs=");
        Serial.print(detectorMinTransientDurationMs());
        Serial.print(" maxMs=");
        Serial.print(detectorMaxTransientDurationMs());
        Serial.print(" minStrength=");
        Serial.print(detectorMinTransientPeakStrength(), 1);
        Serial.print(" freqScore=");
        Serial.print(_frequencyEvidenceTuning.scoreMin, 0);
        Serial.print(" freqContrast=");
        Serial.println(_frequencyEvidenceTuning.contrastMin, 1);
        return;
    }

    if (startsWithTokenIgnoreCase(line, "BASE")) {
        if (_valMode) {
            return;
        }
        char buffer[96];
        strncpy(buffer, line, sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        char* savePtr = nullptr;
        char* token = strtok_r(buffer, " ", &savePtr);
        token = token != nullptr ? strtok_r(nullptr, " ", &savePtr) : nullptr;

        if (token != nullptr && equalsIgnoreCase(token, "stop")) {
            if (_baseSession.active) {
                printBaseSummary();
            }
            stopBaseSession();
            Serial.println("BASE stopped");
            return;
        }

        unsigned long durationMs = 10000;
        bool quiet = false;

        while (token != nullptr) {
            if (startsWithTokenIgnoreCase(token, "dur=")) {
                durationMs = strtoul(token + 4, nullptr, 10);
            } else if (equalsIgnoreCase(token, "quiet")) {
                quiet = true;
            }
            token = strtok_r(nullptr, " ", &savePtr);
        }

        startBaseSession(durationMs, quiet);
        return;
    }

    if (equalsIgnoreCase(line, "TEST")) {
        if (_valMode) {
            return;
        }
        sendEmitterCommand("MODE REMOTE");
        sendEmitterCommand("CHIRP freq=3200 dur=100");
        return;
    }

    if (startsWithTokenIgnoreCase(line, "RAW")) {
        if (_valMode) {
            return;
        }
        char buffer[96];
        strncpy(buffer, line, sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        char* savePtr = nullptr;
        char* token = strtok_r(buffer, " ", &savePtr);
        token = token != nullptr ? strtok_r(nullptr, " ", &savePtr) : nullptr;

        if (token == nullptr || !equalsIgnoreCase(token, "trigger")) {
            Serial.println("RAW_ERR usage=raw trigger f=3200 dur=100 post=1000");
            return;
        }

        unsigned long toneHz = 3200;
        unsigned long durationMs = 100;
        unsigned long postMs = 1000;
        unsigned long preMs = 0;
        unsigned long decim = 1;
        bool dumpChunks = false;
        bool dumpBinary = false;

        while ((token = strtok_r(nullptr, " ", &savePtr)) != nullptr) {
            if (startsWithTokenIgnoreCase(token, "f=")) {
                toneHz = strtoul(token + 2, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "dur=")) {
                durationMs = strtoul(token + 4, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "post=")) {
                postMs = strtoul(token + 5, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "pre=")) {
                preMs = strtoul(token + 4, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "decim=")) {
                decim = strtoul(token + 6, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "dump=")) {
                const char* value = token + 5;
                dumpChunks = equalsIgnoreCase(value, "chunks");
                dumpBinary = equalsIgnoreCase(value, "bin") || equalsIgnoreCase(value, "binary");
            }
        }

        runRawTrigger(toneHz, durationMs, postMs, preMs, decim, dumpChunks, dumpBinary);
        return;
    }

    if (startsWithTokenIgnoreCase(line, "SEQ")) {
        if (_valMode) {
            return;
        }
        char buffer[96];
        strncpy(buffer, line, sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        char* savePtr = nullptr;
        char* token = strtok_r(buffer, " ", &savePtr);
        token = token != nullptr ? strtok_r(nullptr, " ", &savePtr) : nullptr;

        bool externalEmitter = false;
        if (token != nullptr && (equalsIgnoreCase(token, "obs") || equalsIgnoreCase(token, "observe") || equalsIgnoreCase(token, "passive"))) {
            externalEmitter = true;
            token = strtok_r(nullptr, " ", &savePtr);
        }

        if (token != nullptr && (equalsIgnoreCase(token, "help") || equalsIgnoreCase(token, "?"))) {
            printSequenceHelp();
            return;
        }

        if (token != nullptr && equalsIgnoreCase(token, "stop")) {
            if (_sequenceTest.active) {
                printSequenceFinalOutput();
            }
            stopSequenceTest();
            Serial.println("SEQ stopped");
            return;
        }

        unsigned long totalTrials = 100;
        unsigned long periodMs = externalEmitter ? 2000 : 2500;
        unsigned long windowEndOffsetMs = externalEmitter ? 2000 : 2200;
        unsigned long toneHz = 3200;
        unsigned long durationMs = 100;
        uint32_t logFlags = AnalyzerApp::DEFAULT_ANALYZER_LOG_FLAGS;
        bool customLogRequested = false;
        const char* setupLabel = nullptr;
        bool sampleDumpEnabled = false;
        unsigned long sampleDumpFirstTrials = 2;
        unsigned long sampleDumpEveryNth = 0;
        unsigned long sampleDumpLeadMs = 50;
        unsigned long sampleDumpTailMs = 800;
        unsigned long sampleDumpStepMs = 1;
        unsigned long sampleDumpMaxRows = 5000;
        detection::DetectionProfileKind profileKind = detection::DetectionProfileKind::FreqAmp;
        bool liveFrequencyOnly = false;
        bool legacyExplainOutput = false;

        while (token != nullptr) {
            if (equalsIgnoreCase(token, "start")) {
                // Optional human-friendly token; no-op.
            } else if (startsWithTokenIgnoreCase(token, "tries=")) {
                totalTrials = strtoul(token + 6, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "period=")) {
                periodMs = strtoul(token + 7, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "window=")) {
                windowEndOffsetMs = strtoul(token + 7, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "freq=")) {
                toneHz = strtoul(token + 5, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "dur=")) {
                durationMs = strtoul(token + 4, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "test=")) {
                setupLabel = token + 5;
            } else if (startsWithTokenIgnoreCase(token, "dumpSamples=")) {
                sampleDumpEnabled = strtoul(token + 12, nullptr, 10) != 0;
            } else if (startsWithTokenIgnoreCase(token, "curveFormat=")) {
                const char* value = token + 12;
                if (equalsIgnoreCase(value, "samples")) {
                    sampleDumpEnabled = true;
                } else if (equalsIgnoreCase(value, "off") || equalsIgnoreCase(value, "none")) {
                    sampleDumpEnabled = false;
                }
            } else if (startsWithTokenIgnoreCase(token, "sampleFirst=")) {
                sampleDumpFirstTrials = strtoul(token + 12, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "sampleEvery=")) {
                sampleDumpEveryNth = strtoul(token + 12, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "sampleLead=")) {
                sampleDumpLeadMs = strtoul(token + 11, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "sampleTail=")) {
                sampleDumpTailMs = strtoul(token + 11, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "sampleStep=")) {
                sampleDumpStepMs = strtoul(token + 11, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "sampleMax=")) {
                sampleDumpMaxRows = strtoul(token + 10, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "profile=")) {
                const char* value = token + 8;
                if (!detection::detectionProfileKindFromName(value, profileKind)) {
                    Serial.print("SEQ_VERBOSE_WARN reason=unknown_profile value=");
                    Serial.println(value);
                }
            } else if (equalsIgnoreCase(token, "livefreq") || equalsIgnoreCase(token, "freqonly")) {
                liveFrequencyOnly = true;
            } else if (startsWithTokenIgnoreCase(token, "liveFreqOnly=") || startsWithTokenIgnoreCase(token, "freqOnly=")) {
                const char* value = strchr(token, '=');
                liveFrequencyOnly = value != nullptr ? strtoul(value + 1, nullptr, 10) != 0 : true;
            } else if (startsWithTokenIgnoreCase(token, "mode=")) {
                const char* value = token + 5;
                if (equalsIgnoreCase(value, "livefreq") || equalsIgnoreCase(value, "freqonly")) {
                    liveFrequencyOnly = true;
                } else if (detection::detectionProfileKindFromName(value, profileKind)) {
                    // profile selection is now explicit for Analyzer runs.
                }
            } else if (equalsIgnoreCase(token, "log")) {
                token = strtok_r(nullptr, " ", &savePtr);
                if (token != nullptr) {
                    logFlags = analyzerLogFlagsFromToken(token);
                    legacyExplainOutput = analyzerLogTokenUsesLegacyExplain(token);
                    token = strtok_r(nullptr, " ", &savePtr);
                    continue;
                }
            } else if (startsWithTokenIgnoreCase(token, "debug=")) {
                logFlags = analyzerLogFlagsFromLevel(strtoul(token + 6, nullptr, 10));
            } else if (startsWithTokenIgnoreCase(token, "log=")) {
                logFlags = analyzerLogFlagsFromToken(token + 4);
                legacyExplainOutput = analyzerLogTokenUsesLegacyExplain(token + 4);
                customLogRequested = analyzerLogEnabled(logFlags, AnalyzerApp::ANALYZER_LOG_CUSTOM);
            }
            token = strtok_r(nullptr, " ", &savePtr);
        }

        if (customLogRequested) {
            logFlags = AnalyzerApp::ANALYZER_LOG_CUSTOM;
        }

        PendingSequenceStart& pending = _pendingSequenceStart;
        pending.active = true;
        pending.totalTrials = totalTrials;
        pending.periodMs = periodMs;
        pending.windowEndOffsetMs = windowEndOffsetMs;
        pending.toneHz = toneHz;
        pending.durationMs = durationMs;
        pending.quiet = false;
        pending.showDetails = true;
        pending.logFlags = logFlags;
        pending.sampleDumpEnabled = sampleDumpEnabled;
        pending.sampleDumpFirstTrials = sampleDumpFirstTrials;
        pending.sampleDumpEveryNth = sampleDumpEveryNth;
        pending.sampleDumpLeadMs = sampleDumpLeadMs;
        pending.sampleDumpTailMs = sampleDumpTailMs;
        pending.sampleDumpStepMs = sampleDumpStepMs;
        pending.sampleDumpMaxRows = sampleDumpMaxRows;
        pending.profileKind = profileKind;
        pending.liveFrequencyOnly = liveFrequencyOnly;
        pending.externalEmitter = externalEmitter;
        pending.legacyExplainOutput = legacyExplainOutput;
        if (setupLabel != nullptr && setupLabel[0] != '\0') {
            strncpy(pending.setupLabelStorage, setupLabel, sizeof(pending.setupLabelStorage));
            pending.setupLabelStorage[sizeof(pending.setupLabelStorage) - 1] = '\0';
            pending.setupLabel = pending.setupLabelStorage;
        } else {
            pending.setupLabelStorage[0] = '\0';
            pending.setupLabel = nullptr;
        }
        return;
    }

    if (startsWithTokenIgnoreCase(line, "CAP")) {
        if (_valMode) {
            return;
        }
        char buffer[96];
        strncpy(buffer, line, sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        char* savePtr = nullptr;
        char* token = strtok_r(buffer, " ", &savePtr);
        token = token != nullptr ? strtok_r(nullptr, " ", &savePtr) : nullptr;

        if (token != nullptr && equalsIgnoreCase(token, "stop")) {
            if (_captureSession.active) {
                printCaptureSummary();
            }
            stopCaptureSession();
            Serial.println("CAP stopped");
            return;
        }

        unsigned long totalTrials = 20;
        unsigned long periodMs = 2500;
        unsigned long windowEndOffsetMs = 500;
        unsigned long toneHz = 3200;
        unsigned long durationMs = 100;

        while (token != nullptr) {
            if (startsWithTokenIgnoreCase(token, "tries=")) {
                totalTrials = strtoul(token + 6, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "period=")) {
                periodMs = strtoul(token + 7, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "window=")) {
                windowEndOffsetMs = strtoul(token + 7, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "freq=")) {
                toneHz = strtoul(token + 5, nullptr, 10);
            } else if (startsWithTokenIgnoreCase(token, "dur=")) {
                durationMs = strtoul(token + 4, nullptr, 10);
            }
            token = strtok_r(nullptr, " ", &savePtr);
        }

        startCaptureSession(totalTrials, periodMs, windowEndOffsetMs, toneHz, durationMs);
        return;
    }

    if (equalsIgnoreCase(line, "VAL") || equalsIgnoreCase(line, "VAL ON") || equalsIgnoreCase(line, "VERBOSE ON")) {
        _valMode = true;
        return;
    }

    if (equalsIgnoreCase(line, "VAL OFF") || equalsIgnoreCase(line, "VERBOSE OFF")) {
        _valMode = false;
        return;
    }

    if (startsWithTokenIgnoreCase(line, "EMIT ")) {
        if (_valMode) {
            return;
        }
        sendEmitterCommand(line + 5);
        return;
    }

    if (_valMode) {
        return;
    }

    Serial.print("EVT analyzer_unknown_cmd line=");
    Serial.println(line);
}

void AnalyzerApp::sendEmitterCommand(const char* command) {
    Serial2.println(command);
}

// -----------------------------------------------------------------------------
// Raw trigger and value-mode helpers
// -----------------------------------------------------------------------------

void AnalyzerApp::runRawTrigger(unsigned long toneHz, unsigned long durationMs, unsigned long postMs, unsigned long preMs, unsigned long decim, bool dumpChunks, bool dumpBinary) {
    if (_valMode) {
        return;
    }

    stopSequenceTest();
    stopCaptureSession();
    resetDetectorState();
    _audioSource.resetStats();

    if (toneHz == 0) {
        toneHz = 1;
    }
    if (durationMs == 0) {
        durationMs = 1;
    }
    if (postMs == 0) {
        postMs = 1;
    }
    if (decim == 0) {
        decim = 1;
    }
    if (dumpChunks) {
        Serial.println("RAW_INFO dump=chunks");
    }

    const uint32_t sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
    const unsigned long maxSamples = 16000UL;
    unsigned long preWantedSamples = static_cast<unsigned long>((static_cast<uint64_t>(preMs) * static_cast<uint64_t>(sampleRateHz)) / 1000ULL);
    unsigned long postWantedSamples = static_cast<unsigned long>((static_cast<uint64_t>(postMs) * static_cast<uint64_t>(sampleRateHz)) / 1000ULL);
    if (preWantedSamples > maxSamples) {
        preWantedSamples = maxSamples;
    }
    if (postWantedSamples > maxSamples) {
        postWantedSamples = maxSamples;
    }
    if (preWantedSamples + postWantedSamples > maxSamples) {
        if (preWantedSamples >= maxSamples) {
            preWantedSamples = maxSamples;
            postWantedSamples = 0;
        } else {
            postWantedSamples = maxSamples - preWantedSamples;
        }
    }
    if (preWantedSamples == 0 && postWantedSamples == 0) {
        postWantedSamples = 1;
    }
    const unsigned long captureSamples = preWantedSamples + postWantedSamples;
    int16_t* preRingBuffer = nullptr;
    if (preWantedSamples > 0) {
        preRingBuffer = static_cast<int16_t*>(malloc(static_cast<size_t>(preWantedSamples) * sizeof(int16_t)));
        if (preRingBuffer == nullptr) {
            Serial.println("RAW_ERR memory=pre_ring_alloc_failed");
            return;
        }
    }

    static int16_t rawBuffer[16000];
    unsigned long flushedSamples = 0;
    int discardedSample = 0;
    uint32_t discardedSampleTimeUs = 0;
    while (flushedSamples < kRawCaptureFlushSamples && _audioSource.readRawSample(discardedSample, discardedSampleTimeUs)) {
        ++flushedSamples;
    }

    const unsigned long captureId = ++_rawCaptureSequenceId;
    unsigned long preCaptured = 0;
    unsigned long preWriteIndex = 0;
    const unsigned long preDeadlineMs = millis() + preMs + kRawCaptureTimeoutSlackMs;
    while (preCaptured < preWantedSamples && millis() <= preDeadlineMs) {
        int rawSample = 0;
        uint32_t sampleTimeUs = 0;
        if (_audioSource.readRawSample(rawSample, sampleTimeUs)) {
            preRingBuffer[preWriteIndex] = rawCaptureSampleToInt16(rawSample);
            preWriteIndex = (preWriteIndex + 1UL) % (preWantedSamples > 0 ? preWantedSamples : 1UL);
            ++preCaptured;
        } else {
            delay(1);
        }
    }
    if (preCaptured > 0) {
        const unsigned long preStartIndex = preCaptured == preWantedSamples ? preWriteIndex : 0UL;
        for (unsigned long i = 0; i < preCaptured; ++i) {
            const unsigned long ringIndex = (preStartIndex + i) % (preCaptured > 0 ? preCaptured : 1UL);
            rawBuffer[i] = preRingBuffer[ringIndex];
        }
    }

    const unsigned long triggerMs = millis();
    char emitterCommand[96];
    snprintf(emitterCommand, sizeof(emitterCommand), "CHIRP freq=%lu dur=%lu", toneHz, durationMs);
    sendEmitterCommand(emitterCommand);
    Serial2.flush();

    unsigned long postCaptured = 0;
    const unsigned long postDeadlineMs = triggerMs + postMs + kRawCaptureTimeoutSlackMs;
    while (postCaptured < postWantedSamples && millis() <= postDeadlineMs) {
        int rawSample = 0;
        uint32_t sampleTimeUs = 0;
        if (_audioSource.readRawSample(rawSample, sampleTimeUs)) {
            rawBuffer[preCaptured + postCaptured] = rawCaptureSampleToInt16(rawSample);
            ++postCaptured;
        } else {
            delay(1);
        }
    }

    const unsigned long capturedSamples = preCaptured + postCaptured;
    const unsigned long droppedSamples = captureSamples > capturedSamples ? (captureSamples - capturedSamples) : 0;

    float env = 0.0f;
    float maxEnv = 0.0f;
    int maxRaw = 0;
    int maxAbs = 0;
    for (unsigned long i = 0; i < capturedSamples; ++i) {
        const int rawSample = static_cast<int>(rawBuffer[i]);
        const int absSample = rawSample < 0 ? -rawSample : rawSample;
        env = env * 0.95f + static_cast<float>(absSample) * 0.05f;
        if (absSample > maxRaw) {
            maxRaw = absSample;
        }
        if (absSample > maxAbs) {
            maxAbs = absSample;
        }
        if (env > maxEnv) {
            maxEnv = env;
        }
    }

    Serial.print("RAW_BEGIN id=");
    Serial.print(captureId);
    Serial.print(" sr=");
    Serial.print(sampleRateHz);
    Serial.print(" trigger_ms=");
    Serial.print(triggerMs);
    Serial.print(" f=");
    Serial.print(toneHz);
    Serial.print(" dur=");
    Serial.print(durationMs);
    Serial.print(" pre_ms=");
    Serial.print(preMs);
    Serial.print(" post_ms=");
    Serial.print(postMs);
    Serial.print(" decim=");
    Serial.print(decim);
    Serial.print(" pre_samples=");
    Serial.print(preCaptured);
    Serial.print(" post_samples=");
    Serial.print(postCaptured);
    if (dumpBinary) {
        Serial.print(" fields=raw16");
        Serial.print(" dump=bin");
        Serial.print(" samples=");
        Serial.print(capturedSamples);
        Serial.print(" bytes=");
        Serial.print(capturedSamples * sizeof(int16_t));
    } else if (dumpChunks) {
        Serial.print(" fields=min,max,rms,mean_abs");
        Serial.print(" dump=chunks");
        Serial.print(" chunk_samples=");
        Serial.print(rawCaptureChunkSize(sampleRateHz, decim));
    } else {
        Serial.print(" fields=i,raw,abs,env");
        Serial.print(" dump=full");
    }
    Serial.println();

    if (dumpBinary) {
        Serial.write(reinterpret_cast<const uint8_t*>(rawBuffer), capturedSamples * sizeof(int16_t));
        Serial.println();
    } else if (dumpChunks) {
        const unsigned long emittedSamples = (capturedSamples + decim - 1UL) / decim;
        const unsigned long chunkSamples = rawCaptureChunkSize(sampleRateHz, decim);
        for (unsigned long emittedStart = 0; emittedStart < emittedSamples; emittedStart += chunkSamples) {
            const unsigned long emittedEnd = emittedStart + chunkSamples < emittedSamples ? emittedStart + chunkSamples : emittedSamples;
            long chunkMin = 0;
            long chunkMax = 0;
            uint64_t sumAbs = 0;
            uint64_t sumSquares = 0;
            bool first = true;
            for (unsigned long emittedIndex = emittedStart; emittedIndex < emittedEnd; ++emittedIndex) {
                const unsigned long rawIndex = emittedIndex * decim;
                if (rawIndex >= capturedSamples) {
                    break;
                }
                const int sample = static_cast<int>(rawBuffer[rawIndex]);
                const long absSample = sample < 0 ? -static_cast<long>(sample) : static_cast<long>(sample);
                if (first) {
                    chunkMin = sample;
                    chunkMax = sample;
                    first = false;
                } else {
                    if (sample < chunkMin) {
                        chunkMin = sample;
                    }
                    if (sample > chunkMax) {
                        chunkMax = sample;
                    }
                }
                const uint64_t abs64 = static_cast<uint64_t>(absSample);
                sumAbs += abs64;
                sumSquares += abs64 * abs64;
            }
            const unsigned long chunkCount = emittedEnd > emittedStart ? emittedEnd - emittedStart : 0UL;
            if (chunkCount == 0) {
                continue;
            }
            const double meanAbs = static_cast<double>(sumAbs) / static_cast<double>(chunkCount);
            const double rms = sqrt(static_cast<double>(sumSquares) / static_cast<double>(chunkCount));
            const long i0 = static_cast<long>(emittedStart * decim) - static_cast<long>(preCaptured);
            const unsigned long lastRawIndex = (emittedEnd - 1UL) * decim;
            const long i1 = static_cast<long>(lastRawIndex < capturedSamples ? lastRawIndex : (capturedSamples - 1UL)) - static_cast<long>(preCaptured);
            Serial.print("RAW_CHUNK i0=");
            Serial.print(i0);
            Serial.print(" i1=");
            Serial.print(i1);
            Serial.print(" min=");
            Serial.print(chunkMin);
            Serial.print(" max=");
            Serial.print(chunkMax);
            Serial.print(" rms=");
            Serial.print(rms, 1);
            Serial.print(" mean_abs=");
            Serial.println(meanAbs, 1);
        }
    } else {
        env = 0.0f;
        for (long displayIndex = -static_cast<long>(preCaptured); displayIndex < static_cast<long>(postCaptured); displayIndex += static_cast<long>(decim)) {
            const unsigned long rawIndex = static_cast<unsigned long>(displayIndex + static_cast<long>(preCaptured));
            const int rawSample = static_cast<int>(rawBuffer[rawIndex]);
            const int absSample = rawSample < 0 ? -rawSample : rawSample;
            env = env * 0.95f + static_cast<float>(absSample) * 0.05f;
            Serial.print(displayIndex);
            Serial.print(',');
            Serial.print(rawSample);
            Serial.print(',');
            Serial.print(absSample);
            Serial.print(',');
            Serial.println(env, 2);
        }
    }

    Serial.print("RAW_END id=");
    Serial.print(captureId);
    Serial.print(" samples=");
    Serial.print(capturedSamples);
    Serial.print(" dropped=");
    Serial.print(droppedSamples);
    Serial.print(" elapsed_ms=");
    Serial.print(millis() - triggerMs);
    Serial.print(" max_raw=");
    Serial.print(maxRaw);
    Serial.print(" max_abs=");
    Serial.print(maxAbs);
    Serial.print(" max_env=");
    Serial.println(maxEnv, 2);

    if (preRingBuffer != nullptr) {
        free(preRingBuffer);
    }
}

void AnalyzerApp::printValueModeBanner() const {
    if (_valMode) {
        return;
    }
    Serial.print("EVT analyzer_val on source=");
    Serial.print("I2S");
    Serial.println(" detector=AMP");
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
        pending.setupLabel,
        pending.logFlags,
        pending.sampleDumpEnabled,
        pending.sampleDumpFirstTrials,
        pending.sampleDumpEveryNth,
        pending.sampleDumpLeadMs,
        pending.sampleDumpTailMs,
        pending.sampleDumpStepMs,
        pending.sampleDumpMaxRows,
        pending.profileKind,
        pending.liveFrequencyOnly,
        pending.externalEmitter,
        pending.legacyExplainOutput);
}

void AnalyzerApp::startSequenceTest(unsigned long totalTrials, unsigned long periodMs, unsigned long windowEndOffsetMs, unsigned long toneHz, unsigned long durationMs, bool quiet, bool showDetails, const char* setupLabel, uint32_t logFlags, bool sampleDumpEnabled, unsigned long sampleDumpFirstTrials, unsigned long sampleDumpEveryNth, unsigned long sampleDumpLeadMs, unsigned long sampleDumpTailMs, unsigned long sampleDumpStepMs, unsigned long sampleDumpMaxRows, detection::DetectionProfileKind profileKind, bool liveFrequencyOnly, bool externalEmitter, bool legacyExplainOutput) {
    if (_valMode) {
        return;
    }
    if (totalTrials == 0) {
        totalTrials = 1;
    }
    if (periodMs == 0) {
        periodMs = 1;
    }
    if (windowEndOffsetMs < 250) {
        windowEndOffsetMs = 250;
    }
    if (windowEndOffsetMs >= periodMs) {
        windowEndOffsetMs = periodMs > 250 ? periodMs - 250 : periodMs;
    }
    if (sampleDumpStepMs == 0) {
        sampleDumpStepMs = 1;
    }
    if (sampleDumpTailMs < sampleDumpLeadMs) {
        sampleDumpTailMs = sampleDumpLeadMs;
    }
    if (externalEmitter) {
        windowEndOffsetMs = periodMs;
    }

    free(_sequenceTest.deprecatedAnalyzerReports);
    _sequenceTest.deprecatedAnalyzerReports = nullptr;
    _sequenceTest.deprecatedAnalyzerReportCapacity = 0;
    _sequenceTest.deprecatedAnalyzerReportCount = 0;
    free(_sequenceTest.deprecatedTrialReports);
    _sequenceTest.deprecatedTrialReports = nullptr;
    _sequenceTest.deprecatedTrialReportCapacity = 0;
    _sequenceTest.deprecatedTrialReportCount = 0;

    _sequenceTest.active = true;
    _sequenceTest.quiet = quiet;
    _sequenceTest.showDetails = showDetails;
    _sequenceTest.externalEmitter = externalEmitter;
    _sequenceTest.profileKind = profileKind;
    _sequenceTest.liveFrequencyOnly = liveFrequencyOnly;
    _sequenceTest.legacyExplainOutput = legacyExplainOutput;
    _sequenceTest.progressLineStarted = false;
    _sequenceTest.logFlags = logFlags;
    _sequenceTest.totalTrials = totalTrials;
    _sequenceTest.periodMs = periodMs;
    _sequenceTest.windowStartOffsetMs = 0;
    _sequenceTest.windowEndOffsetMs = windowEndOffsetMs;
    _sequenceTest.toneHz = toneHz;
    _sequenceTest.durationMs = durationMs;
    _sequenceTest.sampleDumpEnabled = sampleDumpEnabled;
    _sequenceTest.sampleDumpFirstTrials = sampleDumpFirstTrials;
    _sequenceTest.sampleDumpEveryNth = sampleDumpEveryNth;
    _sequenceTest.sampleDumpLeadMs = sampleDumpLeadMs;
    _sequenceTest.sampleDumpTailMs = sampleDumpTailMs;
    _sequenceTest.sampleDumpStepMs = sampleDumpStepMs;
    _sequenceTest.sampleDumpMaxRows = sampleDumpMaxRows == 0 ? 1 : sampleDumpMaxRows;
    _sequenceTest.sampleDumpWarned = false;
    clearSequenceSampleDump();

    if (_detection == nullptr) {
        _detection = new detection::DetectionRuntime();
    }
    _detection->reset();
    _detection->setFrequencyTuning(_frequencyEvidenceTuning);
    const detection::DetectionProfile& selectedProfile = detection::detectionProfileForKind(_sequenceTest.profileKind);
    _detection->setAmpEnabled(selectedProfile.ampEnabled && !_sequenceTest.liveFrequencyOnly);
    _detection->setInspectionConfig(selectedProfile.inspectionConfig);
    _detection->setFieldStateConfig(selectedProfile.fieldStateConfig);
    _detection->setProfileName(detection::detectionProfileName(selectedProfile.kind));

    const bool wantLegacyReports = sequenceLegacyReportEnabled();
    const bool wantSummaryReports =
        analyzerLogEnabled(logFlags, AnalyzerApp::ANALYZER_LOG_SUMMARY);
    if (wantSummaryReports || wantLegacyReports) {
        const size_t desiredCapacity = static_cast<size_t>(totalTrials < SequenceTest::kMaxTrialReports ? totalTrials : SequenceTest::kMaxTrialReports);
        if (desiredCapacity > 0) {
            _sequenceTest.deprecatedAnalyzerReports = static_cast<AnalyzerReport*>(calloc(desiredCapacity, sizeof(AnalyzerReport)));
            if (_sequenceTest.deprecatedAnalyzerReports != nullptr) {
                _sequenceTest.deprecatedAnalyzerReportCapacity = desiredCapacity;
            } else {
                Serial.print("SEQ_VERBOSE_WARN reason=analyzer_report_alloc_failed requested=");
                Serial.print(desiredCapacity);
                Serial.println(" reports");
            }
        }
    }
    if (wantLegacyReports) {
        const size_t desiredCapacity = static_cast<size_t>(totalTrials < SequenceTest::kMaxTrialReports ? totalTrials : SequenceTest::kMaxTrialReports);
        if (desiredCapacity > 0) {
            _sequenceTest.deprecatedTrialReports = static_cast<SequenceTest::TrialReport*>(calloc(desiredCapacity, sizeof(SequenceTest::TrialReport)));
            if (_sequenceTest.deprecatedTrialReports != nullptr) {
                _sequenceTest.deprecatedTrialReportCapacity = desiredCapacity;
            } else {
                Serial.print("SEQ_VERBOSE_WARN reason=trial_report_alloc_failed requested=");
                Serial.print(desiredCapacity);
                Serial.println(" reports");
            }
        }
    }
    if (setupLabel != nullptr && setupLabel[0] != '\0') {
        strncpy(_sequenceTest.setupLabel, setupLabel, sizeof(_sequenceTest.setupLabel));
        _sequenceTest.setupLabel[sizeof(_sequenceTest.setupLabel) - 1] = '\0';
    } else {
        strncpy(_sequenceTest.setupLabel, TEST_SETUP_LABEL, sizeof(_sequenceTest.setupLabel));
        _sequenceTest.setupLabel[sizeof(_sequenceTest.setupLabel) - 1] = '\0';
    }

    if (_sequenceTest.sampleDumpEnabled) {
        const unsigned long selectedTrialsEstimate = countSelectedSampleDumpTrials(totalTrials, sampleDumpFirstTrials, sampleDumpEveryNth);
        const unsigned long rowsPerTrial = ((sampleDumpLeadMs + sampleDumpTailMs) / sampleDumpStepMs) + 1UL;
        const unsigned long requestedRows = selectedTrialsEstimate * rowsPerTrial;
        const unsigned long maxAllowedRows = _sequenceTest.sampleDumpMaxRows < SequenceTest::kMaxSampleRows
            ? _sequenceTest.sampleDumpMaxRows
            : SequenceTest::kMaxSampleRows;
        if (requestedRows > _sequenceTest.sampleDumpMaxRows || rowsPerTrial > SequenceTest::kMaxSampleRows) {
            Serial.print("SAMPLES_WARN reason=too_many_samples requested=");
            Serial.print(requestedRows);
            Serial.print(" max_allowed=");
            Serial.println(maxAllowedRows);
            _sequenceTest.sampleDumpEnabled = false;
        }
    }
    _sequenceTest.startedAtMs = millis();
    _sequenceTest.nextTriggerAtMs = _sequenceTest.startedAtMs + kSequenceWarmupMs;
    _sequenceTest.currentTrial = 0;
    _sequenceTest.currentTrialScheduledAtMs = 0;
    _sequenceTest.currentTrialStartMs = 0;
    _sequenceTest.currentTrialEndMs = 0;
    _sequenceTest.currentTrialOnsetDetectedMs = 0;
    _sequenceTest.currentTrialTransientDetectedMs = 0;
    _sequenceTest.currentTrialHit = false;
    _sequenceTest.currentTrialFinalized = false;
    _sequenceTest.currentTrialUnexpected = 0;
    _sequenceTest.currentTrialDiagnostics = {};
    _sequenceTest.hits = 0;
    _sequenceTest.expectedHits = 0;
    _sequenceTest.lateHits = 0;
    _sequenceTest.misses = 0;
    _sequenceTest.unexpected = 0;
    _sequenceTest.duplicates = 0;
    _sequenceTest.invalidAudio = 0;
    _sequenceTest.samplesProcessed = 0;
    _sequenceTest.maxSamplesPerLoop = 0;
    _sequenceTest.emptySourceLoops = 0;
    _sequenceTest.totalHitStrengthScaled = 0;
    _sequenceTest.totalHitDurationMs = 0;
    _sequenceTest.tonalExpected = 0;
    _sequenceTest.transientOnlyExpected = 0;
    _sequenceTest.tonalDuplicates = 0;
    _sequenceTest.nonTonalDuplicates = 0;
    _sequenceTest.tonalUnexpected = 0;
    _sequenceTest.nonTonalUnexpected = 0;
    _sequenceTest.freqRejectScore = 0;
    _sequenceTest.freqRejectContrast = 0;
    _sequenceTest.freqRejectBoth = 0;
    _sequenceTest.freqRejectNoEvidence = 0;
    _sequenceTest.freqRejectInvalidWindow = 0;

    if (!_sequenceTest.externalEmitter) {
        // Rebase before the first trial so every run starts from the quiet floor.
        const unsigned long sequenceClaimSendMs = millis();
        sendEmitterCommand("MODE REMOTE");
        const bool sequenceClaimAcked = waitForEmitterAck("OK MODE REMOTE", 1500);
        const unsigned long sequenceClaimAckMs = millis();
        if (_sequenceTest.showDetails && !_sequenceTest.quiet) {
            Serial.print("SEQ remote claim: send=");
            Serial.print(sequenceClaimSendMs);
            Serial.print("ms ack=");
            Serial.print(sequenceClaimAckMs);
            Serial.print("ms wait=");
            Serial.print(sequenceClaimAckMs - sequenceClaimSendMs);
            Serial.print("ms status=");
            Serial.println(sequenceClaimAcked ? "ok" : "timeout");
        }
    }

    const unsigned long sequenceRebaseStartMs = millis();
    delay(100);
    _audioSignal.rebase();
    if (_sequenceTest.showDetails && !_sequenceTest.quiet) {
        Serial.print("SEQ rebase: start=");
        Serial.print(sequenceRebaseStartMs);
        Serial.print("ms end=");
        Serial.print(millis());
        Serial.print("ms elapsed=");
        Serial.print(millis() - sequenceRebaseStartMs);
        Serial.println("ms");
    }
    resetDetectorState();
    _audioSignal.resetStats();
    _audioSource.resetStats();
    Serial.println("AUDIO stats reset");

    Serial.print("SEQ start test=");
    Serial.print(_sequenceTest.setupLabel);
    Serial.print(" warmup_ms=");
    Serial.print(kSequenceWarmupMs);
    Serial.print(" loopDelayMs=");
    Serial.print(TEST_LOOP_DELAY_MS);
    Serial.print(" logStress=");
    Serial.println(TEST_LOG_STRESS ? 1 : 0);

    if (_sequenceTest.showDetails) {
        Serial.print("SEQ start source=");
        Serial.print("I2S");
        Serial.print(" detector=AMP");
        Serial.print(" profile=");
        Serial.print(detection::detectionProfileName(_sequenceTest.profileKind));
        Serial.print(" mode=");
        if (_sequenceTest.liveFrequencyOnly) {
            Serial.print("LIVEFREQ");
        } else {
            Serial.print(_sequenceTest.externalEmitter ? "OBS" : "SEQ");
        }
        Serial.print(" liveFreqOnly=");
        Serial.print(_sequenceTest.liveFrequencyOnly ? 1 : 0);
        Serial.print(" test=");
        Serial.print(_sequenceTest.setupLabel);
        Serial.print(" warmup_ms=");
        Serial.print(kSequenceWarmupMs);
        Serial.print(" loopDelayMs=");
        Serial.print(TEST_LOOP_DELAY_MS);
        Serial.print(" logStress=");
        Serial.print(TEST_LOG_STRESS ? 1 : 0);
        Serial.print(" quiet=");
        Serial.print(_sequenceTest.quiet ? 1 : 0);
        Serial.print(" tries=");
        Serial.print(totalTrials);
        Serial.print(" period_ms=");
        Serial.print(periodMs);
        Serial.print(" window_start_ms=");
        Serial.print(_sequenceTest.windowStartOffsetMs);
        Serial.print(" window_end_ms=");
        Serial.print(windowEndOffsetMs);
        Serial.print(" freq_hz=");
        Serial.print(toneHz);
        Serial.print(" dur_ms=");
        Serial.println(durationMs);
        printDetectionParameters();
        if (!_sequenceTest.quiet) {
            Serial.println(_sequenceTest.externalEmitter ? "OBS running" : "SEQ running");
        }
    }
}

void AnalyzerApp::startCaptureSession(unsigned long totalTrials, unsigned long periodMs, unsigned long windowEndOffsetMs, unsigned long toneHz, unsigned long durationMs, bool quiet) {
    if (totalTrials == 0) {
        totalTrials = 1;
    }
    if (periodMs == 0) {
        periodMs = 1;
    }
    if (windowEndOffsetMs == 0) {
        windowEndOffsetMs = 1;
    }
    if (windowEndOffsetMs >= periodMs) {
        windowEndOffsetMs = periodMs > 1 ? periodMs - 1 : periodMs;
    }

    stopSequenceTest();

    _captureSession.active = true;
    _captureSession.quiet = quiet;
    _captureSession.totalTrials = totalTrials;
    _captureSession.periodMs = periodMs;
    _captureSession.windowStartOffsetMs = 0;
    _captureSession.windowEndOffsetMs = windowEndOffsetMs;
    _captureSession.toneHz = toneHz;
    _captureSession.durationMs = durationMs;
    _captureSession.startedAtMs = millis();
    _captureSession.nextTriggerAtMs = _captureSession.startedAtMs;
    _captureSession.currentTrial = 0;
    _captureSession.currentTrialStartMs = 0;
    _captureSession.currentTrialEndMs = 0;
    _captureSession.currentTrialFinalized = false;
    _captureSession.currentRawMin = 0;
    _captureSession.currentRawMax = 0;
    _captureSession.currentDeltaMin = 0.0f;
    _captureSession.currentDeltaMax = 0.0f;
    _captureSession.quietRawMin = 0;
    _captureSession.quietRawMax = 0;
    _captureSession.quietRawSum = 0;
    _captureSession.quietRawSamples = 0;
    _captureSession.quietDeltaMin = 0.0f;
    _captureSession.quietDeltaMax = 0.0f;
    _captureSession.quietDeltaSum = 0.0f;
    _captureSession.quietDeltaSamples = 0;
    _captureSession.completed = 0;
    _captureSession.totalRawSwing = 0;
    _captureSession.totalDeltaSwing = 0.0f;
    _captureSession.bestRawSwing = 0;
    _captureSession.bestDeltaSwing = 0.0f;
    _captureSession.lastStatusPrintMs = _captureSession.startedAtMs;

    // Capture uses the same emitter hand-off and rebase step as sequence mode.
    const unsigned long captureClaimSendMs = millis();
    sendEmitterCommand("MODE REMOTE");
    const bool captureClaimAcked = waitForEmitterAck("OK MODE REMOTE", 1500);
    const unsigned long captureClaimAckMs = millis();
    if (!_captureSession.quiet) {
        Serial.print("CAP remote claim: send=");
        Serial.print(captureClaimSendMs);
        Serial.print("ms ack=");
        Serial.print(captureClaimAckMs);
        Serial.print("ms wait=");
        Serial.print(captureClaimAckMs - captureClaimSendMs);
        Serial.print("ms status=");
        Serial.println(captureClaimAcked ? "ok" : "timeout");
    }

    const unsigned long captureRebaseStartMs = millis();
    delay(100);
    _audioSignal.rebase();
    if (!_captureSession.quiet) {
        Serial.print("CAP rebase: start=");
        Serial.print(captureRebaseStartMs);
        Serial.print("ms end=");
        Serial.print(millis());
        Serial.print("ms elapsed=");
        Serial.print(millis() - captureRebaseStartMs);
        Serial.println("ms");
    }
    resetDetectorState();
    _audioSignal.resetStats();
    _audioSource.resetStats();
    Serial.println("AUDIO stats reset");

    Serial.print("CAP start tries=");
    Serial.print(totalTrials);
    Serial.print(" period_ms=");
    Serial.print(periodMs);
    Serial.print(" window_ms=");
    Serial.print(windowEndOffsetMs);
    Serial.print(" freq_hz=");
    Serial.print(toneHz);
    Serial.print(" dur_ms=");
    Serial.println(durationMs);
    if (!_captureSession.quiet) {
        Serial.println("CAP running");
    }
}

void AnalyzerApp::stopCaptureSession() {
    _captureSession.active = false;
}

void AnalyzerApp::updateCaptureSession(unsigned long now) {
    if (_valMode) {
        return;
    }
    if (!_captureSession.active) {
        return;
    }

    const bool inTrialWindow = _captureSession.currentTrial > 0
                               && !_captureSession.currentTrialFinalized
                               && now >= _captureSession.currentTrialStartMs
                               && now <= _captureSession.currentTrialEndMs;
    if (inTrialWindow) {
        updateCaptureTrial(now);
    }

    if (_captureSession.currentTrial > 0 && now >= _captureSession.currentTrialEndMs && !_captureSession.currentTrialFinalized) {
        finalizeCaptureTrial(now);
    }

    if (!inTrialWindow) {
        updateCaptureQuietStats(now);
    }

    if (!_captureSession.active) {
        return;
    }

    if (now < _captureSession.nextTriggerAtMs) {
        return;
    }

    if (_captureSession.currentTrial >= _captureSession.totalTrials) {
        return;
    }

    const unsigned long trialNumber = _captureSession.currentTrial + 1;
    _captureSession.currentTrial = trialNumber;
    _captureSession.currentTrialStartMs = now;
    _captureSession.currentTrialEndMs = now + _captureSession.windowEndOffsetMs;
    _captureSession.currentTrialFinalized = false;
    _captureSession.nextTriggerAtMs = now + _captureSession.periodMs;

    const int raw = _audioSignal.rawSignal();
    const float delta = static_cast<float>(_audioSignal.centeredSignal());
    _captureSession.currentRawMin = raw;
    _captureSession.currentRawMax = raw;
    _captureSession.currentDeltaMin = delta;
    _captureSession.currentDeltaMax = delta;

    char command[64];
    snprintf(command, sizeof(command), "CHIRP freq=%lu dur=%lu", _captureSession.toneHz, _captureSession.durationMs);
    sendEmitterCommand(command);
}

void AnalyzerApp::updateCaptureQuietStats(unsigned long now) {
    if (!_captureSession.active) {
        return;
    }
    if (_captureSession.currentTrial > 0 && !_captureSession.currentTrialFinalized && now >= _captureSession.currentTrialStartMs && now <= _captureSession.currentTrialEndMs) {
        return;
    }

    const int raw = _audioSignal.rawSignal();
    const float delta = static_cast<float>(_audioSignal.centeredSignal());

    if (_captureSession.quietRawSamples == 0) {
        _captureSession.quietRawMin = raw;
        _captureSession.quietRawMax = raw;
        _captureSession.quietDeltaMin = delta;
        _captureSession.quietDeltaMax = delta;
    } else {
        if (raw < _captureSession.quietRawMin) {
            _captureSession.quietRawMin = raw;
        }
        if (raw > _captureSession.quietRawMax) {
            _captureSession.quietRawMax = raw;
        }
        if (delta < _captureSession.quietDeltaMin) {
            _captureSession.quietDeltaMin = delta;
        }
        if (delta > _captureSession.quietDeltaMax) {
            _captureSession.quietDeltaMax = delta;
        }
    }

    _captureSession.quietRawSum += static_cast<unsigned long>(raw);
    _captureSession.quietRawSamples++;
    _captureSession.quietDeltaSum += delta;
    _captureSession.quietDeltaSamples++;
}

void AnalyzerApp::updateCaptureTrial(unsigned long now) {
    if (!_captureSession.active || _captureSession.currentTrial == 0 || _captureSession.currentTrialFinalized) {
        return;
    }

    if (now < _captureSession.currentTrialStartMs || now > _captureSession.currentTrialEndMs) {
        return;
    }

    const int raw = _audioSignal.rawSignal();
    const float delta = static_cast<float>(_audioSignal.centeredSignal());

    if (raw < _captureSession.currentRawMin) {
        _captureSession.currentRawMin = raw;
    }
    if (raw > _captureSession.currentRawMax) {
        _captureSession.currentRawMax = raw;
    }
    if (delta < _captureSession.currentDeltaMin) {
        _captureSession.currentDeltaMin = delta;
    }
    if (delta > _captureSession.currentDeltaMax) {
        _captureSession.currentDeltaMax = delta;
    }
}

void AnalyzerApp::finalizeCaptureTrial(unsigned long now) {
    if (!_captureSession.active || _captureSession.currentTrial == 0 || _captureSession.currentTrialFinalized) {
        return;
    }

    const int rawSwing = _captureSession.currentRawMax - _captureSession.currentRawMin;
    const float deltaSwing = _captureSession.currentDeltaMax - _captureSession.currentDeltaMin;

    _captureSession.completed++;
    _captureSession.totalRawSwing += static_cast<unsigned long>(rawSwing);
    _captureSession.totalDeltaSwing += deltaSwing;
    if (rawSwing > _captureSession.bestRawSwing) {
        _captureSession.bestRawSwing = rawSwing;
    }
    if (deltaSwing > _captureSession.bestDeltaSwing) {
        _captureSession.bestDeltaSwing = deltaSwing;
    }

    Serial.print("CAP trial=");
    Serial.print(_captureSession.currentTrial);
    Serial.print(" t=");
    Serial.print(now);
    Serial.print(" rawSample_min=");
    Serial.print(_captureSession.currentRawMin);
    Serial.print(" rawSample_max=");
    Serial.print(_captureSession.currentRawMax);
    Serial.print(" rawSample_swing=");
    Serial.print(rawSwing);
    Serial.print(" centeredSample_min=");
    Serial.print(_captureSession.currentDeltaMin, 1);
    Serial.print(" centeredSample_max=");
    Serial.print(_captureSession.currentDeltaMax, 1);
    Serial.print(" centeredSample_swing=");
    Serial.println(deltaSwing, 1);

    _captureSession.currentTrialFinalized = true;

    if (_captureSession.currentTrial >= _captureSession.totalTrials) {
        stopCaptureSession();
        printCaptureSummary();
        Serial.println("CAP stopped");
    }
}

void AnalyzerApp::stopSequenceTest() {
    _sequenceTest.active = false;
    _sequenceTest.sampleDumpCapturing = false;
    free(_sequenceTest.deprecatedAnalyzerReports);
    _sequenceTest.deprecatedAnalyzerReports = nullptr;
    _sequenceTest.deprecatedAnalyzerReportCapacity = 0;
    _sequenceTest.deprecatedAnalyzerReportCount = 0;
    free(_sequenceTest.deprecatedTrialReports);
    _sequenceTest.deprecatedTrialReports = nullptr;
    _sequenceTest.deprecatedTrialReportCapacity = 0;
    _sequenceTest.deprecatedTrialReportCount = 0;
}

void AnalyzerApp::updateSequenceTest(unsigned long now) {
    if (_valMode) {
        return;
    }
    if (!_sequenceTest.active) {
        return;
    }

    if (_sequenceTest.currentTrial > 0 && now >= _sequenceTest.currentTrialEndMs) {
        finalizeSequenceTrial(now);
    }

    if (!_sequenceTest.active) {
        return;
    }

    if (_sequenceTest.currentTrial > 0) {
        updateSequenceLiveFrequencyDiagnostics(now);
    }

    if (now < _sequenceTest.nextTriggerAtMs) {
        return;
    }

    if (_sequenceTest.currentTrial >= _sequenceTest.totalTrials) {
        return;
    }

    const unsigned long trialNumber = _sequenceTest.currentTrial + 1;
    const unsigned long scheduledAtMs = _sequenceTest.nextTriggerAtMs;
    _sequenceTest.currentTrial = trialNumber;
    _sequenceTest.currentTrialScheduledAtMs = scheduledAtMs;
    _sequenceTest.currentTrialStartMs = now;
    _sequenceTest.currentTrialEndMs = now + _sequenceTest.windowEndOffsetMs;
    _sequenceTest.currentTrialOnsetDetectedMs = 0;
    _sequenceTest.currentTrialTransientDetectedMs = 0;
    _sequenceTest.currentTrialHit = false;
    _sequenceTest.currentTrialFinalized = false;
    _sequenceTest.currentTrialUnexpected = 0;
    _sequenceTest.trialHadAudioOverflow = false;
    _sequenceTest.trialOverflowCountAtStart = _audioSource.stats().overflowCount;
    _sequenceTest.trialTransientRejectTooShortCountAtStart = _audioOnsetDetector.transientRejectedDurationTooShortCount();
    _sequenceTest.trialTransientRejectTooLongCountAtStart = _audioOnsetDetector.transientRejectedDurationTooLongCount();
    _sequenceTest.trialTransientRejectWeakCountAtStart = _audioOnsetDetector.transientRejectedStrengthTooLowCount();
    _sequenceTest.currentTrialDiagnostics = {};
    _sequenceTest.currentTrialDiagnostics.acceptedAmbientBaseline = _audioSignal.baseline();
    _sequenceTest.currentTrialDiagnostics.runtimePatternCaptured = false;
    _sequenceTest.currentTrialDiagnostics.runtimePatternResult = {};
    _sequenceTest.currentTrialDiagnostics.runtimeFieldState = {};
    _sequenceTest.currentTrialDiagnostics.strongestRejectReason = AmpTransientDetector::TransientRejectReason::None;
    _sequenceTest.currentTrialDiagnostics.strongestRejectDtFromTriggerMs = -1;
    _sequenceTest.currentTrialDiagnostics.strongestRejectDurationMs = 0;
    _sequenceTest.currentTrialDiagnostics.strongestRejectStrength = 0.0f;
    _sequenceTest.liveFrequency = {};
    _sequenceTest.nextTriggerAtMs = now + _sequenceTest.periodMs;

    beginSequenceSampleDump(trialNumber);

    if (!_sequenceTest.externalEmitter) {
        char command[64];
        snprintf(command, sizeof(command), "CHIRP freq=%lu dur=%lu", _sequenceTest.toneHz, _sequenceTest.durationMs);
        sendEmitterCommand(command);
    }
}

void AnalyzerApp::handleSequenceTransient(unsigned long now) {
    if (_valMode) {
        return;
    }
    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0) {
        _sequenceTest.unexpected++;
        _sequenceTest.currentTrialUnexpected++;
        return;
    }
    if (_sequenceTest.currentTrialFinalized) {
        return;
    }

    const bool inWindow = now >= _sequenceTest.currentTrialStartMs + _sequenceTest.windowStartOffsetMs && now <= _sequenceTest.currentTrialEndMs;
    if (!inWindow) {
        _sequenceTest.unexpected++;
        _sequenceTest.currentTrialUnexpected++;
        return;
    }

    _sequenceTest.currentTrialDiagnostics.onsetSeen = true;
    if (_sequenceTest.currentTrialDiagnostics.firstOnsetMs == 0) {
        _sequenceTest.currentTrialDiagnostics.firstOnsetMs = now;
    }
    _sequenceTest.currentTrialDiagnostics.lastOnsetMs = now;
    if (_sequenceTest.currentTrialOnsetDetectedMs == 0) {
        _sequenceTest.currentTrialOnsetDetectedMs = now;
    }

    if (_sequenceTest.currentTrialHit) {
        if (_sequenceTest.currentTrialDiagnostics.duplicateCount == 0) {
            _sequenceTest.currentTrialDiagnostics.duplicateTransientMs = now;
            _sequenceTest.currentTrialDiagnostics.duplicateTransientStrength = detectorTransientStrength();
            _sequenceTest.currentTrialDiagnostics.duplicateTransientDurationMs = detectorTransientDurationMs();
            _sequenceTest.currentTrialDiagnostics.duplicateFrequencyEvidence = captureFrequencyEvidence();
            _sequenceTest.currentTrialDiagnostics.duplicateFrequencyProcessedAtMs = now;
            _sequenceTest.currentTrialDiagnostics.duplicateDeltaFromPrimaryMs = _sequenceTest.currentTrialDiagnostics.transientAccepted
                ? static_cast<long>(now) - static_cast<long>(_sequenceTest.currentTrialDiagnostics.acceptedTransientMs)
                : 0;
            _sequenceTest.currentTrialDiagnostics.duplicateOriginWindow = true;
            strncpy(_sequenceTest.currentTrialDiagnostics.duplicateReason, "duplicate_after_primary", sizeof(_sequenceTest.currentTrialDiagnostics.duplicateReason) - 1);
            _sequenceTest.currentTrialDiagnostics.duplicateReason[sizeof(_sequenceTest.currentTrialDiagnostics.duplicateReason) - 1] = '\0';
        }
        _sequenceTest.currentTrialDiagnostics.duplicateCount++;
        if (_sequenceTest.currentTrialDiagnostics.duplicateDtCount < SequenceTest::kMaxDuplicateDts) {
            _sequenceTest.currentTrialDiagnostics.duplicateDts[_sequenceTest.currentTrialDiagnostics.duplicateDtCount++] = now >= _sequenceTest.currentTrialTransientDetectedMs
                ? now - _sequenceTest.currentTrialTransientDetectedMs
                : 0;
        }
        return;
    }

    _sequenceTest.currentTrialDiagnostics.transientAccepted = true;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientMs = now;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientOnsetStrength = detectorOnsetStrength();
    _sequenceTest.currentTrialDiagnostics.acceptedTransientStrength = detectorTransientStrength();
    _sequenceTest.currentTrialDiagnostics.acceptedTransientDurationMs = detectorTransientDurationMs();
    _sequenceTest.currentTrialDiagnostics.acceptedTransientReleaseStrength = detectorTransientStrength();
    _sequenceTest.currentTrialDiagnostics.acceptedTransientPeakMs = now;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientReleaseMs = now + detectorTransientDurationMs();
    _sequenceTest.currentTrialDiagnostics.lastTransientRejectReason = AmpTransientDetector::TransientRejectReason::None;
    _sequenceTest.currentTrialDiagnostics.lastRejectStrength = 0.0f;
    _sequenceTest.currentTrialDiagnostics.lastRejectDurationMs = 0;
    _sequenceTest.currentTrialTransientDetectedMs = now;

    _sequenceTest.currentTrialHit = true;
}

void AnalyzerApp::updateSequenceAmbientStats() {
    if (_valMode) {
        return;
    }
    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0 || _sequenceTest.currentTrialFinalized) {
        return;
    }

    auto& diagnostics = _sequenceTest.currentTrialDiagnostics;
    const float baseline = _audioSignal.baseline();
    const int signalLevel = _audioSignal.smoothedSignalMagnitude();

    if (diagnostics.ambientBaselineSamples == 0) {
        diagnostics.ambientBaselineMin = baseline;
        diagnostics.ambientBaselineMax = baseline;
        diagnostics.maxSignalLevel = signalLevel;
    } else {
        if (baseline < diagnostics.ambientBaselineMin) {
            diagnostics.ambientBaselineMin = baseline;
        }
        if (baseline > diagnostics.ambientBaselineMax) {
            diagnostics.ambientBaselineMax = baseline;
        }
        if (signalLevel > diagnostics.maxSignalLevel) {
            diagnostics.maxSignalLevel = signalLevel;
        }
    }

    diagnostics.ambientBaselineSamples++;
    diagnostics.ambientBaselineSum += baseline;
}

void AnalyzerApp::updateSequenceLiveFrequencyDiagnostics(unsigned long now) {
    if (_valMode) {
        return;
    }
    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0 || _sequenceTest.currentTrialFinalized) {
        return;
    }

    auto& live = _sequenceTest.liveFrequency;
    live.liveFrequencyOnly = _sequenceTest.liveFrequencyOnly;
    const auto evidence = captureFrequencyEvidence();
    live.update(
        evidence,
        now,
        _audioSignal.stats().samplesProcessed,
        _liveFrequencyEvidenceTuning,
        kLiveFrequencyReleaseDebounceMs,
        kLiveFrequencyCooldownAfterOnsetMs,
        kLiveFrequencyMinTransientDurationMs);
}

bool AnalyzerApp::sequenceSampleDumpSelected(unsigned long trialNumber) const {
    if (!_sequenceTest.sampleDumpEnabled) {
        return false;
    }

    const bool firstSelected = _sequenceTest.sampleDumpFirstTrials > 0 && trialNumber <= _sequenceTest.sampleDumpFirstTrials;
    const bool everySelected = _sequenceTest.sampleDumpEveryNth > 0 && trialNumber % _sequenceTest.sampleDumpEveryNth == 0;
    return firstSelected || everySelected;
}

void AnalyzerApp::clearSequenceSampleDump() {
    _sequenceTest.sampleDumpSelectedForTrial = false;
    _sequenceTest.sampleDumpCapturing = false;
    _sequenceTest.sampleDumpCurrentTrial = 0;
    _sequenceTest.sampleDumpTriggerMs = 0;
    _sequenceTest.sampleDumpTriggerSampleMs = 0;
    _sequenceTest.sampleDumpCaptureStartMs = 0;
    _sequenceTest.sampleDumpCaptureEndMs = 0;
    _sequenceTest.sampleDumpNextEmitMs = 0;
    _sequenceTest.sampleRowCount = 0;
    _sequenceTest.sampleHistoryStart = 0;
    _sequenceTest.sampleHistoryCount = 0;
    _sequenceTest.sampleHistoryLastMs = 0;
    _sequenceTest.sampleHistoryHasPending = false;
    _sequenceTest.sampleHistoryPending = {};
}

void AnalyzerApp::flushSequenceSampleHistory(unsigned long currentSampleMs) {
    if (!_sequenceTest.sampleHistoryHasPending) {
        return;
    }
    if (_sequenceTest.sampleHistoryPending.sampleMs >= currentSampleMs) {
        return;
    }

    const CurveSnapshot committed = _sequenceTest.sampleHistoryPending;
    _sequenceTest.sampleHistoryHasPending = false;
    _sequenceTest.sampleHistoryPending = {};

    if (_sequenceTest.sampleHistoryCount < SequenceTest::kMaxSampleHistory) {
        const size_t index = (_sequenceTest.sampleHistoryStart + _sequenceTest.sampleHistoryCount) % SequenceTest::kMaxSampleHistory;
        _sequenceTest.sampleHistory[index] = committed;
        ++_sequenceTest.sampleHistoryCount;
    } else {
        _sequenceTest.sampleHistory[_sequenceTest.sampleHistoryStart] = committed;
        _sequenceTest.sampleHistoryStart = (_sequenceTest.sampleHistoryStart + 1) % SequenceTest::kMaxSampleHistory;
    }

    _sequenceTest.sampleHistoryLastMs = committed.sampleMs;

    if (!_sequenceTest.sampleDumpCapturing
        || !_sequenceTest.sampleDumpSelectedForTrial
        || _sequenceTest.sampleDumpCurrentTrial != _sequenceTest.currentTrial) {
        return;
    }

    if (committed.sampleMs < _sequenceTest.sampleDumpCaptureStartMs || committed.sampleMs > _sequenceTest.sampleDumpCaptureEndMs) {
        return;
    }
    if (committed.sampleMs < _sequenceTest.sampleDumpNextEmitMs) {
        return;
    }

    if (_sequenceTest.sampleRowCount >= SequenceTest::kMaxSampleRows) {
        if (!_sequenceTest.sampleDumpWarned) {
            Serial.print("SAMPLES_WARN reason=too_many_samples requested=");
            Serial.print(_sequenceTest.sampleRowCount + 1UL);
            Serial.print(" max_allowed=");
            Serial.println(SequenceTest::kMaxSampleRows);
            _sequenceTest.sampleDumpWarned = true;
        }
        _sequenceTest.sampleDumpCapturing = false;
        return;
    }

    _sequenceTest.sampleRows[_sequenceTest.sampleRowCount++] = committed;
    _sequenceTest.sampleDumpNextEmitMs = committed.sampleMs + _sequenceTest.sampleDumpStepMs;
}

void AnalyzerApp::recordSequenceSample(const CurveSnapshot& snapshot) {
    const unsigned long sampleMs = snapshot.sampleMs;

    if (!_sequenceTest.sampleHistoryHasPending) {
        _sequenceTest.sampleHistoryPending = snapshot;
        _sequenceTest.sampleHistoryHasPending = true;
        return;
    }

    if (sampleMs == _sequenceTest.sampleHistoryPending.sampleMs) {
        _sequenceTest.sampleHistoryPending = snapshot;
        return;
    }

    flushSequenceSampleHistory(sampleMs);
    _sequenceTest.sampleHistoryPending = snapshot;
    _sequenceTest.sampleHistoryHasPending = true;
}

void AnalyzerApp::beginSequenceSampleDump(unsigned long trialNumber) {
    _sequenceTest.sampleDumpSelectedForTrial = sequenceSampleDumpSelected(trialNumber);
    _sequenceTest.sampleDumpCurrentTrial = trialNumber;
    _sequenceTest.sampleDumpCapturing = _sequenceTest.sampleDumpSelectedForTrial;
    _sequenceTest.sampleDumpTriggerMs = _sequenceTest.currentTrialStartMs;
    _sequenceTest.sampleDumpTriggerSampleMs = _audioSignal.sampleTimeUs() / 1000UL;
    _sequenceTest.sampleDumpCaptureStartMs = _sequenceTest.sampleDumpTriggerSampleMs > _sequenceTest.sampleDumpLeadMs
        ? _sequenceTest.sampleDumpTriggerSampleMs - _sequenceTest.sampleDumpLeadMs
        : 0;
    _sequenceTest.sampleDumpCaptureEndMs = _sequenceTest.sampleDumpTriggerSampleMs + _sequenceTest.sampleDumpTailMs;
    _sequenceTest.sampleDumpNextEmitMs = _sequenceTest.sampleDumpCaptureStartMs;
    _sequenceTest.sampleRowCount = 0;

    flushSequenceSampleHistory(_sequenceTest.sampleDumpTriggerSampleMs + 1UL);

    if (!_sequenceTest.sampleDumpCapturing) {
        return;
    }

    for (size_t i = 0; i < _sequenceTest.sampleHistoryCount; ++i) {
        const size_t index = (_sequenceTest.sampleHistoryStart + i) % SequenceTest::kMaxSampleHistory;
        const auto& snapshot = _sequenceTest.sampleHistory[index];
        if (snapshot.sampleMs < _sequenceTest.sampleDumpCaptureStartMs) {
            continue;
        }
        if (snapshot.sampleMs > _sequenceTest.sampleDumpTriggerSampleMs) {
            break;
        }
        if (snapshot.sampleMs >= _sequenceTest.sampleDumpNextEmitMs) {
            if (_sequenceTest.sampleRowCount < SequenceTest::kMaxSampleRows) {
                _sequenceTest.sampleRows[_sequenceTest.sampleRowCount++] = snapshot;
                _sequenceTest.sampleDumpNextEmitMs = snapshot.sampleMs + _sequenceTest.sampleDumpStepMs;
            } else if (!_sequenceTest.sampleDumpWarned) {
                Serial.print("SAMPLES_WARN reason=too_many_samples requested=");
                Serial.print(_sequenceTest.sampleRowCount + 1UL);
                Serial.print(" max_allowed=");
                Serial.println(SequenceTest::kMaxSampleRows);
                _sequenceTest.sampleDumpWarned = true;
                _sequenceTest.sampleDumpCapturing = false;
                break;
            }
        }
    }
}

void AnalyzerApp::printSequenceSampleDump(unsigned long trialNumber) const {
    if (!_sequenceTest.sampleDumpEnabled || !_sequenceTest.sampleDumpSelectedForTrial || _sequenceTest.sampleDumpCurrentTrial != trialNumber) {
        return;
    }

    Serial.print("SAMPLES_BEGIN trial=");
    Serial.print(trialNumber);
    Serial.print(" trigger_ms=");
    Serial.print(_sequenceTest.sampleDumpTriggerMs);
    Serial.print(" sample_rate_ms=");
    Serial.print(_sequenceTest.sampleDumpStepMs);
    Serial.print(" fields=t,current,env,peak,open onset=");
    Serial.print(detectorOnsetDetectionThreshold(), 1);
    Serial.print(" release=");
    Serial.print(detectorOnsetReleaseThreshold(), 1);
    Serial.print(" minStrength=");
    Serial.print(detectorMinTransientPeakStrength(), 1);
    Serial.print(" minMs=");
    Serial.print(detectorMinTransientDurationMs());
    Serial.print(" maxMs=");
    Serial.print(detectorMaxTransientDurationMs());
    Serial.println();

    for (size_t i = 0; i < _sequenceTest.sampleRowCount; ++i) {
        const auto& sample = _sequenceTest.sampleRows[i];
        const long tMs = static_cast<long>(sample.sampleMs) - static_cast<long>(_sequenceTest.sampleDumpTriggerSampleMs);
        Serial.print(tMs);
        Serial.print(",");
        Serial.print(sample.current);
        Serial.print(",");
        Serial.print(sample.env);
        Serial.print(",");
        Serial.print(sample.peak, 1);
        Serial.print(",");
        Serial.println(sample.open ? 1 : 0);
    }

    Serial.print("SAMPLES_END trial=");
    Serial.println(trialNumber);
}

void AnalyzerApp::sequenceCurveSampleCallback(const CurveSnapshot& snapshot, void* context) {
    auto* self = static_cast<AnalyzerApp*>(context);
    if (self == nullptr) {
        return;
    }
    self->recordSequenceSample(snapshot);
}

detection::FrequencyEvidence AnalyzerApp::captureFrequencyEvidence() const {
    detection::FrequencyEvidence evidence;
    evidence.observedAtMs = millis();
    const bool present = _freqBandStream.windowReady();
    const float totalEnergy = _freqBandStream.lastTotalEnergy();

    evidence.present = present;
    evidence.matched = false;
    evidence.targetHz = present ? _freqBandStream.targetFrequencyHz() : 0;
    evidence.windowSampleCount = _freqBandStream.sampleCount();
    evidence.windowAvailable = present;
    evidence.score = _freqBandStream.lastFrequencyScore();
    evidence.confidence = 0.0f;
    evidence.targetPower = _freqBandStream.lastTargetPower();
    evidence.neighborPower = _freqBandStream.lastNeighborPower();
    evidence.totalEnergy = totalEnergy;
    evidence.spectralContrast = _freqBandStream.lastSpectralContrast();
    evidence.validWindow = present;
    return evidence;
}

detection::FrequencyEvidence AnalyzerApp::scanSequenceFrequencyParity64(const detection::PatternCandidate& patternCandidate, unsigned long observedAtMs) const {
    detection::FrequencyEvidence evidence;
    DetectorCandidate detectorCandidate;
    detectorCandidate.onsetSample = patternCandidate.onsetSample;
    detectorCandidate.peakSample = patternCandidate.peakSample;
    detectorCandidate.releaseSample = patternCandidate.releaseSample;
    detectorCandidate.onsetMillisApprox = patternCandidate.startMs;
    detectorCandidate.releaseMillisApprox = patternCandidate.startMs + patternCandidate.durationMs;
    detectorCandidate.onsetStrength = patternCandidate.onsetStrength;
    detectorCandidate.peakStrength = patternCandidate.peakStrength;
    detectorCandidate.releaseStrength = patternCandidate.releaseStrength;
    detectorCandidate.ambientBaseline = patternCandidate.ambientBaseline;
    detectorCandidate.durationMs = patternCandidate.durationMs;
    detectorCandidate.audioOverflowDuringCandidate = patternCandidate.audioOverflowDuringCandidate;

    const unsigned long sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
    if (detection::measureCandidateWindowFrequencyParityScan64(_audioSignal,
                                                                       detectorCandidate,
                                                                       sampleRateHz,
                                                                       _freqBandStream.targetFrequencyHz(),
                                                                       observedAtMs,
                                                                       evidence,
                                                                       64UL)) {
        return evidence;
    }

    return {};
}

void AnalyzerApp::noteSequenceTransientReject(unsigned long eventMs) {
    if (_valMode) {
        return;
    }
    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0 || _sequenceTest.currentTrialFinalized) {
        return;
    }

    const char* reasonName = detectorTransientRejectReasonName();
    if (strcmp(reasonName, "none") == 0) {
        return;
    }

    noteSequenceTransientRejectReason(eventMs, reasonName, detectorTransientRejectedDurationMs(), detectorTransientRejectedStrength());
}

void AnalyzerApp::noteSequenceTransientRejectReason(unsigned long eventMs, const char* reasonName, unsigned long durationMs, float strength) {
    if (_valMode) {
        return;
    }
    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0 || _sequenceTest.currentTrialFinalized) {
        return;
    }

    auto& diagnostics = _sequenceTest.currentTrialDiagnostics;
    const unsigned long shortCount = _audioOnsetDetector.transientRejectedDurationTooShortCount() - _sequenceTest.trialTransientRejectTooShortCountAtStart;
    const unsigned long longCount = _audioOnsetDetector.transientRejectedDurationTooLongCount() - _sequenceTest.trialTransientRejectTooLongCountAtStart;
    const unsigned long weakCount = _audioOnsetDetector.transientRejectedStrengthTooLowCount() - _sequenceTest.trialTransientRejectWeakCountAtStart;

    diagnostics.transientRejectTooShortCount = shortCount;
    diagnostics.transientRejectTooLongCount = longCount;
    diagnostics.transientRejectWeakCount = weakCount;

    const AmpTransientDetector::TransientRejectReason reason =
        strcmp(reasonName, "duration_too_short") == 0 ? AmpTransientDetector::TransientRejectReason::DurationTooShort :
        strcmp(reasonName, "duration_too_long") == 0 ? AmpTransientDetector::TransientRejectReason::DurationTooLong :
        strcmp(reasonName, "strength_too_low") == 0 ? AmpTransientDetector::TransientRejectReason::StrengthTooLow :
        strcmp(reasonName, "peak_still_active") == 0 ? AmpTransientDetector::TransientRejectReason::PeakStillActive :
        AmpTransientDetector::TransientRejectReason::None;

    if (reason != AmpTransientDetector::TransientRejectReason::None && strength >= diagnostics.strongestRejectStrength) {
        diagnostics.strongestRejectReason = reason;
        diagnostics.strongestRejectDtFromTriggerMs = static_cast<long>(eventMs - _sequenceTest.currentTrialStartMs);
        diagnostics.strongestRejectDurationMs = durationMs;
        diagnostics.strongestRejectStrength = strength;
    }
}

const char* AnalyzerApp::sequenceTrialClassificationName(const char* result, long dtMs, long durMs, const SequenceTest::TrialDiagnostics& diagnostics) const {
    if (strcmp(result, "invalid_audio") == 0) {
        return "invalid_audio";
    }
    if (strcmp(result, "unexpected") == 0) {
        return "unexpected";
    }
    if (strcmp(result, "late") == 0) {
        return "late";
    }
    if (strcmp(result, "miss") == 0) {
        switch (diagnostics.strongestRejectReason) {
            case AmpTransientDetector::TransientRejectReason::DurationTooLong:
                return "miss_too_long";
            case AmpTransientDetector::TransientRejectReason::StrengthTooLow:
                return "miss_weak";
            case AmpTransientDetector::TransientRejectReason::None:
            case AmpTransientDetector::TransientRejectReason::DurationTooShort:
            case AmpTransientDetector::TransientRejectReason::PeakStillActive:
                return "miss_no_onset";
        }
    }
    if (dtMs >= kLateOnsetMinMs) {
        return "late";
    }
    if (durMs >= kTooLongDurationMinMs) {
        return "expected_too_long";
    }
    if (durMs >= kSmearedDurationMinMs && durMs <= kSmearedDurationMaxMs) {
        return "expected_smeared";
    }
    if (durMs >= kCleanDurationMinMs && durMs <= kCleanDurationMaxMs) {
        return "expected_clean";
    }
    return "expected_clean";
}

void AnalyzerApp::recordSequenceClassifierOutcome(const detection::PatternResult& patternResult, bool duplicateCandidate, bool unexpectedCandidate) {
    if (_valMode || !patternResult.candidateValid) {
        return;
    }

    const auto freqEval = FrequencyEvidenceEvaluation::evaluate(patternResult.freq, _frequencyEvidenceTuning);

    if (unexpectedCandidate) {
        if (patternResult.tonalValid) {
            ++_sequenceTest.tonalUnexpected;
        } else {
            ++_sequenceTest.nonTonalUnexpected;
        }
    } else if (duplicateCandidate) {
        if (patternResult.tonalValid) {
            ++_sequenceTest.tonalDuplicates;
        } else {
            ++_sequenceTest.nonTonalDuplicates;
        }
    } else {
        if (patternResult.tonalValid) {
            ++_sequenceTest.tonalExpected;
        } else {
            ++_sequenceTest.transientOnlyExpected;
        }
    }

    switch (freqEval.reason) {
        case FrequencyEvidenceEvaluation::Reason::None:
            break;
        case FrequencyEvidenceEvaluation::Reason::NoEvidence:
            ++_sequenceTest.freqRejectNoEvidence;
            break;
        case FrequencyEvidenceEvaluation::Reason::InvalidWindow:
            ++_sequenceTest.freqRejectInvalidWindow;
            break;
        case FrequencyEvidenceEvaluation::Reason::ScoreTooLow:
            ++_sequenceTest.freqRejectScore;
            break;
        case FrequencyEvidenceEvaluation::Reason::ContrastTooLow:
            ++_sequenceTest.freqRejectContrast;
            break;
        case FrequencyEvidenceEvaluation::Reason::ScoreAndContrastTooLow:
            ++_sequenceTest.freqRejectBoth;
            break;
    }
}

void AnalyzerApp::handleSequenceCandidate(const detection::PatternResult& patternResult, unsigned long queueDepthBeforeDrain, const detection::FrequencyEvidence* liveFrequencyEvidence) {
    if (_valMode) {
        return;
    }

    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0) {
        return;
    }

    auto& diagnostics = _sequenceTest.currentTrialDiagnostics;
    diagnostics.rawCandidateCount++;

    const auto& candidate = patternResult.candidate;
    const unsigned long candidateIdx = diagnostics.rawCandidateCount;
    const unsigned long onsetMs = candidate.startMs;
    const long dtFromTriggerMs = static_cast<long>(onsetMs) - static_cast<long>(_sequenceTest.currentTrialStartMs);
    const long dtFromTrialStartMs = static_cast<long>(onsetMs) - static_cast<long>(_sequenceTest.currentTrialScheduledAtMs);
    const long processLagMs = patternResult.processedAtMs >= onsetMs
        ? static_cast<long>(patternResult.processedAtMs - onsetMs)
        : -1;
    const uint32_t sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
    const unsigned long peakOffsetMs = candidate.peakSample >= candidate.onsetSample
        ? static_cast<unsigned long>(((candidate.peakSample - candidate.onsetSample) * 1000ULL) / static_cast<uint64_t>(sampleRateHz))
        : 0UL;

    const bool overflowSeenNow = candidate.audioOverflowDuringCandidate
                                 || _audioSource.stats().overflowCount != _sequenceTest.trialOverflowCountAtStart;
    if (overflowSeenNow) {
        _sequenceTest.trialHadAudioOverflow = true;
    }

    const bool preWindow = !_sequenceTest.externalEmitter && onsetMs < _sequenceTest.currentTrialStartMs + _sequenceTest.windowStartOffsetMs;
    const bool postWindow = !_sequenceTest.externalEmitter && onsetMs > _sequenceTest.currentTrialEndMs;
    const bool inWindow = _sequenceTest.externalEmitter || (!preWindow && !postWindow);
    const bool duplicateCandidate = _sequenceTest.currentTrialHit && inWindow;
    const char* candidateClass = h3SequenceCandidateClass(duplicateCandidate, inWindow, dtFromTriggerMs);

    const SequenceTest::CandidateOrigin origin = preWindow
        ? SequenceTest::CandidateOrigin::PreWindow
        : postWindow
            ? SequenceTest::CandidateOrigin::PostWindow
            : SequenceTest::CandidateOrigin::InWindow;

    if (diagnostics.firstCandidateMs == 0) {
        diagnostics.firstCandidateMs = onsetMs;
    }

    if (diagnostics.candidateCount < SequenceTest::kMaxTrialCandidates) {
        auto& entry = diagnostics.candidates[diagnostics.candidateCount++];
        entry.candidateMs = onsetMs;
        entry.dtFromTriggerMs = dtFromTriggerMs;
        entry.dtFromTrialStartMs = dtFromTrialStartMs;
        entry.durationMs = candidate.durationMs;
        entry.strength = candidate.peakStrength;
        entry.origin = origin;
    } else {
        diagnostics.candidateOverflowCount++;
    }

    if (origin == SequenceTest::CandidateOrigin::PreWindow) {
        diagnostics.candidatePreWindowCount++;
    } else if (origin == SequenceTest::CandidateOrigin::InWindow) {
        diagnostics.candidateInWindowCount++;
    } else {
        diagnostics.candidatePostWindowCount++;
    }

    recordSequenceClassifierOutcome(patternResult, duplicateCandidate, !inWindow);

    if (!diagnostics.bestCandidateValid || candidate.peakStrength > diagnostics.bestCandidateStrength) {
        diagnostics.bestCandidateValid = true;
        diagnostics.bestCandidateDtFromTriggerMs = dtFromTriggerMs;
        diagnostics.bestCandidateDurationMs = candidate.durationMs;
        diagnostics.bestCandidateStrength = candidate.peakStrength;
        diagnostics.bestCandidateOrigin = origin;
    }

    if (analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_CANDIDATE) && !_sequenceTest.quiet) {
        const uint32_t sampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
        const unsigned long peakOffsetMs = candidate.peakSample >= candidate.onsetSample
            ? static_cast<unsigned long>(((candidate.peakSample - candidate.onsetSample) * 1000ULL) / static_cast<uint64_t>(sampleRateHz))
            : 0UL;
        Serial.print("SEQ_CAND role=detector trial=");
        Serial.print(_sequenceTest.currentTrial);
        Serial.print(" idx=");
        Serial.print(candidateIdx);
        Serial.print(" onset_ms=");
        Serial.print(onsetMs);
        Serial.print(" onset_sample=");
        Serial.print(candidate.onsetSample);
        Serial.print(" peak_sample=");
        Serial.print(candidate.peakSample);
        Serial.print(" release_sample=");
        Serial.print(candidate.releaseSample);
        Serial.print(" onset_dt_ms=");
        Serial.print(dtFromTriggerMs);
        Serial.print(" peak_ms=");
        Serial.print(candidate.startMs + peakOffsetMs);
        Serial.print(" dur=");
        Serial.print(candidate.durationMs);
        Serial.print(" end_dt_ms=");
        if (dtFromTriggerMs >= 0) {
            Serial.print(dtFromTriggerMs + static_cast<long>(candidate.durationMs));
            Serial.print("ms");
        } else {
            Serial.print("-");
        }
        Serial.print(" processed_at_ms=");
        Serial.print(patternResult.processedAtMs);
        Serial.print(" process_lag_ms=");
        if (processLagMs >= 0) {
            Serial.print(processLagMs);
            Serial.print("ms");
        } else {
            Serial.print("-");
        }
        Serial.print(" queue_before=");
        Serial.print(queueDepthBeforeDrain);
        Serial.print(" strength=");
        Serial.print(candidate.peakStrength, 1);
        Serial.print(" transient_present=");
        Serial.print(patternResult.candidate.transient.present ? 1 : 0);
        Serial.print(" freq_present=");
        Serial.print(patternResult.freq.present ? 1 : 0);
        Serial.print(" freq_matched=");
        Serial.print(patternResult.freq.matched ? 1 : 0);
        Serial.print(" freq_score=");
        Serial.print(patternResult.freq.score, 1);
        Serial.print(" freq_conf=");
        Serial.print(patternResult.freq.confidence, 1);
        Serial.print(" freq_target_hz=");
        Serial.print(patternResult.freq.targetHz);
        Serial.print(" freq_contrast=");
        Serial.print(patternResult.freq.spectralContrast, 1);
        printH3FrequencyEvidenceFields(patternResult, patternResult.freq, liveFrequencyEvidence, _frequencyEvidenceTuning, candidateClass, dtFromTriggerMs, patternResult.processedAtMs);
        Serial.print(" source=");
        Serial.println(detection::patternSourceName(patternResult.source));

        Serial.print("SEQ_CAND role=pattern trial=");
        Serial.print(_sequenceTest.currentTrial);
        Serial.print(" idx=");
        Serial.print(candidateIdx);
        Serial.print(" onset_ms=");
        Serial.print(onsetMs);
        Serial.print(" onset_sample=");
        Serial.print(candidate.onsetSample);
        Serial.print(" peak_sample=");
        Serial.print(candidate.peakSample);
        Serial.print(" release_sample=");
        Serial.print(candidate.releaseSample);
        Serial.print(" onset_dt_ms=");
        Serial.print(dtFromTriggerMs);
        Serial.print(" peak_ms=");
        Serial.print(candidate.startMs + peakOffsetMs);
        Serial.print(" dur=");
        Serial.print(candidate.durationMs);
        Serial.print(" end_dt_ms=");
        if (dtFromTriggerMs >= 0) {
            Serial.print(dtFromTriggerMs + static_cast<long>(candidate.durationMs));
            Serial.print("ms");
        } else {
            Serial.print("-");
        }
        Serial.print(" processed_at_ms=");
        Serial.print(patternResult.processedAtMs);
        Serial.print(" process_lag_ms=");
        if (processLagMs >= 0) {
            Serial.print(processLagMs);
            Serial.print("ms");
        } else {
            Serial.print("-");
        }
        Serial.print(" queue_before=");
        Serial.print(queueDepthBeforeDrain);
        Serial.print(" strength=");
        Serial.print(candidate.peakStrength, 1);
        Serial.print(" transient_present=");
        Serial.print(patternResult.candidate.transient.present ? 1 : 0);
        Serial.print(" freq_present=");
        Serial.print(patternResult.freq.present ? 1 : 0);
        Serial.print(" freq_matched=");
        Serial.print(patternResult.freq.matched ? 1 : 0);
        Serial.print(" freq_score=");
        Serial.print(patternResult.freq.score, 1);
        Serial.print(" freq_conf=");
        Serial.print(patternResult.freq.confidence, 1);
        Serial.print(" freq_target_hz=");
        Serial.print(patternResult.freq.targetHz);
        Serial.print(" freq_contrast=");
        Serial.print(patternResult.freq.spectralContrast, 1);
        printH3FrequencyEvidenceFields(patternResult, patternResult.freq, liveFrequencyEvidence, _frequencyEvidenceTuning, candidateClass, dtFromTriggerMs, patternResult.processedAtMs);
        Serial.print(" source=");
        Serial.println(detection::patternSourceName(patternResult.source));
    }

    if (!inWindow) {
        if (!_sequenceTest.trialHadAudioOverflow) {
            _sequenceTest.unexpected++;
            _sequenceTest.currentTrialUnexpected++;
        }
        return;
    }

    _sequenceTest.currentTrialDiagnostics.onsetSeen = true;
    if (_sequenceTest.currentTrialDiagnostics.firstOnsetMs == 0) {
        _sequenceTest.currentTrialDiagnostics.firstOnsetMs = onsetMs;
    }
    _sequenceTest.currentTrialDiagnostics.lastOnsetMs = onsetMs;
    if (_sequenceTest.currentTrialOnsetDetectedMs == 0) {
        _sequenceTest.currentTrialOnsetDetectedMs = onsetMs;
    }

    if (_sequenceTest.currentTrialHit) {
        if (diagnostics.duplicateCount == 0) {
            diagnostics.duplicateTransientMs = onsetMs;
            diagnostics.duplicateTransientStrength = candidate.peakStrength;
            diagnostics.duplicateTransientDurationMs = candidate.durationMs;
            diagnostics.duplicateTransientOnsetSample = candidate.onsetSample;
            diagnostics.duplicateTransientPeakSample = candidate.peakSample;
            diagnostics.duplicateTransientReleaseSample = candidate.releaseSample;
            diagnostics.duplicateTransientPeakMs = candidate.startMs + peakOffsetMs;
            diagnostics.duplicateTransientReleaseMs = candidate.startMs + candidate.durationMs;
            diagnostics.duplicateFrequencyEvidence = patternResult.freq;
            diagnostics.duplicateFrequencyEvidenceFull = patternResult.freqFull;
            diagnostics.duplicateFrequencyProcessedAtMs = patternResult.processedAtMs;
            diagnostics.duplicateParityProbe64 = scanSequenceFrequencyParity64(patternResult.candidate, patternResult.processedAtMs);
            diagnostics.duplicateParityProbe64ProcessedAtMs = patternResult.processedAtMs;
            diagnostics.duplicateDeltaFromPrimaryMs = diagnostics.transientAccepted
                ? static_cast<long>(onsetMs) - static_cast<long>(diagnostics.acceptedTransientMs)
                : 0;
            strncpy(diagnostics.duplicateReason, "duplicate_after_primary", sizeof(diagnostics.duplicateReason) - 1);
            diagnostics.duplicateReason[sizeof(diagnostics.duplicateReason) - 1] = '\0';
        }
        _sequenceTest.currentTrialDiagnostics.duplicateCount++;
        if (_sequenceTest.currentTrialDiagnostics.duplicateDtCount < SequenceTest::kMaxDuplicateDts) {
            _sequenceTest.currentTrialDiagnostics.duplicateDts[_sequenceTest.currentTrialDiagnostics.duplicateDtCount++] = onsetMs >= _sequenceTest.currentTrialTransientDetectedMs
                ? onsetMs - _sequenceTest.currentTrialTransientDetectedMs
                : 0;
        }
        return;
    }

    _sequenceTest.currentTrialDiagnostics.transientAccepted = true;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientMs = onsetMs;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientOnsetStrength = candidate.onsetStrength;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientStrength = candidate.peakStrength;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientDurationMs = candidate.durationMs;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientReleaseStrength = candidate.releaseStrength;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientOnsetSample = candidate.onsetSample;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientPeakSample = candidate.peakSample;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientReleaseSample = candidate.releaseSample;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientPeakMs = candidate.startMs + peakOffsetMs;
    _sequenceTest.currentTrialDiagnostics.acceptedTransientReleaseMs = candidate.startMs + candidate.durationMs;
    _sequenceTest.currentTrialDiagnostics.acceptedAmbientBaseline = candidate.ambientBaseline;
    _sequenceTest.currentTrialDiagnostics.acceptedFrequencyEvidence = patternResult.freq;
    _sequenceTest.currentTrialDiagnostics.acceptedFrequencyEvidenceFull = patternResult.freqFull;
    _sequenceTest.currentTrialDiagnostics.acceptedFrequencyProcessedAtMs = patternResult.processedAtMs;
    _sequenceTest.currentTrialDiagnostics.acceptedParityProbe64 = scanSequenceFrequencyParity64(patternResult.candidate, patternResult.processedAtMs);
    _sequenceTest.currentTrialDiagnostics.acceptedParityProbe64ProcessedAtMs = patternResult.processedAtMs;
            _sequenceTest.currentTrialDiagnostics.deprecatedAcceptedSignalCandidate = makeModernSignalCandidateFromPatternResult(patternResult);
    // Legacy analyzer-side recheck removed from the normal path.
    // DetectionRuntime owns the PatternResult handed to the analyzer now.
    _sequenceTest.currentTrialDiagnostics.deprecatedAcceptedPatternCaptured = false;
    _sequenceTest.currentTrialDiagnostics.lastTransientRejectReason = AmpTransientDetector::TransientRejectReason::None;
    _sequenceTest.currentTrialDiagnostics.lastRejectStrength = 0.0f;
    _sequenceTest.currentTrialDiagnostics.lastRejectDurationMs = 0;
    _sequenceTest.currentTrialTransientDetectedMs = onsetMs;

    _sequenceTest.currentTrialHit = true;

    if (analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_CANDIDATE) && !_sequenceTest.quiet) {
        Serial.print("SEQ_CAND role=result trial=");
        Serial.print(_sequenceTest.currentTrial);
        Serial.print(" primary_idx=");
        Serial.print(candidateIdx);
        Serial.print(" onset_ms=");
        Serial.print(candidate.startMs);
        Serial.print(" onset_sample=");
        Serial.print(candidate.onsetSample);
        Serial.print(" peak_sample=");
        Serial.print(candidate.peakSample);
        Serial.print(" release_sample=");
        Serial.print(candidate.releaseSample);
        Serial.print(" onset_dt_ms=");
        Serial.print(dtFromTriggerMs);
        Serial.print(" peak_ms=");
        Serial.print(candidate.startMs + peakOffsetMs);
        Serial.print(" dur=");
        Serial.print(candidate.durationMs);
        Serial.print(" end_dt_ms=");
        if (dtFromTriggerMs >= 0) {
            Serial.print(dtFromTriggerMs + static_cast<long>(candidate.durationMs));
            Serial.print("ms");
        } else {
            Serial.print("-");
        }
        Serial.print(" processed_at_ms=");
        Serial.print(patternResult.processedAtMs);
        Serial.print(" process_lag_ms=");
        if (processLagMs >= 0) {
            Serial.print(processLagMs);
            Serial.print("ms");
        } else {
            Serial.print("-");
        }
        Serial.print(" queue_before=");
        Serial.print(queueDepthBeforeDrain);
        Serial.print(" strength=");
        Serial.print(candidate.peakStrength, 1);
        Serial.print(" transient_present=");
        Serial.print(patternResult.candidate.transient.present ? 1 : 0);
        Serial.print(" freq_present=");
        Serial.print(patternResult.freq.present ? 1 : 0);
        Serial.print(" freq_matched=");
        Serial.print(patternResult.freq.matched ? 1 : 0);
        Serial.print(" freq_score=");
        Serial.print(patternResult.freq.score, 1);
        Serial.print(" freq_conf=");
        Serial.print(patternResult.freq.confidence, 1);
        Serial.print(" freq_target_hz=");
        Serial.print(patternResult.freq.targetHz);
        Serial.print(" freq_contrast=");
        Serial.print(patternResult.freq.spectralContrast, 1);
        Serial.print(" reason=");
        Serial.print(detection::patternReasonName(patternResult.reasonCode));
        printH3FrequencyEvidenceFields(patternResult, patternResult.freq, liveFrequencyEvidence, _frequencyEvidenceTuning, candidateClass, dtFromTriggerMs, patternResult.processedAtMs);
        Serial.println();
    }
}

namespace {

// Legacy parity check for Pass J validation only.
// Keep disabled in the normal runtime path now that Pass K trusts runtime output.
static constexpr bool kAnalyzerEnableRecheckParity = false;

struct PatternResultParity {
    bool compared = false;
    bool match = true;
    bool acceptedMatch = true;
    bool typeMatch = true;
    bool sourceMatch = true;
    bool reasonMatch = true;
    bool timingClose = true;
    bool confidenceClose = true;
    float confidenceDelta = 0.0f;
    long timingDeltaMs = 0;
    const char* summary = "none";
    const char* reason = "none";
};

static PatternResultParity comparePatternResultsForAnalyzer(
    const detection::PatternResult& actual,
    const detection::PatternResult& rechecked,
    long actualDtMs,
    long recheckedDtMs
) {
    PatternResultParity parity = {};
    parity.compared = true;
    parity.acceptedMatch = actual.valid == rechecked.valid;
    parity.typeMatch = actual.type == rechecked.type;
    parity.sourceMatch = actual.source == rechecked.source;
    parity.reasonMatch = actual.reasonCode == rechecked.reasonCode;
    parity.timingDeltaMs = actualDtMs - recheckedDtMs;
    if (parity.timingDeltaMs < 0) {
        parity.timingDeltaMs = -parity.timingDeltaMs;
    }
    parity.timingClose = parity.timingDeltaMs <= 25;
    parity.confidenceDelta = fabsf(actual.confidence - rechecked.confidence);
    parity.confidenceClose = parity.confidenceDelta <= 0.10f;

    parity.match = parity.acceptedMatch &&
        parity.typeMatch &&
        parity.sourceMatch &&
        parity.reasonMatch &&
        parity.timingClose &&
        parity.confidenceClose;

    if (parity.match) {
        parity.summary = "none";
    } else {
        char summary[96];
        summary[0] = '\0';
        auto appendSummary = [&](const char* tag) {
            if (summary[0] != '\0') {
                strncat(summary, ",", sizeof(summary) - strlen(summary) - 1);
            }
            strncat(summary, tag, sizeof(summary) - strlen(summary) - 1);
        };
        if (!parity.acceptedMatch) appendSummary("accepted_mismatch");
        if (!parity.typeMatch) appendSummary("type_mismatch");
        if (!parity.sourceMatch) appendSummary("source_mismatch");
        if (!parity.reasonMatch) appendSummary("reason_mismatch");
        if (!parity.timingClose) appendSummary("timing_mismatch");
        if (!parity.confidenceClose) appendSummary("confidence_mismatch");
        if (summary[0] == '\0') {
            appendSummary("unknown_mismatch");
        }
        static char summaryStorage[96];
        strncpy(summaryStorage, summary, sizeof(summaryStorage) - 1);
        summaryStorage[sizeof(summaryStorage) - 1] = '\0';
        parity.summary = summaryStorage;
    }
    return parity;
}

} // namespace

void AnalyzerApp::finalizeSequenceTrial(unsigned long now) {
    if (_valMode) {
        return;
    }
    if (_sequenceTest.currentTrial == 0) {
        return;
    }

    if (_sequenceTest.currentTrialFinalized) {
        return;
    }

    auto& diagnostics = _sequenceTest.currentTrialDiagnostics;
    auto& live = _sequenceTest.liveFrequency;

    const bool promoteLiveFrequency = live.frequencyCandidate.valid && !_sequenceTest.currentTrialHit;
    if ((_sequenceTest.liveFrequencyOnly && live.candidateEmitted) || promoteLiveFrequency) {
        live.candidateActive = false;
        live.candidateClosed = true;
        live.candidateEmitted = true;
        if (live.candidateReleaseMs == 0) {
            live.candidateReleaseMs = live.candidateLastMatchedMs > 0 ? live.candidateLastMatchedMs : now;
        }
        if (live.candidateReleaseSample == 0) {
            live.candidateReleaseSample = _audioSignal.stats().samplesProcessed;
        }
        live.candidateHoldMs = live.candidateReleaseMs >= live.candidateFirstSeenMs
            ? live.candidateReleaseMs - live.candidateFirstSeenMs
            : 0UL;
        live.candidateState[0] = '\0';
        strncpy(live.candidateState, "closed", sizeof(live.candidateState) - 1);
        live.candidateState[sizeof(live.candidateState) - 1] = '\0';

        if (analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_TRIAL)) {
            Serial.print("SEQ_FREQ_CAND trial=");
            Serial.print(_sequenceTest.currentTrial);
            Serial.print(" state=closed");
            Serial.print(" source=");
            Serial.print(sequenceFrequencyCandidateSourceName(live.frequencyCandidate.valid));
            Serial.print(" first_seen=");
            Serial.print(live.candidateFirstSeenMs);
            Serial.print("ms peak=");
            Serial.print(live.candidatePeakMs);
            Serial.print("ms release=");
            Serial.print(live.candidateReleaseMs);
            Serial.print("ms dur=");
            Serial.print(live.candidateHoldMs);
            Serial.print("ms score=");
            Serial.print(live.candidatePeakScore, 1);
            Serial.print(" contrast=");
            Serial.print(live.candidatePeakContrast, 2);
            Serial.print(" ready=");
            Serial.print(live.readyOk ? 1 : 0);
            Serial.println();
        }

        if (live.frequencyCandidate.valid) {
            diagnostics.transientAccepted = true;
            diagnostics.acceptedTransientMs = live.candidateFirstSeenMs;
            diagnostics.acceptedTransientStrength = live.candidatePeakScore;
            diagnostics.acceptedTransientDurationMs = live.candidateHoldMs;
            diagnostics.acceptedTransientOnsetStrength = live.candidatePeakContrast;
            diagnostics.acceptedTransientReleaseStrength = live.candidatePeakContrast;
            diagnostics.acceptedTransientOnsetSample = live.candidateFirstSeenSample;
            diagnostics.acceptedTransientPeakSample = live.candidatePeakSample;
            diagnostics.acceptedTransientReleaseSample = live.candidateReleaseSample;
            diagnostics.acceptedTransientPeakMs = live.candidatePeakMs;
            diagnostics.acceptedTransientReleaseMs = live.candidateReleaseMs;
            diagnostics.acceptedAmbientBaseline = _audioSignal.baseline();
            diagnostics.acceptedFrequencyEvidence = live.candidateEvidence;
            diagnostics.acceptedFrequencyEvidenceFull = live.candidateEvidence;
            diagnostics.acceptedFrequencyProcessedAtMs = live.candidatePeakMs;
            diagnostics.acceptedParityProbe64 = live.candidateEvidence;
            diagnostics.acceptedParityProbe64ProcessedAtMs = live.candidatePeakMs;
            diagnostics.lastTransientRejectReason = AmpTransientDetector::TransientRejectReason::None;
            diagnostics.lastRejectStrength = 0.0f;
            diagnostics.lastRejectDurationMs = 0;
            _sequenceTest.currentTrialTransientDetectedMs = live.candidateFirstSeenMs;
            _sequenceTest.currentTrialHit = true;
        }
    }

    diagnostics.peakActiveAtEnd = detectorTransientPeakActive();
    const char* transientRejectReason = detectorTransientRejectReasonName();
    if (strcmp(transientRejectReason, "duration_too_short") == 0) {
        diagnostics.lastTransientRejectReason = AmpTransientDetector::TransientRejectReason::DurationTooShort;
    } else if (strcmp(transientRejectReason, "duration_too_long") == 0) {
        diagnostics.lastTransientRejectReason = AmpTransientDetector::TransientRejectReason::DurationTooLong;
    } else if (strcmp(transientRejectReason, "strength_too_low") == 0) {
        diagnostics.lastTransientRejectReason = AmpTransientDetector::TransientRejectReason::StrengthTooLow;
    } else if (strcmp(transientRejectReason, "peak_still_active") == 0) {
        diagnostics.lastTransientRejectReason = AmpTransientDetector::TransientRejectReason::PeakStillActive;
    } else {
        diagnostics.lastTransientRejectReason = AmpTransientDetector::TransientRejectReason::None;
    }
    diagnostics.lastRejectDurationMs = detectorTransientRejectedDurationMs();
    diagnostics.lastRejectStrength = detectorTransientRejectedStrength();

    const bool invalidAudioTrial = _sequenceTest.trialHadAudioOverflow
                                   || _audioSource.stats().overflowCount != _sequenceTest.trialOverflowCountAtStart;
    const bool unexpectedTrial = !invalidAudioTrial && _sequenceTest.currentTrialUnexpected > 0;
    const bool hitTrial = !invalidAudioTrial && _sequenceTest.currentTrialHit;

    const char* result = "miss";
    long dtMs = -1;
    long durMs = -1;
    float strength = 0.0f;

    if (invalidAudioTrial) {
        _sequenceTest.invalidAudio++;
        result = "invalid_audio";
    } else if (unexpectedTrial) {
        _sequenceTest.unexpected++;
        result = "unexpected";
    } else if (hitTrial) {
        _sequenceTest.hits++;
        dtMs = static_cast<long>(diagnostics.acceptedTransientMs - _sequenceTest.currentTrialStartMs);
        durMs = static_cast<long>(diagnostics.acceptedTransientDurationMs);
        strength = diagnostics.acceptedTransientStrength;
        if (dtMs >= kLateOnsetMinMs) {
            result = "late";
            _sequenceTest.lateHits++;
        } else {
            result = "expected";
            _sequenceTest.expectedHits++;
        }
        _sequenceTest.totalHitStrengthScaled += static_cast<unsigned long>(diagnostics.acceptedTransientStrength * 100.0f);
        _sequenceTest.totalHitDurationMs += diagnostics.acceptedTransientDurationMs;
    } else {
        _sequenceTest.misses++;
    }

    _sequenceTest.duplicates += diagnostics.duplicateCount;
    AnalyzerReport finalizedReport = buildSequenceAnalyzerReport(_sequenceTest.currentTrial, result, dtMs, durMs, strength, invalidAudioTrial, diagnostics.duplicateCount, diagnostics);
    if (kAnalyzerEnableRecheckParity) {
        if (finalizedReport.debug.parityCompared) {
            ++_sequenceTest.parityCompared;
            if (finalizedReport.debug.parityMatch) {
                ++_sequenceTest.parityMatched;
            } else {
                ++_sequenceTest.parityAcceptedMismatch;
                if (!finalizedReport.debug.parityTypeMatch) {
                    ++_sequenceTest.parityTypeMismatch;
                }
                if (!finalizedReport.debug.paritySourceMatch) {
                    ++_sequenceTest.paritySourceMismatch;
                }
                if (!finalizedReport.debug.parityReasonMatch) {
                    ++_sequenceTest.parityReasonMismatch;
                }
                if (!finalizedReport.debug.parityTimingClose) {
                    ++_sequenceTest.parityTimingMismatch;
                }
                if (!finalizedReport.debug.parityConfidenceClose) {
                    ++_sequenceTest.parityConfidenceMismatch;
                }
            }
        } else if (finalizedReport.debug.parityReason != nullptr) {
            if (strcmp(finalizedReport.debug.parityReason, "missing_actual_pipeline_result") == 0) {
                ++_sequenceTest.parityMissingActual;
            } else if (strcmp(finalizedReport.debug.parityReason, "missing_recheck") == 0) {
                ++_sequenceTest.parityMissingRecheck;
            }
        }
    }
    const bool briefTrial = analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_TRIAL_BRIEF);
    flushSequenceSampleHistory(now + 1UL);
    printSequenceSampleDump(_sequenceTest.currentTrial);
    if (briefTrial) {
        printSequenceTrialResult(_sequenceTest.currentTrial, result, dtMs, durMs, strength, invalidAudioTrial, diagnostics.duplicateCount, diagnostics);
    } else {
        printSequenceTrialResult(finalizedReport);
    }
    if (analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_EXPLAIN)) {
        printSequenceExplain(finalizedReport);
        if (_sequenceTest.legacyExplainOutput) {
            printSequenceExplainLegacy(_sequenceTest.currentTrial, result, diagnostics);
        }
    }
    if (analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_CUSTOM) &&
        !analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_EXPLAIN)) {
        printSequenceAmpWindow(finalizedReport);
    }

    if (_sequenceTest.deprecatedTrialReports != nullptr) {
        const size_t reportIndex = static_cast<size_t>(_sequenceTest.currentTrial - 1UL);
        if (reportIndex < _sequenceTest.deprecatedTrialReportCapacity) {
            auto& storedReport = _sequenceTest.deprecatedTrialReports[reportIndex];
            storedReport.trialNumber = _sequenceTest.currentTrial;
            storedReport.startMs = _sequenceTest.currentTrialStartMs;
            storedReport.endMs = _sequenceTest.currentTrialEndMs;
            storedReport.dtMs = dtMs;
            storedReport.durMs = durMs;
            storedReport.strength = strength;
            storedReport.duplicates = diagnostics.duplicateCount;
            storedReport.onsetSeen = diagnostics.onsetSeen;
            storedReport.maxEnv = diagnostics.maxSignalLevel;
            storedReport.maxStrengthEst = diagnostics.strongestRejectStrength;
            if (diagnostics.transientAccepted && diagnostics.acceptedTransientStrength > storedReport.maxStrengthEst) {
                storedReport.maxStrengthEst = diagnostics.acceptedTransientStrength;
            }
            if (diagnostics.duplicateCount > 0 && diagnostics.duplicateTransientStrength > storedReport.maxStrengthEst) {
                storedReport.maxStrengthEst = diagnostics.duplicateTransientStrength;
            }
            if (diagnostics.bestCandidateStrength > storedReport.maxStrengthEst) {
                storedReport.maxStrengthEst = diagnostics.bestCandidateStrength;
            }
            storedReport.transientRejectTooShortCount = diagnostics.transientRejectTooShortCount;
            storedReport.transientRejectTooLongCount = diagnostics.transientRejectTooLongCount;
            storedReport.transientRejectWeakCount = diagnostics.transientRejectWeakCount;
            storedReport.onsetRejectPeakActiveCount = diagnostics.onsetRejectPeakActive;
            storedReport.onsetRejectCooldownCount = diagnostics.onsetRejectCooldown;
            storedReport.onsetRejectOtherCount = diagnostics.onsetRejectOther;
            storedReport.bestCandidateDtFromTriggerMs = diagnostics.bestCandidateDtFromTriggerMs >= 0 ? static_cast<unsigned long>(diagnostics.bestCandidateDtFromTriggerMs) : 0UL;
            storedReport.bestCandidateDurationMs = diagnostics.bestCandidateDurationMs;
            storedReport.bestCandidateStrength = diagnostics.bestCandidateStrength;
            storedReport.bestCandidateValid = diagnostics.bestCandidateValid;
            storedReport.bestCandidateOrigin = diagnostics.bestCandidateOrigin;
            storedReport.candidateCount = diagnostics.candidateCount;
            storedReport.candidateOverflowCount = diagnostics.candidateOverflowCount;
            storedReport.candidatePreWindowCount = diagnostics.candidatePreWindowCount;
            storedReport.candidateInWindowCount = diagnostics.candidateInWindowCount;
            storedReport.candidatePostWindowCount = diagnostics.candidatePostWindowCount;
            storedReport.freqEarly = {};
            storedReport.freqFull = {};
            storedReport.duplicateFreqEarly = {};
            storedReport.duplicateFreqFull = {};
            if (diagnostics.transientAccepted) {
                storedReport.freqEarly = diagnostics.acceptedFrequencyEvidence;
                storedReport.freqFull = diagnostics.acceptedFrequencyEvidenceFull;
            } else if (diagnostics.duplicateCount > 0) {
                storedReport.freqEarly = diagnostics.duplicateFrequencyEvidence;
                storedReport.freqFull = diagnostics.duplicateFrequencyEvidenceFull;
            }
            if (diagnostics.duplicateCount > 0) {
                storedReport.duplicateFreqEarly = diagnostics.duplicateFrequencyEvidence;
                storedReport.duplicateFreqFull = diagnostics.duplicateFrequencyEvidenceFull;
                storedReport.duplicateDurMs = static_cast<long>(diagnostics.duplicateTransientDurationMs);
                storedReport.duplicateStrength = diagnostics.duplicateTransientStrength;
            }
            strncpy(storedReport.result, result, sizeof(storedReport.result));
            storedReport.result[sizeof(storedReport.result) - 1] = '\0';
            storedReport.deprecatedAnalyzerReport = finalizedReport;
            storedReport.deprecatedAnalyzerReportCaptured = true;
            const size_t storedCount = reportIndex + 1UL;
            if (storedCount > _sequenceTest.deprecatedTrialReportCount) {
                _sequenceTest.deprecatedTrialReportCount = storedCount;
            }
        }
    }
    if (_sequenceTest.deprecatedAnalyzerReports != nullptr) {
        const size_t reportIndex = static_cast<size_t>(_sequenceTest.currentTrial - 1UL);
        if (reportIndex < _sequenceTest.deprecatedAnalyzerReportCapacity) {
            _sequenceTest.deprecatedAnalyzerReports[reportIndex] = finalizedReport;
            const size_t storedCount = reportIndex + 1UL;
            if (storedCount > _sequenceTest.deprecatedAnalyzerReportCount) {
                _sequenceTest.deprecatedAnalyzerReportCount = storedCount;
            }
        }
    }

    Serial.flush();
    _sequenceTest.currentTrialFinalized = true;

    if (_sequenceTest.currentTrial >= _sequenceTest.totalTrials) {
        printSequenceFinalOutput();
        stopSequenceTest();
    }
}

// Legacy debug fallback only; not used in the normal Analyzer reporting path.
bool AnalyzerApp::evaluateModernSignalCandidate(const detection::SignalCandidate& signal, detection::PatternResult& outResult, detection::InspectedSignal* outInspected) const {
    return evaluateModernSignalCandidateImpl(signal, _frequencyEvidenceTuning, _sequenceFeatureHistory, outResult, outInspected,
                                               analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_EXPLAIN));
}

static AnalyzerResult analyzerResultFromSequenceOutcome(const char* result) {
    if (result == nullptr) {
        return AnalyzerResult::Unknown;
    }
    if (strcmp(result, "expected") == 0) {
        return AnalyzerResult::Expected;
    }
    if (strcmp(result, "late") == 0) {
        return AnalyzerResult::Late;
    }
    if (strcmp(result, "miss") == 0) {
        return AnalyzerResult::Miss;
    }
    if (strcmp(result, "duplicate") == 0) {
        return AnalyzerResult::Duplicate;
    }
    if (strcmp(result, "unexpected") == 0) {
        return AnalyzerResult::Unexpected;
    }
    if (strcmp(result, "rejected") == 0) {
        return AnalyzerResult::Rejected;
    }
    if (strcmp(result, "ambiguous") == 0) {
        return AnalyzerResult::Ambiguous;
    }
    if (strcmp(result, "too_dense") == 0) {
        return AnalyzerResult::TooDense;
    }
    if (strcmp(result, "invalid_audio") == 0) {
        return AnalyzerResult::InvalidAudio;
    }
    return AnalyzerResult::Unknown;
}

static AnalyzerReason analyzerReasonFromSequenceOutcome(const char* result,
                                                       long dtMs,
                                                       unsigned long rawCandidateCount,
                                                       AmpTransientDetector::TransientRejectReason strongestRejectReason,
                                                       bool audioOverflow) {
    if (audioOverflow) {
        return AnalyzerReason::InvalidAudio;
    }
    if (result == nullptr) {
        return AnalyzerReason::Unknown;
    }
    if (strcmp(result, "expected") == 0) {
        return AnalyzerReason::ValidPatternInExpectedWindow;
    }
    if (strcmp(result, "late") == 0) {
        return AnalyzerReason::ValidPatternAfterWindow;
    }
    if (strcmp(result, "unexpected") == 0) {
        return AnalyzerReason::UnexpectedValidPatternWithoutTrigger;
    }
    if (strcmp(result, "duplicate") == 0) {
        return AnalyzerReason::DuplicatePatternAfterPrimary;
    }
    if (strcmp(result, "invalid_audio") == 0) {
        return AnalyzerReason::InvalidAudio;
    }
    if (strcmp(result, "miss") == 0) {
        if (rawCandidateCount == 0) {
            return AnalyzerReason::NoSignalCandidate;
        }
        switch (strongestRejectReason) {
            case AmpTransientDetector::TransientRejectReason::DurationTooShort:
            case AmpTransientDetector::TransientRejectReason::DurationTooLong:
            case AmpTransientDetector::TransientRejectReason::StrengthTooLow:
                return AnalyzerReason::SignalSeenButRejected;
            case AmpTransientDetector::TransientRejectReason::PeakStillActive:
                return AnalyzerReason::InspectionFailed;
            case AmpTransientDetector::TransientRejectReason::None:
            default:
                break;
        }
        return dtMs >= 0 ? AnalyzerReason::PatternCandidateRejected : AnalyzerReason::NoSignalCandidate;
    }
    if (strcmp(result, "rejected") == 0) {
        return AnalyzerReason::PatternCandidateRejected;
    }
    if (strcmp(result, "ambiguous") == 0) {
        return AnalyzerReason::MultipleCompetingPatterns;
    }
    if (strcmp(result, "too_dense") == 0) {
        return AnalyzerReason::FieldTooDense;
    }
    return AnalyzerReason::Unknown;
}

static size_t analyzerReasonIndex(AnalyzerReason value) {
    return static_cast<size_t>(value);
}

const char* AnalyzerApp::activeAnalyzerProfileName() const {
    return detection::detectionProfileName(_sequenceTest.profileKind);
}

const char* analyzerProfileDetailNamespace(detection::DetectionProfileKind profileKind, bool liveFrequencyOnly) {
    if (liveFrequencyOnly) {
        return "freq_amp_live";
    }
    switch (profileKind) {
        case detection::DetectionProfileKind::Chirp:
            return "chirp";
        case detection::DetectionProfileKind::FreqAmp:
        default:
            return "freq_amp";
    }
}

const char* analyzerProfileDetailSummary(detection::DetectionProfileKind profileKind, bool liveFrequencyOnly) {
    if (liveFrequencyOnly) {
        return "generic live-frequency profile view";
    }
    switch (profileKind) {
        case detection::DetectionProfileKind::Chirp:
            return "chirp profile view";
        case detection::DetectionProfileKind::FreqAmp:
        default:
            return "generic freq-amp profile view";
    }
}

bool AnalyzerApp::sequenceLegacyReportEnabled() const {
    return analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_FREQ_CLASS) ||
           (_sequenceTest.legacyExplainOutput && analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_EXPLAIN));
}

AnalyzerReport AnalyzerApp::buildSequenceAnalyzerReport(unsigned long trialNumber,
                                                        const char* result,
                                                        long dtMs,
                                                        long durMs,
                                                     float strength,
                                                     bool audioOverflow,
                                                     unsigned long duplicateCount,
                                                     const SequenceTest::TrialDiagnostics& diagnostics) const {
    AnalyzerReport report = makeEmptyAnalyzerReport();

    report.context.profile = activeAnalyzerProfileName();
    report.context.mode = _sequenceTest.externalEmitter ? "OBS" : (_sequenceTest.liveFrequencyOnly ? "LIVEFREQ" : "SEQ");
    report.context.trial = trialNumber;
    report.context.trigger = _sequenceTest.externalEmitter ? "observe" : "chirp";
    report.context.target = _sequenceTest.liveFrequencyOnly ? "livefreq" : "tone";
    report.context.timestampMs = _sequenceTest.currentTrialEndMs;
    report.context.build = "pass-c";

    report.expected.triggerMs = _sequenceTest.currentTrialStartMs;
    report.expected.windowStartMs = _sequenceTest.currentTrialStartMs;
    report.expected.windowEndMs = _sequenceTest.currentTrialEndMs;
    report.expected.patternType = _sequenceTest.liveFrequencyOnly ? "live_frequency" : "sequence_trial";
    report.expected.expectedSource = _sequenceTest.externalEmitter ? "external" : "local";

    const detection::DetectionPipelineResult* pipelineResult = _detection != nullptr && _detection->hasLatestPipelineResult()
        ? &_detection->latestPipelineResult()
        : nullptr;
    const bool actualPipelineAvailable = pipelineResult != nullptr && pipelineResult->hasPattern;
    const detection::PatternResult* runtimePatternResult = actualPipelineAvailable ? &pipelineResult->pattern : nullptr;
    const detection::InspectedSignal* runtimeInspectedSignal = actualPipelineAvailable && pipelineResult->hasInspectedSignal
        ? &pipelineResult->inspectedSignal
        : nullptr;
    const detection::FieldState* runtimeFieldState = actualPipelineAvailable && pipelineResult->hasField
        ? &pipelineResult->field
        : nullptr;
    const auto artifactReason = [&]() -> const char* {
        if (actualPipelineAvailable) {
            return "captured_from_runtime_pipeline";
        }
        return "missing_pipeline_result";
    }();

    report.classification.result = analyzerResultFromSequenceOutcome(result);
    report.classification.reason = actualPipelineAvailable
        ? analyzerReasonFromSequenceOutcome(result, dtMs, diagnostics.rawCandidateCount, diagnostics.strongestRejectReason, audioOverflow)
        : AnalyzerReason::MissingPipelineResult;
    report.classification.dtMs = dtMs;
    report.classification.confidence = actualPipelineAvailable && runtimePatternResult != nullptr ? runtimePatternResult->confidence : 0.0f;
    {
        // Analyzer consumes the PatternResult produced by DetectionRuntime.
        // Analyzer does not re-run signal inspection or pattern interpretation.
        AnalyzerPatternObservation pattern = {};
        pattern.type = actualPipelineAvailable && runtimePatternResult != nullptr ? detection::patternTypeName(runtimePatternResult->type) : "unknown";
        pattern.accepted = actualPipelineAvailable && runtimePatternResult != nullptr
            ? runtimePatternResult->valid
            : false;
        pattern.confidence = actualPipelineAvailable && runtimePatternResult != nullptr ? runtimePatternResult->confidence : 0.0f;
        pattern.dtMs = report.classification.dtMs;
        pattern.ampSupport = actualPipelineAvailable && runtimePatternResult != nullptr ? ampSupportName(runtimePatternResult->ampSupport) : "unknown";
        pattern.sourceClass = actualPipelineAvailable && runtimePatternResult != nullptr ? detection::patternSourceName(runtimePatternResult->source) : "unknown";
        pattern.reason = actualPipelineAvailable && runtimePatternResult != nullptr ? detection::patternReasonName(runtimePatternResult->reasonCode) : analyzerReasonName(report.classification.reason);
        pattern.involvedSignals = actualPipelineAvailable && runtimePatternResult != nullptr ? runtimePatternResult->signalCount : 0U;
        report.primaryPattern = pattern;
    }

    report.signals.total = diagnostics.rawCandidateCount;
    report.signals.accepted = actualPipelineAvailable && runtimePatternResult != nullptr && runtimePatternResult->valid ? 1U : 0U;
    report.signals.rejected = diagnostics.rawCandidateCount > report.signals.accepted ? diagnostics.rawCandidateCount - report.signals.accepted : 0U;
    report.signals.primarySource = actualPipelineAvailable && runtimeInspectedSignal != nullptr && runtimeInspectedSignal->signal.present
        ? signalSourceName(runtimeInspectedSignal->signal.source)
        : "unknown";
    report.signals.primaryDtMs = dtMs;
    report.signals.primaryDurationMs = durMs >= 0 ? static_cast<unsigned long>(durMs) : 0UL;
    report.signals.primaryStrength = strength;
    report.signals.primaryConfidence = actualPipelineAvailable && runtimePatternResult != nullptr ? runtimePatternResult->confidence : 0.0f;
    report.signals.mainRejectReason = actualPipelineAvailable && runtimeInspectedSignal != nullptr
        ? (runtimeInspectedSignal->rejected ? signalRejectReasonName(runtimeInspectedSignal->rejectReason) : "none")
        : analyzerReasonName(report.classification.reason);
    report.signals.duplicateRisk = duplicateCount > 0;

    report.inspection.inspected = diagnostics.rawCandidateCount;
    report.inspection.accepted = report.signals.accepted;
    report.inspection.rejected = diagnostics.rawCandidateCount > report.inspection.accepted ? diagnostics.rawCandidateCount - report.inspection.accepted : 0U;
    if (actualPipelineAvailable && runtimeInspectedSignal != nullptr && runtimeInspectedSignal->signal.present) {
        report.inspection.primaryEvidence = signalSourceName(runtimeInspectedSignal->signal.source);
        report.inspection.ampSupport = ampSupportName(runtimeInspectedSignal->ampSupport);
        report.inspection.supportClass = ampSupportName(runtimeInspectedSignal->ampSupport);
        report.inspection.mainRejectReason = runtimeInspectedSignal->rejected ? signalRejectReasonName(runtimeInspectedSignal->rejectReason) : "none";
    } else {
        report.inspection.primaryEvidence = "none";
        report.inspection.ampSupport = "unknown";
        report.inspection.supportClass = "unsupported";
        report.inspection.mainRejectReason = analyzerReasonName(report.classification.reason);
    }

    if (actualPipelineAvailable && runtimeFieldState != nullptr) {
        report.field.state = runtimeFieldState->dense ? "dense" : (runtimeFieldState->active ? (runtimeFieldState->quiet ? "quiet" : "active") : "unknown");
        report.field.activity = runtimeFieldState->activity;
        report.field.density = runtimeFieldState->density;
        report.field.recentValidPatterns = runtimeFieldState->recentPatternCount;
        report.field.recentRejects = runtimeFieldState->recentSignalCount > runtimeFieldState->recentPatternCount
            ? runtimeFieldState->recentSignalCount - runtimeFieldState->recentPatternCount
            : 0U;
    } else {
        report.field.state = "unknown";
        report.field.activity = 0.0f;
        report.field.density = 0.0f;
        report.field.recentValidPatterns = 0U;
        report.field.recentRejects = diagnostics.rawCandidateCount;
    }

    report.profileDetail.namespaceName = analyzerProfileDetailNamespace(_sequenceTest.profileKind, _sequenceTest.liveFrequencyOnly);
    report.profileDetail.summary = analyzerProfileDetailSummary(_sequenceTest.profileKind, _sequenceTest.liveFrequencyOnly);
    report.profileDetail.freqScore = actualPipelineAvailable && runtimePatternResult != nullptr ? runtimePatternResult->freq.score : 0.0f;
    report.profileDetail.freqContrast = actualPipelineAvailable && runtimePatternResult != nullptr ? runtimePatternResult->freq.spectralContrast : 0.0f;
    report.profileDetail.ampLevel = report.signals.primaryStrength;
    report.profileDetail.ampBase = diagnostics.acceptedAmbientBaseline;
    report.profileDetail.ampLift = report.profileDetail.ampLevel - report.profileDetail.ampBase;
    report.profileDetail.ampSupport = report.primaryPattern.ampSupport;
    const detection::AmpWindowEvidence ampWindowEvidence = actualPipelineAvailable && runtimeInspectedSignal != nullptr
        ? runtimeInspectedSignal->ampWindow
        : detection::AmpWindowEvidence{};
    report.profileDetail.ampWindow.available = ampWindowEvidence.available;
    report.profileDetail.ampWindow.observedOnly = ampWindowEvidence.observedOnly;
    report.profileDetail.ampWindow.note = ampWindowEvidence.available
        ? "inspector_seen"
        : (actualPipelineAvailable ? "inspector_no_amp_window" : "missing_pipeline_result");
    report.profileDetail.ampWindow.windowStartMs = ampWindowEvidence.windowStartMs;
    report.profileDetail.ampWindow.windowEndMs = ampWindowEvidence.windowEndMs;
    report.profileDetail.ampWindow.peak = ampWindowEvidence.peak;
    report.profileDetail.ampWindow.baseline = ampWindowEvidence.baseline;
    report.profileDetail.ampWindow.lift = ampWindowEvidence.lift;
    report.profileDetail.ampWindow.supportClass = ampSupportName(ampWindowEvidence.supportClass);

    report.debug.signals = diagnostics.rawCandidateCount;
    report.debug.inspected = diagnostics.rawCandidateCount;
    report.debug.patterns = diagnostics.transientAccepted ? 1U : 0U;
    report.debug.rejects = report.signals.rejected;
    report.debug.duplicates = duplicateCount;
    report.debug.unexpected = strcmp(result, "unexpected") == 0 ? 1U : 0U;
    report.debug.artifactCaptured = actualPipelineAvailable;
    report.debug.artifactFallback = !actualPipelineAvailable;
    report.debug.artifactState = actualPipelineAvailable ? "CAPTURED" : "MISSING_PIPELINE";
    report.debug.artifactReason = artifactReason;
    report.debug.pipelineSource = actualPipelineAvailable ? "actual_pipeline" : "missing_runtime_pipeline";
    report.debug.pipelineFallback = !actualPipelineAvailable;
    report.debug.mainRejectReason = actualPipelineAvailable && runtimeInspectedSignal != nullptr
        ? (runtimeInspectedSignal->rejected ? signalRejectReasonName(runtimeInspectedSignal->rejectReason) : "none")
        : analyzerReasonName(report.classification.reason);

    if (kAnalyzerEnableRecheckParity && actualPipelineAvailable && diagnostics.deprecatedAcceptedPatternCaptured) {
        const long actualDtMs = diagnostics.runtimePatternResult.candidate.startMs >= _sequenceTest.currentTrialStartMs
            ? static_cast<long>(diagnostics.runtimePatternResult.candidate.startMs - _sequenceTest.currentTrialStartMs)
            : -1;
        const long recheckedDtMs = diagnostics.deprecatedAcceptedPatternResult.candidate.startMs >= _sequenceTest.currentTrialStartMs
            ? static_cast<long>(diagnostics.deprecatedAcceptedPatternResult.candidate.startMs - _sequenceTest.currentTrialStartMs)
            : -1;
        const PatternResultParity parity = comparePatternResultsForAnalyzer(
            diagnostics.runtimePatternResult,
            diagnostics.deprecatedAcceptedPatternResult,
            actualDtMs,
            recheckedDtMs);
        report.debug.parityCompared = parity.compared;
        report.debug.parityMatch = parity.match;
        report.debug.parityAcceptedMatch = parity.acceptedMatch;
        report.debug.parityTypeMatch = parity.typeMatch;
        report.debug.paritySourceMatch = parity.sourceMatch;
        report.debug.parityReasonMatch = parity.reasonMatch;
        report.debug.parityTimingClose = parity.timingClose;
        report.debug.parityConfidenceClose = parity.confidenceClose;
        report.debug.parityConfidenceDelta = parity.confidenceDelta;
        report.debug.parityTimingDeltaMs = parity.timingDeltaMs;
        report.debug.paritySummary = parity.summary;
        report.debug.parityReason = parity.reason;
    } else if (!actualPipelineAvailable) {
        report.debug.parityReason = "missing_actual_pipeline_result";
    }

    return report;
}

void AnalyzerApp::printSequenceTrialResult(const AnalyzerReport& report) const {
    if (_valMode) {
        return;
    }
    if (_sequenceTest.logFlags == AnalyzerApp::ANALYZER_LOG_CUSTOM) {
        return;
    }
    if (!analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_TRIAL)) {
        return;
    }

    Serial.println();
    Serial.print("SEQ_TRIAL trial=");
    Serial.print(report.context.trial);
    Serial.print(" result=");
    Serial.print(analyzerResultName(report.classification.result));
    Serial.print(" dt=");
    if (report.classification.dtMs >= 0) {
        Serial.print(report.classification.dtMs);
        Serial.print("ms");
    } else {
        Serial.print("-1ms");
    }
    Serial.print(" pattern=");
    Serial.print(report.primaryPattern.type != nullptr ? report.primaryPattern.type : "unknown");
    Serial.print(" profile=");
    Serial.print(report.context.profile != nullptr ? report.context.profile : "unknown");
    Serial.print(" source=");
    Serial.print(report.primaryPattern.sourceClass != nullptr ? report.primaryPattern.sourceClass : "unknown");
    Serial.print(" confidence=");
    Serial.print(report.primaryPattern.confidence, 2);
    Serial.print(" amp_support=");
    Serial.print(report.primaryPattern.ampSupport != nullptr ? report.primaryPattern.ampSupport : "unknown");
    Serial.print(" field=");
    Serial.print(report.field.state != nullptr ? report.field.state : "unknown");
    Serial.print(" reason=");
    Serial.print(analyzerReasonName(report.classification.reason));
    Serial.print(" dup=");
    Serial.print(report.debug.duplicates);
    Serial.print(" candidates=");
    Serial.print(report.signals.total);
    Serial.println();
}

void AnalyzerApp::printSequenceExplain(const AnalyzerReport& report) const {
    if (_valMode) {
        return;
    }
    if (_sequenceTest.logFlags == AnalyzerApp::ANALYZER_LOG_CUSTOM) {
        return;
    }
    if (!analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_EXPLAIN)) {
        return;
    }

    const auto printMs = [](long value) {
        if (value >= 0) {
            Serial.print(value);
            Serial.print("ms");
        } else {
            Serial.print("-1ms");
        }
    };
    const auto printUnsignedMs = [](unsigned long value) {
        Serial.print(value);
        Serial.print("ms");
    };

    Serial.println();
    Serial.print("SEQ_EXPLAIN trial=");
    Serial.print(report.context.trial);
    Serial.print(" result=");
    Serial.print(analyzerResultName(report.classification.result));
    Serial.print(" pattern=");
    Serial.print(report.primaryPattern.type != nullptr ? report.primaryPattern.type : "unknown");
    Serial.print(" dt=");
    if (report.classification.dtMs >= 0) {
        Serial.print(report.classification.dtMs);
        Serial.print("ms");
    } else {
        Serial.print("-1ms");
    }
    Serial.print(" profile=");
    Serial.print(report.context.profile != nullptr ? report.context.profile : "unknown");
    Serial.print(" source=");
    Serial.print(report.primaryPattern.sourceClass != nullptr ? report.primaryPattern.sourceClass : "unknown");
    Serial.print(" confidence=");
    Serial.print(report.primaryPattern.confidence, 2);
    Serial.print(" amp_support=");
    Serial.print(report.primaryPattern.ampSupport != nullptr ? report.primaryPattern.ampSupport : "unknown");
    Serial.print(" reason=");
    Serial.print(analyzerReasonName(report.classification.reason));
    Serial.println();

    Serial.print("SEQ_AMP_WINDOW trial=");
    Serial.print(report.context.trial);
    Serial.print(" dt=");
    printMs(report.primaryPattern.dtMs);
    Serial.print(" win=");
    Serial.print(static_cast<long>(report.profileDetail.ampWindow.windowStartMs));
    Serial.print("..");
    Serial.print(static_cast<long>(report.profileDetail.ampWindow.windowEndMs));
    Serial.print("ms");
    Serial.print(" available=");
    Serial.print(report.profileDetail.ampWindow.available ? 1 : 0);
    Serial.print(" amp_support=");
    Serial.print(report.profileDetail.ampWindow.supportClass != nullptr ? report.profileDetail.ampWindow.supportClass : "unknown");
    Serial.print(" peak=");
    Serial.print(report.profileDetail.ampWindow.peak, 1);
    Serial.print(" floor=");
    Serial.print(report.profileDetail.ampWindow.baseline, 1);
    Serial.print(" lift=");
    Serial.print(report.profileDetail.ampWindow.lift, 1);
    Serial.print(" note=");
    Serial.println(report.profileDetail.ampWindow.note != nullptr ? report.profileDetail.ampWindow.note : "none");

    Serial.print("SEQ_EXPLAIN_FIELD state=");
    Serial.print(report.field.state != nullptr ? report.field.state : "unknown");
    Serial.print(" activity=");
    Serial.print(report.field.activity, 2);
    Serial.print(" density=");
    Serial.print(report.field.density, 2);
    Serial.print(" recent_valid=");
    Serial.print(report.field.recentValidPatterns);
    Serial.print(" recent_rejects=");
    Serial.print(report.field.recentRejects);
    Serial.println();

    Serial.print("SEQ_EXPLAIN_CLASSIFICATION result=");
    Serial.print(analyzerResultName(report.classification.result));
    Serial.print(" reason=");
    Serial.print(analyzerReasonName(report.classification.reason));
    Serial.print(" dt=");
    printMs(report.classification.dtMs);
    Serial.print(" confidence=");
    Serial.print(report.classification.confidence, 2);
    Serial.println();

    if (analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_CUSTOM)) {
        Serial.print("SEQ_EXPLAIN_EXPECTED pattern=");
        Serial.print(report.expected.patternType != nullptr ? report.expected.patternType : "none");
        Serial.print(" window=");
        printUnsignedMs(report.expected.windowStartMs);
        Serial.print("-");
        printUnsignedMs(report.expected.windowEndMs);
        Serial.print(" target=");
        Serial.print(report.context.target != nullptr ? report.context.target : "none");
        Serial.print(" source=");
        Serial.print(report.expected.expectedSource != nullptr ? report.expected.expectedSource : "none");
        Serial.print(" trigger_ms=");
        printUnsignedMs(report.expected.triggerMs);
        Serial.println();

        Serial.print("SEQ_EXPLAIN_SIGNAL total=");
        Serial.print(report.signals.total);
        Serial.print(" accepted=");
        Serial.print(report.signals.accepted);
        Serial.print(" rejected=");
        Serial.print(report.signals.rejected);
        Serial.print(" primary_source=");
        Serial.print(report.signals.primarySource != nullptr ? report.signals.primarySource : "none");
        Serial.print(" primary_dt=");
        printMs(report.signals.primaryDtMs);
        Serial.print(" primary_dur=");
        printUnsignedMs(report.signals.primaryDurationMs);
        Serial.print(" primary_strength=");
        Serial.print(report.signals.primaryStrength, 1);
        Serial.print(" confidence=");
        Serial.print(report.signals.primaryConfidence, 2);
        Serial.print(" main_reject=");
        Serial.print(report.signals.mainRejectReason != nullptr ? report.signals.mainRejectReason : "none");
        Serial.print(" duplicate_risk=");
        Serial.print(report.signals.duplicateRisk ? 1 : 0);
        Serial.println();

        Serial.print("SEQ_EXPLAIN_INSPECTION inspected=");
        Serial.print(report.inspection.inspected);
        Serial.print(" accepted=");
        Serial.print(report.inspection.accepted);
        Serial.print(" rejected=");
        Serial.print(report.inspection.rejected);
        Serial.print(" evidence=");
        Serial.print(report.inspection.primaryEvidence != nullptr ? report.inspection.primaryEvidence : "none");
        Serial.print(" amp_support=");
        Serial.print(report.inspection.ampSupport != nullptr ? report.inspection.ampSupport : "unknown");
        Serial.print(" main_reject=");
        Serial.print(report.inspection.mainRejectReason != nullptr ? report.inspection.mainRejectReason : "none");
        Serial.println();

        Serial.print("SEQ_EXPLAIN_PATTERN type=");
        Serial.print(report.primaryPattern.type != nullptr ? report.primaryPattern.type : "none");
        Serial.print(" accepted=");
        Serial.print(report.primaryPattern.accepted ? 1 : 0);
        Serial.print(" dt=");
        printMs(report.primaryPattern.dtMs);
        Serial.print(" confidence=");
        Serial.print(report.primaryPattern.confidence, 2);
        Serial.print(" amp_support=");
        Serial.print(report.primaryPattern.ampSupport != nullptr ? report.primaryPattern.ampSupport : "unknown");
        Serial.print(" source=");
        Serial.print(report.primaryPattern.sourceClass != nullptr ? report.primaryPattern.sourceClass : "unknown");
        Serial.print(" reason=");
        Serial.print(report.primaryPattern.reason != nullptr ? report.primaryPattern.reason : "none");
        Serial.print(" signals=");
        Serial.print(report.primaryPattern.involvedSignals);
        Serial.println();

        Serial.print("SEQ_EXPLAIN_PROFILE_DETAIL ns=");
        Serial.print(report.profileDetail.namespaceName != nullptr ? report.profileDetail.namespaceName : "none");
        Serial.print(" summary=");
        Serial.print(report.profileDetail.summary != nullptr ? report.profileDetail.summary : "");
        Serial.print(" freq_score=");
        Serial.print(report.profileDetail.freqScore, 1);
        Serial.print(" freq_contrast=");
        Serial.print(report.profileDetail.freqContrast, 2);
        Serial.print(" amp_level=");
        Serial.print(report.profileDetail.ampLevel, 1);
        Serial.print(" amp_base=");
        Serial.print(report.profileDetail.ampBase, 1);
        Serial.print(" amp_lift=");
        Serial.print(report.profileDetail.ampLift, 1);
        Serial.print(" amp_support=");
        Serial.print(report.profileDetail.ampSupport != nullptr ? report.profileDetail.ampSupport : "unknown");
        Serial.println();

        Serial.print("SEQ_EXPLAIN_PIPELINE_SOURCE source=");
        Serial.print(report.debug.pipelineSource != nullptr ? report.debug.pipelineSource : "unknown");
        Serial.print(" fallback=");
        Serial.print(report.debug.pipelineFallback ? 1 : 0);
        Serial.println();

        if (kAnalyzerEnableRecheckParity) {
            Serial.print("SEQ_EXPLAIN_PARITY compared=");
            Serial.print(report.debug.parityCompared ? 1 : 0);
            if (report.debug.parityCompared) {
                Serial.print(" match=");
                Serial.print(report.debug.parityMatch ? 1 : 0);
                Serial.print(" accepted=");
                Serial.print(report.debug.parityAcceptedMatch ? 1 : 0);
                Serial.print(" type=");
                Serial.print(report.debug.parityTypeMatch ? 1 : 0);
                Serial.print(" source=");
                Serial.print(report.debug.paritySourceMatch ? 1 : 0);
                Serial.print(" reason=");
                Serial.print(report.debug.parityReasonMatch ? 1 : 0);
                Serial.print(" confidence_delta=");
                Serial.print(report.debug.parityConfidenceDelta, 2);
                Serial.print(" timing_delta=");
                Serial.print(report.debug.parityTimingDeltaMs);
                Serial.print("ms");
                Serial.print(" summary=");
                Serial.print(report.debug.paritySummary != nullptr ? report.debug.paritySummary : "none");
            } else {
                Serial.print(" reason=");
                Serial.print(report.debug.parityReason != nullptr ? report.debug.parityReason : "none");
            }
            Serial.println();
        }

        Serial.print("SEQ_EXPLAIN_DUPLICATES count=");
        Serial.print(report.debug.duplicates);
        Serial.print(" duplicate_risk=");
        Serial.print(report.signals.duplicateRisk ? 1 : 0);
        Serial.println();
    }

    printSequenceAmpWindow(report);

    Serial.print("SEQ_EXPLAIN_DEBUG raw_candidates=");
    Serial.print(report.debug.signals);
    Serial.print(" inspected_candidates=");
    Serial.print(report.debug.inspected);
    Serial.print(" accepted_patterns=");
    Serial.print(report.debug.patterns);
    Serial.print(" rejects=");
    Serial.print(report.debug.rejects);
    Serial.print(" duplicate_hits=");
    Serial.print(report.debug.duplicates);
    Serial.print(" unexpected_hits=");
    Serial.print(report.debug.unexpected);
    Serial.print(" main_reject=");
    Serial.print(report.debug.mainRejectReason != nullptr ? report.debug.mainRejectReason : "none");
    Serial.println();
}

// Legacy explain/debug dump retained for Pass A quarantine.
void AnalyzerApp::printSequenceExplainLegacy(unsigned long trialNumber, const char* result, const SequenceTest::TrialDiagnostics& diagnostics) const {
    if (_valMode) {
        return;
    }

    const long acceptedDtMs = diagnostics.transientAccepted
        ? static_cast<long>(diagnostics.acceptedTransientMs) - static_cast<long>(_sequenceTest.currentTrialStartMs)
        : -1;
    const float ambientBaselineAvg = diagnostics.ambientBaselineSamples > 0
        ? diagnostics.ambientBaselineSum / static_cast<float>(diagnostics.ambientBaselineSamples)
        : 0.0f;
    const char* strongestRejectReasonName = "none";
    switch (diagnostics.strongestRejectReason) {
        case AmpTransientDetector::TransientRejectReason::None:
            strongestRejectReasonName = "none";
            break;
        case AmpTransientDetector::TransientRejectReason::DurationTooShort:
            strongestRejectReasonName = "too_short";
            break;
        case AmpTransientDetector::TransientRejectReason::DurationTooLong:
            strongestRejectReasonName = "too_long";
            break;
        case AmpTransientDetector::TransientRejectReason::StrengthTooLow:
            strongestRejectReasonName = "weak";
            break;
        case AmpTransientDetector::TransientRejectReason::PeakStillActive:
            strongestRejectReasonName = "peak_active";
            break;
    }

    const bool isMiss = strcmp(result, "miss") == 0;
    const bool isLate = strcmp(result, "late") == 0;
    const bool isUnexpected = strcmp(result, "unexpected") == 0;
    const bool hasDuplicates = diagnostics.duplicateCount > 0;
    const bool expectedDtSlow = strcmp(result, "expected") == 0 && acceptedDtMs >= kLateOnsetMinMs;
    const bool expectedDurLong = strcmp(result, "expected") == 0 && diagnostics.acceptedTransientDurationMs >= kSmearedDurationMinMs;
    const unsigned long totalRejects = diagnostics.transientRejectTooShortCount + diagnostics.transientRejectTooLongCount + diagnostics.transientRejectWeakCount;

    if (!(isMiss || isLate || isUnexpected || hasDuplicates || expectedDtSlow || expectedDurLong)) {
        return;
    }

    const char* classification = sequenceTrialClassificationName(result, acceptedDtMs, diagnostics.acceptedTransientDurationMs, diagnostics);

    const char* candidateClass = h3SequenceCandidateClassFromResult(result);
    const auto& freq = diagnostics.acceptedFrequencyEvidence;
    const bool validPattern = strcmp(result, "miss") != 0 && strcmp(result, "invalid_audio") != 0;
    const auto freqEval = FrequencyEvidenceEvaluation::evaluate(freq, _frequencyEvidenceTuning);
    const auto modernFrequencySignal = makeModernFrequencySignalCandidate(_sequenceTest.liveFrequency);
    detection::PatternResult modernFrequencyResult = {};
    detection::InspectedSignal modernFrequencyInspected = {};
    const bool modernFrequencyEvaluated = evaluateModernSignalCandidate(modernFrequencySignal, modernFrequencyResult, &modernFrequencyInspected);
    const unsigned long freqAgeMs = freq.observedAtMs > 0 && diagnostics.acceptedFrequencyProcessedAtMs >= freq.observedAtMs
        ? diagnostics.acceptedFrequencyProcessedAtMs - freq.observedAtMs
        : 0;

    Serial.print("SEQ_EXPLAIN_LEGACY_FREQ_CLASS trial=");
    Serial.print(trialNumber);
    Serial.print(" artifact_state=LEGACY_DIAGNOSTICS");
    Serial.print(" result=");
    Serial.print(result);
    Serial.print(" class=");
    Serial.print(classification);
    Serial.print(" candidate_class=");
    Serial.print(candidateClass);
    Serial.print(" pattern_valid=");
    Serial.print(validPattern ? 1 : 0);
    Serial.print(" pattern_type=");
    Serial.print(validPattern
        ? (modernFrequencyEvaluated && modernFrequencyResult.valid
               ? detection::patternTypeName(modernFrequencyResult.type)
               : "transient_only")
        : "invalid");
    Serial.print(" pattern_reason=");
    Serial.print(validPattern
        ? (modernFrequencyEvaluated && modernFrequencyResult.valid
               ? detection::patternReasonName(modernFrequencyResult.reasonCode)
               : "detector_rejected")
        : "detector_rejected");
    Serial.print(" candidate_valid=");
    Serial.print(validPattern && modernFrequencyEvaluated && modernFrequencyResult.candidateValid ? 1 : 0);
    Serial.print(" tonal_valid=");
    Serial.print(modernFrequencyEvaluated && modernFrequencyResult.valid && modernFrequencyResult.tonalValid ? 1 : 0);
    Serial.print(" behavior_eligible=");
    Serial.print(modernFrequencyEvaluated && modernFrequencyResult.behaviorEligible ? 1 : 0);
    Serial.print(" reject_reason=");
    Serial.print(modernFrequencyEvaluated && modernFrequencyResult.valid
        ? detection::patternRejectReasonName(modernFrequencyResult.rejectReason)
        : "no_candidate");
    Serial.print(" transient_duration_ms=");
    Serial.print(diagnostics.acceptedTransientDurationMs);
    Serial.print(" transient_peak_strength=");
    Serial.print(diagnostics.acceptedTransientStrength, 1);
    Serial.print(" transient_age_or_dt_ms=");
    if (diagnostics.transientAccepted && diagnostics.acceptedTransientMs >= _sequenceTest.currentTrialStartMs) {
        Serial.print(static_cast<long>(diagnostics.acceptedTransientMs) - static_cast<long>(_sequenceTest.currentTrialStartMs));
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(" freq_present=");
    Serial.print(freq.present ? 1 : 0);
    Serial.print(" freq_matched=");
    Serial.print(modernFrequencyEvaluated && modernFrequencyResult.valid && modernFrequencyResult.tonalValid ? 1 : 0);
    Serial.print(" freq_score_ok=");
    Serial.print(freqEval.scoreOk ? 1 : 0);
    Serial.print(" freq_contrast_ok=");
    Serial.print(freqEval.contrastOk ? 1 : 0);
    Serial.print(" freq_score=");
    Serial.print(freq.score, 1);
    Serial.print(" freq_conf=");
    Serial.print(freq.confidence, 1);
    Serial.print(" freq_target_hz=");
    Serial.print(freq.targetHz);
    Serial.print(" freq_target_power=");
    Serial.print(freq.targetPower, 1);
    Serial.print(" freq_neighbor_power=");
    Serial.print(freq.neighborPower, 1);
    Serial.print(" freq_total_energy=");
    Serial.print(freq.totalEnergy, 1);
    Serial.print(" freq_contrast=");
    Serial.print(freq.spectralContrast, 2);
    Serial.print(" freq_observed_at_ms=");
    Serial.print(freq.observedAtMs);
    Serial.print(" freq_age_ms=");
    Serial.print(freqAgeMs);
    Serial.print("ms");
    Serial.print(" freq_valid_window=");
    Serial.print(freq.validWindow ? 1 : 0);
    Serial.print(" freq_eval_reason=");
    Serial.print(modernFrequencyEvaluated && modernFrequencyResult.valid
        ? detection::patternRejectReasonName(modernFrequencyResult.rejectReason)
        : FrequencyEvidenceEvaluation::reasonName(freqEval.reason));
    Serial.print(" compatCand[present=");
    Serial.print(freq.present ? 1 : 0);
    Serial.print(" source=");
    Serial.print(modernFrequencyEvaluated && modernFrequencyResult.valid
        ? detection::patternSourceName(modernFrequencyResult.source)
        : "comparison_only");
    Serial.print(" first_ms=");
    Serial.print(diagnostics.transientAccepted ? diagnostics.acceptedTransientMs : diagnostics.duplicateTransientMs);
    Serial.print(" peak_ms=");
    Serial.print(diagnostics.transientAccepted ? diagnostics.acceptedTransientPeakMs : diagnostics.duplicateTransientPeakMs);
    Serial.print(" release_ms=");
    Serial.print(diagnostics.transientAccepted ? diagnostics.acceptedTransientReleaseMs : diagnostics.duplicateTransientReleaseMs);
    Serial.print(" dur_ms=");
    Serial.print(diagnostics.transientAccepted ? diagnostics.acceptedTransientDurationMs : diagnostics.duplicateTransientDurationMs);
    Serial.print(" strength=");
    Serial.print(diagnostics.transientAccepted ? diagnostics.acceptedTransientStrength : diagnostics.duplicateTransientStrength, 1);
    Serial.print(" candidate_reject=");
    Serial.print(diagnostics.transientAccepted ? "none" : strongestRejectReasonName);
    Serial.print(" next_suppress=");
    Serial.print(_sequenceTest.liveFrequency.suppressReason);
    Serial.print("]");
    if (diagnostics.duplicateCount > 0) {
        Serial.print(" dup[dur=");
        Serial.print(diagnostics.duplicateTransientDurationMs);
        Serial.print(" strength=");
        Serial.print(diagnostics.duplicateTransientStrength, 1);
        Serial.print(" delta_ms=");
        Serial.print(diagnostics.duplicateDeltaFromPrimaryMs);
        Serial.print("]");
    }
    Serial.print(" detector_candidates=");
    Serial.print(diagnostics.rawCandidateCount);
    Serial.print(" accepted=");
    Serial.print(diagnostics.transientAccepted ? 1 : 0);
    Serial.print(" duplicates=");
    Serial.println(diagnostics.duplicateCount);

    if (!analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_EXPLAIN)) {
        return;
    }

    Serial.print("SEQ_EXPLAIN_LEGACY timing trial_start_ms=");
    Serial.print(_sequenceTest.currentTrialScheduledAtMs);
    Serial.print(" trigger_sent_ms=");
    Serial.print(_sequenceTest.currentTrialStartMs);
    Serial.print(" first_candidate_ms=");
    if (diagnostics.firstCandidateMs > 0) {
        Serial.print(diagnostics.firstCandidateMs);
    } else {
        Serial.print("-");
    }
    Serial.print(" ambient_baseline_avg=");
    Serial.print(ambientBaselineAvg, 1);
    Serial.print(" ambient_baseline_min=");
    Serial.print(diagnostics.ambientBaselineSamples > 0 ? diagnostics.ambientBaselineMin : 0.0f, 1);
    Serial.print(" ambient_baseline_max=");
    Serial.print(diagnostics.ambientBaselineSamples > 0 ? diagnostics.ambientBaselineMax : 0.0f, 1);
    Serial.print(" max_signal_level=");
    Serial.println(diagnostics.maxSignalLevel);

    Serial.print("SEQ_EXPLAIN_LEGACY origin_counts={pre_window:");
    Serial.print(diagnostics.candidatePreWindowCount);
    Serial.print(",in_window:");
    Serial.print(diagnostics.candidateInWindowCount);
    Serial.print(",post_window:");
    Serial.print(diagnostics.candidatePostWindowCount);
    Serial.println("}");

    Serial.print("SEQ_EXPLAIN_LEGACY rejects={too_short:");
    Serial.print(diagnostics.transientRejectTooShortCount);
    Serial.print(",too_long:");
    Serial.print(diagnostics.transientRejectTooLongCount);
    Serial.print(",weak:");
    Serial.print(diagnostics.transientRejectWeakCount);
    Serial.println("}");

    Serial.print("SEQ_EXPLAIN_LEGACY strongest_reject={reason:");
    if (totalRejects > 0) {
        Serial.print(strongestRejectReasonName);
        Serial.print(",dt:");
        if (diagnostics.strongestRejectDtFromTriggerMs >= 0) {
            Serial.print(diagnostics.strongestRejectDtFromTriggerMs);
        } else {
            Serial.print("-");
        }
        Serial.print(",dur:");
        Serial.print(diagnostics.strongestRejectDurationMs);
        Serial.print(",strength:");
        Serial.print(diagnostics.strongestRejectStrength, 1);
    } else {
        Serial.print("none,dt:-,dur:0,strength:0.0");
    }
    Serial.println("}");

    Serial.print("SEQ_EXPLAIN_LEGACY best_candidate={dt:");
    if (diagnostics.bestCandidateValid) {
        Serial.print(diagnostics.bestCandidateDtFromTriggerMs);
    } else {
        Serial.print("-");
    }
    Serial.print(",dur:");
    if (diagnostics.bestCandidateValid) {
        Serial.print(diagnostics.bestCandidateDurationMs);
    } else {
        Serial.print("-");
    }
    Serial.print(",end_dt:");
    if (diagnostics.bestCandidateValid && diagnostics.bestCandidateDtFromTriggerMs >= 0) {
        Serial.print(diagnostics.bestCandidateDtFromTriggerMs + static_cast<long>(diagnostics.bestCandidateDurationMs));
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(",strength:");
    if (diagnostics.bestCandidateValid) {
        Serial.print(diagnostics.bestCandidateStrength, 1);
    } else {
        Serial.print("0");
    }
    Serial.println("}");

    if (isMiss || isLate || isUnexpected || hasDuplicates || expectedDtSlow || expectedDurLong) {
        Serial.print("SEQ_EXPLAIN_LEGACY issues=[");
        bool firstIssue = true;
        auto printIssue = [&](const char* label) {
            if (!firstIssue) {
                Serial.print(",");
            }
            Serial.print(label);
            firstIssue = false;
        };
        if (isMiss) {
            printIssue("miss");
        }
        if (isLate) {
            printIssue("late");
        }
        if (isUnexpected) {
            printIssue("unexpected");
        }
        if (hasDuplicates) {
            printIssue("duplicates");
        }
        if (expectedDtSlow) {
            printIssue("expected_dt_gt_200ms");
        }
        if (expectedDurLong) {
            printIssue("expected_dur_gt_180ms");
        }
        Serial.println("]");
    }

    if (analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_EXPLAIN)) {
        Serial.print("SEQ_EXPLAIN_LEGACY duplicate_dts=[");
        for (unsigned long i = 0; i < diagnostics.duplicateDtCount; ++i) {
            if (i > 0) {
                Serial.print(",");
            }
            Serial.print(diagnostics.duplicateDts[i]);
        }
        Serial.println("]");
        for (unsigned long i = 0; i < diagnostics.candidateCount; ++i) {
            const auto& entry = diagnostics.candidates[i];
            const char* originName = "in_window";
            switch (entry.origin) {
                case SequenceTest::CandidateOrigin::PreWindow:
                    originName = "pre_window";
                    break;
                case SequenceTest::CandidateOrigin::InWindow:
                    originName = "in_window";
                    break;
                case SequenceTest::CandidateOrigin::PostWindow:
                    originName = "post_window";
                    break;
            }
            Serial.print("SEQ_EXPLAIN_LEGACY candidate[");
            Serial.print(i);
            Serial.print("] origin=");
            Serial.print(originName);
            Serial.print(" onset_dt_ms=");
            Serial.print(entry.dtFromTriggerMs);
            Serial.print(" dur=");
            Serial.print(entry.durationMs);
            Serial.print(" end_dt_ms=");
            if (entry.dtFromTriggerMs >= 0) {
                Serial.print(entry.dtFromTriggerMs + static_cast<long>(entry.durationMs));
                Serial.print("ms");
            } else {
                Serial.print("-");
            }
            Serial.print(" strength=");
            Serial.println(entry.strength, 1);
        }
    }
}

void AnalyzerApp::printSequenceAmpWindow(const AnalyzerReport& report) const {
    if (_valMode) {
        return;
    }
    if (!analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_CUSTOM) &&
        !analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_EXPLAIN)) {
        return;
    }

    const auto printMs = [](long value) {
        if (value >= 0) {
            Serial.print(value);
            Serial.print("ms");
        } else {
            Serial.print("-1ms");
        }
    };

    Serial.println();
    Serial.print("SEQ_AMP_WINDOW trial=");
    Serial.print(report.context.trial);
    Serial.print(" dt=");
    printMs(report.primaryPattern.dtMs);
    Serial.print(" win=");
    Serial.print(static_cast<long>(report.profileDetail.ampWindow.windowStartMs));
    Serial.print("..");
    Serial.print(static_cast<long>(report.profileDetail.ampWindow.windowEndMs));
    Serial.print("ms");
    Serial.print(" available=");
    Serial.print(report.profileDetail.ampWindow.available ? 1 : 0);
    Serial.print(" amp_support=");
    Serial.print(report.profileDetail.ampWindow.supportClass != nullptr ? report.profileDetail.ampWindow.supportClass : "unknown");
    Serial.print(" peak=");
    Serial.print(report.profileDetail.ampWindow.peak, 1);
    Serial.print(" floor=");
    Serial.print(report.profileDetail.ampWindow.baseline, 1);
    Serial.print(" lift=");
    Serial.print(report.profileDetail.ampWindow.lift, 1);
    Serial.print(" note=");
    Serial.println(report.profileDetail.ampWindow.note != nullptr ? report.profileDetail.ampWindow.note : "none");
}

void AnalyzerApp::printSequenceLegacyReports() const {
    if (_valMode) {
        return;
    }
    if (_sequenceTest.logFlags == AnalyzerApp::ANALYZER_LOG_CUSTOM) {
        return;
    }
    if (!sequenceLegacyReportEnabled()) {
        return;
    }
    if (_sequenceTest.deprecatedTrialReports == nullptr || _sequenceTest.deprecatedTrialReportCount == 0) {
        return;
    }

    Serial.println("SEQ_REPORT_BEGIN");
    for (size_t i = 0; i < _sequenceTest.deprecatedTrialReportCount && i < _sequenceTest.deprecatedTrialReportCapacity; ++i) {
        const auto& report = _sequenceTest.deprecatedTrialReports[i];
        const char* candidateClass = h3SequenceCandidateClassFromResult(report.result);
        const auto& freq = report.freqEarly;
        const bool hasAmp = report.durMs >= 0 || report.strength > 0.0f;
        const bool cmpHasFreq = freq.present;
        const unsigned long freqPeakMinusAmpPeakMs = cmpHasFreq && report.bestCandidateValid
            ? (freq.observedAtMs >= report.startMs ? static_cast<unsigned long>(freq.observedAtMs - report.startMs) : 0UL)
            : 0UL;

        Serial.print("SEQ_REPORT trial=");
        Serial.print(report.trialNumber);
        Serial.print(" artifact_state=LEGACY_DIAGNOSTICS");
        Serial.print(" result=");
        Serial.print(report.result);
        Serial.print(" candidate_class=");
        Serial.print(candidateClass);
        Serial.print(" start_ms=");
        Serial.print(report.startMs);
        Serial.print(" end_ms=");
        Serial.print(report.endMs);
        Serial.print(" dt_ms=");
        if (report.dtMs >= 0) {
            Serial.print(report.dtMs);
            Serial.print("ms");
        } else {
            Serial.print("-");
        }
        Serial.print(" dur_ms=");
        if (report.durMs >= 0) {
            Serial.print(report.durMs);
            Serial.print("ms");
        } else {
            Serial.print("-");
        }
        Serial.print(" freq[");
        Serial.print("valid=");
        Serial.print(freq.present ? 1 : 0);
        Serial.print(" source=");
        Serial.print(freq.present ? "frequency_primary" : "comparison_only");
        Serial.print(" first_ms=");
        Serial.print(freq.observedAtMs);
        Serial.print(" peak_ms=");
        Serial.print(freq.observedAtMs);
        Serial.print(" release_ms=");
        Serial.print(freq.observedAtMs);
        Serial.print(" dur_or_hold_ms=");
        Serial.print(freq.present ? report.durMs : -1);
        Serial.print(" score=");
        Serial.print(freq.score, 1);
        Serial.print(" contrast=");
        Serial.print(freq.spectralContrast, 2);
        Serial.print(" reject=");
        Serial.print(freq.present ? "none" : "comparison_only");
        Serial.print("]");
        if (hasAmp) {
            Serial.print(" amp[dur=");
            Serial.print(report.durMs >= 0 ? report.durMs : 0);
            Serial.print(" strength=");
            Serial.print(report.strength, 1);
            Serial.print("]");
        }
        Serial.print(" cmp[freqPeakMinusAmpPeakMs=");
        if (cmpHasFreq && hasAmp) {
            Serial.print(freqPeakMinusAmpPeakMs);
            Serial.print("ms");
        } else {
            Serial.print("-");
        }
        Serial.print("]");
        Serial.println();
    }
    Serial.println("SEQ_REPORT_END");
}

void AnalyzerApp::printSequenceTrialResult(unsigned long trialNumber, const char* result, long dtMs, long durMs, float strength, bool audioOverflow, unsigned long duplicateCount, const SequenceTest::TrialDiagnostics& diagnostics) const {
    if (_valMode) {
        return;
    }
    if (_sequenceTest.logFlags == AnalyzerApp::ANALYZER_LOG_CUSTOM) {
        return;
    }
    if (!analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_TRIAL)) {
        return;
    }

    const auto buildTrialSignal = [&]() {
        if (!diagnostics.transientAccepted) {
            return makeModernFrequencySignalCandidate(_sequenceTest.liveFrequency);
        }

        detection::SignalCandidate signal = {};
        signal.kind = detection::SignalKind::FrequencyMatch;
        signal.source = detection::SignalSource::Frequency;
        signal.detectorKind = detection::SignalDetectorKind::FrequencyMatch;
        signal.present = true;
        signal.valid = true;
        signal.startSample = diagnostics.acceptedTransientOnsetSample;
        signal.peakSample = diagnostics.acceptedTransientPeakSample;
        signal.releaseSample = diagnostics.acceptedTransientReleaseSample;
        signal.startMs = diagnostics.acceptedTransientMs;
        signal.peakMs = diagnostics.acceptedTransientPeakMs != 0 ? diagnostics.acceptedTransientPeakMs : diagnostics.acceptedTransientMs;
        signal.releaseMs = diagnostics.acceptedTransientReleaseMs != 0 ? diagnostics.acceptedTransientReleaseMs : signal.peakMs;
        signal.endMs = signal.releaseMs;
        signal.durationMs = diagnostics.acceptedTransientDurationMs;
        signal.strength = diagnostics.acceptedTransientStrength;
        signal.score = diagnostics.acceptedFrequencyEvidence.present ? diagnostics.acceptedFrequencyEvidence.score : diagnostics.acceptedTransientStrength;
        signal.contrast = diagnostics.acceptedFrequencyEvidence.present ? diagnostics.acceptedFrequencyEvidence.spectralContrast : 0.0f;
        signal.confidence = diagnostics.acceptedFrequencyEvidence.present && diagnostics.acceptedFrequencyEvidence.matched ? 1.0f : 0.0f;
        signal.signalConfidence = signal.confidence;
        signal.frequencyConfidence = signal.confidence;
        signal.ampLevel = diagnostics.acceptedTransientStrength;
        signal.ampBaseline = diagnostics.acceptedAmbientBaseline;
        signal.ampEvidencePresent = true;
        signal.frequency = diagnostics.acceptedFrequencyEvidence;
        signal.frequency.present = true;
        signal.frequency.observedAtMs = diagnostics.acceptedFrequencyProcessedAtMs;
        return signal;
    };

    const detection::SignalCandidate trialSignal = buildTrialSignal();
    detection::PatternResult trialResult = {};
    detection::InspectedSignal trialInspected = {};
    const bool trialEvaluated = evaluateModernSignalCandidate(trialSignal, trialResult, &trialInspected);
    const unsigned long probeStartMs = trialSignal.startMs > 20UL ? trialSignal.startMs - 20UL : 0UL;
    const unsigned long probeEndMs = trialSignal.endMs != 0
        ? trialSignal.endMs + 20UL
        : (trialSignal.releaseMs != 0 ? trialSignal.releaseMs + 20UL : trialSignal.peakMs + 20UL);
    const size_t ampSampleCount = _sequenceFeatureHistory != nullptr
        ? _sequenceFeatureHistory->sampleCount(detection::FeatureStreamId::AmpEnvelope)
        : 0U;
    const size_t floorSampleCount = _sequenceFeatureHistory != nullptr
        ? _sequenceFeatureHistory->sampleCount(detection::FeatureStreamId::AmbientFloor)
        : 0U;
    const unsigned long ampLatestMs = _sequenceFeatureHistory != nullptr
        ? _sequenceFeatureHistory->latestTimeMs(detection::FeatureStreamId::AmpEnvelope)
        : 0UL;
    const unsigned long floorLatestMs = _sequenceFeatureHistory != nullptr
        ? _sequenceFeatureHistory->latestTimeMs(detection::FeatureStreamId::AmbientFloor)
        : 0UL;
    const detection::ScalarWindow ampWindow = _sequenceFeatureHistory != nullptr
        ? _sequenceFeatureHistory->getWindow(detection::FeatureStreamId::AmpEnvelope, probeStartMs, probeEndMs)
        : detection::ScalarWindow{};
    const detection::ScalarWindow floorWindow = _sequenceFeatureHistory != nullptr
        ? _sequenceFeatureHistory->getWindow(detection::FeatureStreamId::AmbientFloor, probeStartMs, probeEndMs)
        : detection::ScalarWindow{};

    const bool briefTrial = analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_TRIAL_BRIEF);
    if (briefTrial) {
        const bool accepted = strcmp(result, "expected") == 0 || strcmp(result, "late") == 0;
        const float freqScore = trialEvaluated ? trialResult.freq.score : (diagnostics.transientAccepted ? diagnostics.acceptedTransientStrength : _sequenceTest.liveFrequency.frequencyCandidate.score);
        const float freqContrast = trialEvaluated ? trialResult.freq.spectralContrast : (diagnostics.transientAccepted ? diagnostics.acceptedFrequencyEvidence.spectralContrast : _sequenceTest.liveFrequency.candidatePeakContrast);
        const char* ampSupport = trialEvaluated ? ampSupportName(trialResult.ampSupport) : "Unknown";
        const float ampPeak = trialEvaluated ? trialInspected.signal.ampLevel : trialSignal.ampLevel;
        const float ampBaseline = trialEvaluated ? trialInspected.signal.ampBaseline : trialSignal.ampBaseline;
        const float ampLift = ampPeak - ampBaseline;

        Serial.println();
        Serial.print("SEQ_TRIAL trial=");
        Serial.print(trialNumber);
        Serial.print(" accept=");
        Serial.print(accepted ? 1 : 0);
        Serial.print(" dur=");
        if (durMs >= 0) {
            Serial.print(durMs);
            Serial.print("ms");
        } else {
            Serial.print("-");
        }
        Serial.print(" freq_score=");
        Serial.print(freqScore, 1);
        Serial.print(" freq_contrast=");
        Serial.print(freqContrast, 2);
        Serial.print(" amp_support=");
        Serial.print(ampSupport);
        Serial.print(" amp_peak=");
        Serial.print(ampPeak, 1);
        Serial.print(" diagnostic_floor=");
        Serial.print(ampBaseline, 1);
        Serial.print(" amp_lift=");
        Serial.print(ampLift, 1);
        Serial.print(" amp_hist=");
        Serial.print(ampSampleCount);
        Serial.print("/");
        Serial.print(floorSampleCount);
        Serial.print(" amp_latest_ms=");
        Serial.print(ampLatestMs);
        Serial.print("/");
        Serial.print(floorLatestMs);
        Serial.print(" amp_window=");
        Serial.print(ampWindow.valid ? 1 : 0);
        Serial.print("/");
        Serial.print(floorWindow.valid ? 1 : 0);
        Serial.println();
        return;
    }

    Serial.println();
    Serial.print("SEQ_TRIAL trial=");
    Serial.print(trialNumber);
    Serial.print(" result=");
    Serial.print(result);
    Serial.print(" dt=");
    if (dtMs >= 0) {
        Serial.print(dtMs);
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(" dur=");
    if (durMs >= 0) {
        Serial.print(durMs);
        Serial.print("ms");
    } else {
        Serial.print("-");
    }
    Serial.print(" dur_class=");
    Serial.print(sequenceTrialDurationClass(durMs));
    Serial.print(" strength=");
    Serial.print(strength, 1);
    Serial.print(" freq_score=");
    Serial.print(trialEvaluated ? trialResult.freq.score : (diagnostics.transientAccepted ? diagnostics.acceptedTransientStrength : _sequenceTest.liveFrequency.frequencyCandidate.score), 1);
    Serial.print(" freq_contrast=");
    Serial.print(trialEvaluated ? trialResult.freq.spectralContrast : (diagnostics.transientAccepted ? diagnostics.acceptedFrequencyEvidence.spectralContrast : _sequenceTest.liveFrequency.candidatePeakContrast), 2);
    Serial.print(" dup=");
    Serial.print(duplicateCount);
    Serial.print(" candidates=");
    Serial.print(diagnostics.candidateCount);
    const uint32_t trialSampleRateHz = _audioSource.sampleRateHz() > 0 ? _audioSource.sampleRateHz() : 16000UL;
    const bool cmpHasAmp = !_sequenceTest.liveFrequencyOnly && (diagnostics.transientAccepted || diagnostics.duplicateCount > 0);
    const uint64_t ampOnsetSample = diagnostics.transientAccepted
        ? diagnostics.acceptedTransientOnsetSample
        : diagnostics.duplicateCount > 0 ? diagnostics.duplicateTransientOnsetSample : 0ULL;
    const uint64_t ampPeakSample = diagnostics.transientAccepted
        ? diagnostics.acceptedTransientPeakSample
        : diagnostics.duplicateCount > 0 ? diagnostics.duplicateTransientPeakSample : 0ULL;
    const uint64_t ampReleaseSample = diagnostics.transientAccepted
        ? diagnostics.acceptedTransientReleaseSample
        : diagnostics.duplicateCount > 0 ? diagnostics.duplicateTransientReleaseSample : 0ULL;
    const unsigned long ampOnsetMs = diagnostics.transientAccepted
        ? diagnostics.acceptedTransientMs
        : diagnostics.duplicateCount > 0 ? diagnostics.duplicateTransientMs : 0UL;
    const unsigned long ampPeakMs = diagnostics.transientAccepted
        ? diagnostics.acceptedTransientPeakMs
        : diagnostics.duplicateCount > 0 ? diagnostics.duplicateTransientPeakMs : 0UL;
    const unsigned long ampReleaseMs = diagnostics.transientAccepted
        ? diagnostics.acceptedTransientReleaseMs
        : diagnostics.duplicateCount > 0 ? diagnostics.duplicateTransientReleaseMs : 0UL;
    const auto& freqCand = _sequenceTest.liveFrequency.frequencyCandidate;
    Serial.print(" proposerCand[");
    Serial.print("valid=");
    Serial.print(freqCand.valid ? 1 : 0);
    Serial.print(" source=");
    Serial.print(sequenceFrequencyCandidateSourceName(freqCand.valid));
    Serial.print(" proposer_first_ms=");
    Serial.print(freqCand.startMs);
    Serial.print(" proposer_peak_ms=");
    Serial.print(freqCand.peakMs);
    Serial.print(" proposer_release_ms=");
    Serial.print(freqCand.releaseMs);
    Serial.print(" proposer_hold_ms=");
    Serial.print(freqCand.durationMs);
    Serial.print(" proposer_score=");
    Serial.print(freqCand.score, 1);
    Serial.print(" proposer_contrast=");
    Serial.print(freqCand.contrast, 2);
    Serial.print(" candidate_reject=");
    Serial.print(freqCand.valid ? "none" : _sequenceTest.liveFrequency.suppressReason);
    Serial.print(" next_suppress=");
    Serial.print(_sequenceTest.liveFrequency.suppressReason);
    Serial.print("]");
    const detection::FrequencyEvidence* windowEarlyEvidence = nullptr;
    const detection::FrequencyEvidence* windowFullEvidence = nullptr;
    if (diagnostics.transientAccepted) {
        windowEarlyEvidence = &diagnostics.acceptedFrequencyEvidence;
        windowFullEvidence = &diagnostics.acceptedFrequencyEvidenceFull;
    } else if (diagnostics.duplicateCount > 0) {
        windowEarlyEvidence = &diagnostics.duplicateFrequencyEvidence;
        windowFullEvidence = &diagnostics.duplicateFrequencyEvidenceFull;
    }
    const bool hasWindowEvidence = windowEarlyEvidence != nullptr && windowFullEvidence != nullptr;
    const FrequencyEvidenceEvaluation::Evaluation windowEarlyEval = hasWindowEvidence
        ? FrequencyEvidenceEvaluation::evaluate(*windowEarlyEvidence, _frequencyEvidenceTuning)
        : FrequencyEvidenceEvaluation::Evaluation{};
    const FrequencyEvidenceEvaluation::Evaluation windowFullEval = hasWindowEvidence
        ? FrequencyEvidenceEvaluation::evaluate(*windowFullEvidence, _frequencyEvidenceTuning)
        : FrequencyEvidenceEvaluation::Evaluation{};
    const bool ampCandPresent = cmpHasAmp;
    Serial.print(" freqCompare[proposer_state=");
    Serial.print(_sequenceTest.liveFrequency.candidateState);
    Serial.print(" proposer_valid=");
    Serial.print(_sequenceTest.liveFrequency.frequencyCandidate.valid ? 1 : 0);
    Serial.print(" proposer_matched=");
    Serial.print(_sequenceTest.liveFrequency.wouldProduceCandidate ? 1 : 0);
    Serial.print(" proposer_ready=");
    Serial.print(_sequenceTest.liveFrequency.readyOk ? 1 : 0);
    Serial.print(" proposer_gate=");
    Serial.print(_sequenceTest.liveFrequency.gateOpen ? 1 : 0);
    Serial.print(" proposer_suppress=");
    Serial.print(_sequenceTest.liveFrequency.suppressReason);
    Serial.print(" proposer_would=");
    Serial.print(_sequenceTest.liveFrequency.wouldCandidateReason);
    Serial.print(" proposer_first_ms=");
    Serial.print(_sequenceTest.liveFrequency.candidateFirstSeenMs);
    Serial.print(" proposer_peak_ms=");
    Serial.print(_sequenceTest.liveFrequency.candidatePeakMs);
    Serial.print(" proposer_release_ms=");
    Serial.print(_sequenceTest.liveFrequency.candidateReleaseMs);
    Serial.print(" proposer_hold_ms=");
    Serial.print(_sequenceTest.liveFrequency.candidateHoldMs);
    Serial.print(" proposer_score=");
    Serial.print(_sequenceTest.liveFrequency.candidatePeakScore, 1);
    Serial.print(" proposer_contrast=");
    Serial.print(_sequenceTest.liveFrequency.candidatePeakContrast, 2);
    Serial.print(" proposer_source=");
    Serial.print(_sequenceTest.liveFrequency.frequencyCandidate.valid ? "frequency_primary" : "comparison_only");
    Serial.print(" proposer_reject=");
    Serial.print(_sequenceTest.liveFrequency.frequencyCandidate.valid ? "none" : _sequenceTest.liveFrequency.suppressReason);
    Serial.print(" window_present=");
    Serial.print(hasWindowEvidence ? 1 : 0);
    Serial.print(" window_reason=");
    if (hasWindowEvidence) {
        Serial.print("none");
    } else {
        Serial.print("not_measured");
    }
    Serial.print(" windowEarly_score=");
    Serial.print(hasWindowEvidence ? windowEarlyEval.score : 0.0f, 1);
    Serial.print(" windowEarly_contrast=");
    Serial.print(hasWindowEvidence ? windowEarlyEval.contrast : 0.0f, 2);
    Serial.print(" windowEarly_matched=");
    Serial.print(hasWindowEvidence && windowEarlyEval.matched ? 1 : 0);
    Serial.print(" windowFull_score=");
    Serial.print(hasWindowEvidence ? windowFullEval.score : 0.0f, 1);
    Serial.print(" windowFull_contrast=");
    Serial.print(hasWindowEvidence ? windowFullEval.contrast : 0.0f, 2);
    Serial.print(" windowFull_matched=");
    Serial.print(hasWindowEvidence && windowFullEval.matched ? 1 : 0);
    Serial.print(" windowEarly_obs_ms=");
    Serial.print(hasWindowEvidence ? windowEarlyEvidence->observedAtMs : 0UL);
    Serial.print(" windowFull_obs_ms=");
    Serial.print(hasWindowEvidence ? windowFullEvidence->observedAtMs : 0UL);
    Serial.print("]");
    Serial.print(" sourceCand[present=");
    Serial.print(trialSignal.present ? 1 : 0);
    Serial.print(" source=frequency");
    Serial.print(" first_ms=");
    Serial.print(trialSignal.startMs);
    Serial.print(" peak_ms=");
    Serial.print(trialSignal.peakMs);
    Serial.print(" release_ms=");
    Serial.print(trialSignal.releaseMs);
    Serial.print(" dur_or_hold_ms=");
    Serial.print(trialSignal.durationMs);
    Serial.print(" score=");
    Serial.print(trialSignal.score, 1);
    Serial.print(" contrast=");
    Serial.print(trialSignal.contrast, 2);
    Serial.print(" candidate_reject=");
    Serial.print(trialSignal.valid ? "none" : _sequenceTest.liveFrequency.suppressReason);
    Serial.print(" next_suppress=");
    Serial.print(_sequenceTest.liveFrequency.suppressReason);
    Serial.print("]");
    if (cmpHasAmp) {
        const unsigned long freqPeakMs = trialSignal.valid ? trialSignal.peakMs : 0UL;
        const long freqPeakMinusAmpPeakMs = trialSignal.valid
            ? static_cast<long>(freqPeakMs) - static_cast<long>(ampPeakMs)
            : 0L;
        Serial.print(" ampCand[present=");
        Serial.print(1);
        if (ampCandPresent) {
            Serial.print(" onset_ms=");
            Serial.print(ampOnsetMs);
            Serial.print(" peak_ms=");
            Serial.print(ampPeakMs);
            Serial.print(" release_ms=");
            Serial.print(ampReleaseMs);
            Serial.print(" dur_ms=");
            Serial.print(diagnostics.transientAccepted ? diagnostics.acceptedTransientDurationMs : diagnostics.duplicateTransientDurationMs);
            Serial.print(" strength=");
            Serial.print(diagnostics.transientAccepted ? diagnostics.acceptedTransientStrength : diagnostics.duplicateTransientStrength, 1);
        }
        Serial.print("]");
        Serial.print(" amp_peak=");
        Serial.print(trialSignal.ampLevel, 1);
        Serial.print(" amp_base=");
        Serial.print(trialSignal.ampBaseline, 1);
        Serial.print(" amp_lift=");
        Serial.print(trialSignal.ampLevel - trialSignal.ampBaseline, 1);
        Serial.print(" cmp[freqPeakMinusAmpPeakMs=");
        if (trialSignal.valid) {
            Serial.print(freqPeakMinusAmpPeakMs);
            Serial.print("ms");
        } else {
            Serial.print("-");
        }
        Serial.print("]");
    } else {
        Serial.print(" ampCand[present=0]");
    }
    Serial.println();

    (void)audioOverflow;
    (void)diagnostics;
}

void AnalyzerApp::printDetectionParameters() const {
    if (_valMode) {
        return;
    }
    Serial.print("SEQ det mode=");
    Serial.print("AMP");
    Serial.print(" onset=");
    Serial.print(detectorOnsetDetectionThreshold(), 1);
    Serial.print(" release=");
    Serial.print(detectorOnsetReleaseThreshold(), 1);
    Serial.print(" cooldown=");
    Serial.print(detectorCooldownAfterOnsetMs());
    Serial.print(" releaseDebounce=");
    Serial.print(detectorReleaseDebounceMs());
    Serial.print(" minMs=");
    Serial.print(detectorMinTransientDurationMs());
    Serial.print(" maxMs=");
    Serial.print(detectorMaxTransientDurationMs());
    Serial.print(" minStrength=");
    Serial.print(detectorMinTransientPeakStrength(), 1);
    Serial.println();
}

void AnalyzerApp::printTransientAcceptedDebug(unsigned long now, float strength, unsigned long durationMs) const {
    if (_valMode) {
        return;
    }
    Serial.print("DET transient accepted t=");
    Serial.print(now);
    Serial.print(" dur=");
    Serial.print(durationMs);
    Serial.print(" strength=");
    Serial.println(strength, 1);
}

void AnalyzerApp::printTransientStatsDebug(unsigned long now) const {
    if (_valMode) {
        return;
    }
    const unsigned long elapsedMs = now - _sequenceTest.startedAtMs;
    const unsigned long expectedCount = (elapsedMs + (detectorCooldownAfterOnsetMs() / 2)) / detectorCooldownAfterOnsetMs();
    const unsigned long successRate = expectedCount > 0 ? ((_sequenceTest.hits * 100UL) / expectedCount) : 0;

    Serial.print("DET transient stats t=");
    Serial.print(now);
    Serial.print(" hits=");
    Serial.print(_sequenceTest.hits);
    Serial.print(" expected=");
    Serial.print(expectedCount);
    Serial.print(" success=");
    Serial.print(successRate);
    Serial.println("%");
}

void AnalyzerApp::printSequenceSummary() const {
    if (_valMode) {
        return;
    }
    if (_sequenceTest.logFlags == AnalyzerApp::ANALYZER_LOG_CUSTOM) {
        return;
    }
    const bool verboseTrialReports = sequenceLegacyReportEnabled();
    const bool summaryEnabled = analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_SUMMARY);
    if (!verboseTrialReports && !summaryEnabled) {
        return;
    }
    if (verboseTrialReports) {
        printSequenceLegacyReports();
    }
    if (!summaryEnabled) {
        return;
    }
    AnalyzerSummary summary = {};
    summary.profileName = activeAnalyzerProfileName();
    summary.trials = _sequenceTest.totalTrials;

    const size_t reasonCount = static_cast<size_t>(AnalyzerReason::Unknown) + 1U;
    unsigned int missReasonCounts[reasonCount] = {};
    unsigned int rejectReasonCounts[reasonCount] = {};

    unsigned long dtSumMs = 0;
    unsigned long dtCount = 0;
    unsigned long durSumMs = 0;
    unsigned long durCount = 0;
    float confidenceSum = 0.0f;
    bool capturedSummaryAvailable = false;
    unsigned int completedTrialCount = 0;

    if (_sequenceTest.deprecatedAnalyzerReports != nullptr) {
        const size_t limit = _sequenceTest.deprecatedAnalyzerReportCount < _sequenceTest.deprecatedAnalyzerReportCapacity
            ? _sequenceTest.deprecatedAnalyzerReportCount
            : _sequenceTest.deprecatedAnalyzerReportCapacity;
        for (size_t i = 0; i < limit; ++i) {
            capturedSummaryAvailable = true;
            ++completedTrialCount;

            const AnalyzerReport& report = _sequenceTest.deprecatedAnalyzerReports[i];
            if (report.debug.artifactCaptured) {
                // capturedSummaryAvailable already tells us these reports came from stored runtime output.
            }
            switch (report.classification.result) {
                case AnalyzerResult::Expected:
                    ++summary.expected;
                    break;
                case AnalyzerResult::Early:
                    ++summary.early;
                    break;
                case AnalyzerResult::Late:
                    ++summary.late;
                    break;
                case AnalyzerResult::Miss:
                    ++summary.miss;
                    break;
                case AnalyzerResult::Unexpected:
                    ++summary.unexpected;
                    break;
                case AnalyzerResult::Rejected:
                    ++summary.rejected;
                    break;
                case AnalyzerResult::Ambiguous:
                    ++summary.ambiguous;
                    break;
                case AnalyzerResult::TooDense:
                    ++summary.tooDense;
                    break;
                case AnalyzerResult::InvalidAudio:
                    ++summary.invalidAudio;
                    break;
                case AnalyzerResult::Unknown:
                default:
                    break;
            }

            if (report.debug.duplicates > 0) {
                ++summary.duplicate;
            }

            if (report.classification.dtMs >= 0) {
                dtSumMs += static_cast<unsigned long>(report.classification.dtMs);
                ++dtCount;
            }

            if (report.signals.primaryDurationMs > 0) {
                durSumMs += report.signals.primaryDurationMs;
                ++durCount;
            }

            const float reportConfidence = report.primaryPattern.confidence > 0.0f
                ? report.primaryPattern.confidence
                : report.classification.confidence;
            confidenceSum += reportConfidence;

            const size_t reasonIndex = analyzerReasonIndex(report.classification.reason);
            if (reasonIndex < reasonCount) {
                if (report.classification.result == AnalyzerResult::Miss) {
                    ++missReasonCounts[reasonIndex];
                } else if (report.classification.result == AnalyzerResult::Rejected ||
                           report.classification.result == AnalyzerResult::Ambiguous ||
                           report.classification.result == AnalyzerResult::TooDense ||
                           report.classification.result == AnalyzerResult::InvalidAudio) {
                    ++rejectReasonCounts[reasonIndex];
                }
            }
        }
    }

    if (!capturedSummaryAvailable) {
        summary.expected = _sequenceTest.expectedHits;
        summary.early = 0;
        summary.late = _sequenceTest.lateHits;
        summary.miss = _sequenceTest.misses;
        summary.duplicate = _sequenceTest.duplicates > 0 ? 1U : 0U;
        summary.unexpected = _sequenceTest.unexpected;
        summary.rejected = 0;
        summary.ambiguous = 0;
        summary.tooDense = 0;
        summary.invalidAudio = _sequenceTest.invalidAudio;
        summary.avgDtMs = -1.0f;
        summary.avgDurationMs = -1.0f;
        summary.avgConfidence = 0.0f;
        summary.completed = _sequenceTest.currentTrial;
        summary.duplicateRate = summary.completed > 0 ? static_cast<float>(summary.duplicate) / static_cast<float>(summary.completed) : 0.0f;
        summary.unexpectedRate = summary.completed > 0 ? static_cast<float>(summary.unexpected) / static_cast<float>(summary.completed) : 0.0f;
        completedTrialCount = summary.completed;
    } else {
        summary.avgDtMs = dtCount > 0 ? static_cast<float>(dtSumMs) / static_cast<float>(dtCount) : -1.0f;
        summary.avgDurationMs = durCount > 0 ? static_cast<float>(durSumMs) / static_cast<float>(durCount) : -1.0f;
        summary.completed = completedTrialCount;
        summary.avgConfidence = summary.completed > 0 ? confidenceSum / static_cast<float>(summary.completed) : 0.0f;
        summary.duplicateRate = summary.completed > 0 ? static_cast<float>(summary.duplicate) / static_cast<float>(summary.completed) : 0.0f;
        summary.unexpectedRate = summary.completed > 0 ? static_cast<float>(summary.unexpected) / static_cast<float>(summary.completed) : 0.0f;
    }

    if (summary.completed == 0) {
        summary.completed = capturedSummaryAvailable ? completedTrialCount : _sequenceTest.currentTrial;
    }

    auto selectMaxReason = [&](const unsigned int* counts, AnalyzerReason fallback) {
        unsigned int bestCount = 0;
        AnalyzerReason bestReason = fallback;
        for (size_t i = 0; i < reasonCount; ++i) {
            if (counts[i] > bestCount) {
                bestCount = counts[i];
                bestReason = static_cast<AnalyzerReason>(i);
            }
        }
        return bestReason;
    };

    summary.mainMissReason = selectMaxReason(missReasonCounts, AnalyzerReason::None);
    summary.mainRejectReason = selectMaxReason(rejectReasonCounts, AnalyzerReason::None);

    const long avgDtRounded = summary.avgDtMs >= 0.0f ? static_cast<long>(summary.avgDtMs + 0.5f) : -1L;
    const long avgDurRounded = summary.avgDurationMs >= 0.0f ? static_cast<long>(summary.avgDurationMs + 0.5f) : -1L;

    Serial.print("SEQ_SUMMARY profile=");
    Serial.print(summary.profileName != nullptr ? summary.profileName : "unknown");
    Serial.print(" trials=");
    Serial.print(summary.trials);
    Serial.print(" completed=");
    Serial.print(summary.completed);
    Serial.print(" expected=");
    Serial.print(summary.expected);
    Serial.print(" early=");
    Serial.print(summary.early);
    Serial.print(" late=");
    Serial.print(summary.late);
    Serial.print(" miss=");
    Serial.print(summary.miss);
    Serial.print(" duplicate=");
    Serial.print(summary.duplicate);
    Serial.print(" unexpected=");
    Serial.print(summary.unexpected);
    Serial.print(" rejected=");
    Serial.print(summary.rejected);
    Serial.print(" ambiguous=");
    Serial.print(summary.ambiguous);
    Serial.print(" too_dense=");
    Serial.print(summary.tooDense);
    Serial.print(" invalid_audio=");
    Serial.print(summary.invalidAudio);
    Serial.print(" avg_dt=");
    if (avgDtRounded >= 0) {
        Serial.print(avgDtRounded);
        Serial.print("ms");
    } else {
        Serial.print("-1ms");
    }
    Serial.print(" avg_dur=");
    if (avgDurRounded >= 0) {
        Serial.print(avgDurRounded);
        Serial.print("ms");
    } else {
        Serial.print("-1ms");
    }
    Serial.print(" avg_confidence=");
    Serial.print(summary.avgConfidence, 2);
    Serial.print(" duplicate_rate=");
    Serial.print(summary.duplicateRate, 2);
    Serial.print(" unexpected_rate=");
    Serial.print(summary.unexpectedRate, 2);
    Serial.print(" main_miss_reason=");
    Serial.print(analyzerReasonName(summary.mainMissReason));
    Serial.print(" main_reject_reason=");
    Serial.println(analyzerReasonName(summary.mainRejectReason));

    bool anyReasonCounts = false;
    for (size_t i = 0; i < reasonCount; ++i) {
        if (missReasonCounts[i] > 0 || rejectReasonCounts[i] > 0) {
            anyReasonCounts = true;
            break;
        }
    }

    if (anyReasonCounts || analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_CUSTOM)) {
        Serial.print("SEQ_REASON_COUNTS profile=");
        Serial.print(summary.profileName != nullptr ? summary.profileName : "unknown");
        Serial.print(" valid_pattern_in_expected_window=");
        Serial.print(missReasonCounts[analyzerReasonIndex(AnalyzerReason::ValidPatternInExpectedWindow)]);
        Serial.print(" valid_pattern_before_window=");
        Serial.print(missReasonCounts[analyzerReasonIndex(AnalyzerReason::ValidPatternBeforeWindow)]);
        Serial.print(" valid_pattern_after_window=");
        Serial.print(missReasonCounts[analyzerReasonIndex(AnalyzerReason::ValidPatternAfterWindow)]);
        Serial.print(" no_signal_candidate=");
        Serial.print(missReasonCounts[analyzerReasonIndex(AnalyzerReason::NoSignalCandidate)]);
        Serial.print(" signal_seen_but_rejected=");
        Serial.print(missReasonCounts[analyzerReasonIndex(AnalyzerReason::SignalSeenButRejected)]);
        Serial.print(" inspection_failed=");
        Serial.print(missReasonCounts[analyzerReasonIndex(AnalyzerReason::InspectionFailed)]);
        Serial.print(" pattern_candidate_rejected=");
        Serial.print(missReasonCounts[analyzerReasonIndex(AnalyzerReason::PatternCandidateRejected)]);
        Serial.print(" duplicate_pattern_after_primary=");
        Serial.print(rejectReasonCounts[analyzerReasonIndex(AnalyzerReason::DuplicatePatternAfterPrimary)]);
        Serial.print(" unexpected_valid_pattern_without_trigger=");
        Serial.print(rejectReasonCounts[analyzerReasonIndex(AnalyzerReason::UnexpectedValidPatternWithoutTrigger)]);
        Serial.print(" invalid_audio=");
        Serial.println(rejectReasonCounts[analyzerReasonIndex(AnalyzerReason::InvalidAudio)]);
    }

    if (analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_CUSTOM)) {
        Serial.print("SEQ_LEGACY_PROFILE_SUMMARY tonal_expected=");
        Serial.print(_sequenceTest.tonalExpected);
        Serial.print(" transient_only_expected=");
        Serial.print(_sequenceTest.transientOnlyExpected);
        Serial.print(" tonal_duplicates=");
        Serial.print(_sequenceTest.tonalDuplicates);
        Serial.print(" non_tonal_duplicates=");
        Serial.print(_sequenceTest.nonTonalDuplicates);
        Serial.print(" tonal_unexpected=");
        Serial.print(_sequenceTest.tonalUnexpected);
        Serial.print(" non_tonal_unexpected=");
        Serial.print(_sequenceTest.nonTonalUnexpected);
        Serial.print(" freq_reject_score=");
        Serial.print(_sequenceTest.freqRejectScore);
        Serial.print(" freq_reject_contrast=");
        Serial.print(_sequenceTest.freqRejectContrast);
        Serial.print(" freq_reject_both=");
        Serial.print(_sequenceTest.freqRejectBoth);
        Serial.print(" freq_reject_no_evidence=");
        Serial.print(_sequenceTest.freqRejectNoEvidence);
        Serial.print(" freq_reject_invalid_window=");
        Serial.println(_sequenceTest.freqRejectInvalidWindow);
    }
    if (kAnalyzerEnableRecheckParity && (_sequenceTest.parityCompared > 0 || _sequenceTest.parityMissingActual > 0 || _sequenceTest.parityMissingRecheck > 0)) {
        Serial.print("SEQ_PARITY_SUMMARY compared=");
        Serial.print(_sequenceTest.parityCompared);
        Serial.print(" match=");
        Serial.print(_sequenceTest.parityMatched);
        Serial.print(" mismatch=");
        Serial.print(_sequenceTest.parityCompared > _sequenceTest.parityMatched
            ? _sequenceTest.parityCompared - _sequenceTest.parityMatched
            : 0UL);
        Serial.print(" missing_actual=");
        Serial.print(_sequenceTest.parityMissingActual);
        Serial.print(" missing_recheck=");
        Serial.print(_sequenceTest.parityMissingRecheck);
        Serial.print(" accepted_mismatch=");
        Serial.print(_sequenceTest.parityAcceptedMismatch);
        Serial.print(" type_mismatch=");
        Serial.print(_sequenceTest.parityTypeMismatch);
        Serial.print(" locality_mismatch=");
        Serial.print(_sequenceTest.parityLocalityMismatch);
        Serial.print(" source_mismatch=");
        Serial.print(_sequenceTest.paritySourceMismatch);
        Serial.print(" reason_mismatch=");
        Serial.print(_sequenceTest.parityReasonMismatch);
        Serial.print(" timing_mismatch=");
        Serial.print(_sequenceTest.parityTimingMismatch);
        Serial.print(" confidence_mismatch=");
        Serial.println(_sequenceTest.parityConfidenceMismatch);
    }
    if (_sequenceTest.showDetails) {
        printDetectionParameters();
    }
    printAudioSourceSummary();
    printSignalSummary();
}

void AnalyzerApp::printSequenceFinalOutput() const {
    if (_valMode) {
        return;
    }
    printSequenceSummary();
}

void AnalyzerApp::printBaseSummary() const {
    const unsigned long samples = _baseSession.samples;
    const unsigned long rawAvg = samples > 0 ? _baseSession.rawSum / samples : 0;
    const float deltaAvg = samples > 0 ? _baseSession.deltaSum / static_cast<float>(samples) : 0.0f;
    const float baselineAvg = samples > 0 ? _baseSession.baselineSum / static_cast<float>(samples) : 0.0f;
    const float baselineDrift = _baseSession.baselineMax - _baseSession.baselineMin;
    const int rawSwing = _baseSession.rawMax - _baseSession.rawMin;
    const float deltaSwing = _baseSession.deltaMax - _baseSession.deltaMin;
    const float deltaPeak = _baseSession.deltaMax >= 0.0f ? _baseSession.deltaMax : -_baseSession.deltaMax;
    const float deltaFloor = _baseSession.deltaMin <= 0.0f ? -_baseSession.deltaMin : _baseSession.deltaMin;
    const float deltaQuietPeak = deltaPeak > deltaFloor ? deltaPeak : deltaFloor;

    Serial.print("BASE done: samples=");
    Serial.print(samples);
    if (_baseSession.ignoredRawSamples > 0) {
        Serial.print(" ignored_raw=");
        Serial.print(_baseSession.ignoredRawSamples);
    }
    Serial.print(" rawSample_avg=");
    Serial.print(rawAvg);
    Serial.print(" rawSample_min=");
    Serial.print(_baseSession.rawMin);
    Serial.print(" rawSample_max=");
    Serial.print(_baseSession.rawMax);
    Serial.print(" rawSample_swing=");
    Serial.print(rawSwing);
    Serial.print(" centeredSample_avg=");
    Serial.print(deltaAvg, 1);
    Serial.print(" centeredSample_min=");
    Serial.print(_baseSession.deltaMin, 1);
    Serial.print(" centeredSample_max=");
    Serial.print(_baseSession.deltaMax, 1);
    Serial.print(" centeredSample_swing=");
    Serial.print(deltaSwing, 1);
    Serial.print(" baseline_avg=");
    Serial.print(baselineAvg, 1);
    Serial.print(" baseline_min=");
    Serial.print(_baseSession.baselineMin, 1);
    Serial.print(" baseline_max=");
    Serial.print(_baseSession.baselineMax, 1);
    Serial.print(" baseline_drift=");
    Serial.println(baselineDrift, 1);
    Serial.print("BASE quiet: quiet_rawSample_min=");
    Serial.print(_baseSession.rawMin);
    Serial.print(" quiet_rawSample_max=");
    Serial.print(_baseSession.rawMax);
    Serial.print(" quiet_rawSample_swing=");
    Serial.print(rawSwing);
    Serial.print(" quiet_centeredSample_min=");
    Serial.print(_baseSession.deltaMin, 1);
    Serial.print(" quiet_centeredSample_max=");
    Serial.print(_baseSession.deltaMax, 1);
    Serial.print(" quiet_centeredSample_swing=");
    Serial.print(deltaSwing, 1);
    Serial.print(" quiet_centeredSample_peak=");
    Serial.println(deltaQuietPeak, 1);
    printBaseHints();
    printAudioSourceSummary();
    printSignalSummary();
}

void AnalyzerApp::printBaseHints() const {
    const float quietDeltaPeak = _baseSession.deltaMax >= 0.0f ? _baseSession.deltaMax : -_baseSession.deltaMax;
    const float quietDeltaFloor = _baseSession.deltaMin <= 0.0f ? -_baseSession.deltaMin : _baseSession.deltaMin;
    const float quietNoisePeak = quietDeltaPeak > quietDeltaFloor ? quietDeltaPeak : quietDeltaFloor;
    const unsigned long suggestedMinStrength = static_cast<unsigned long>(quietNoisePeak) + 6;
    const unsigned long suggestedAttack = static_cast<unsigned long>(quietNoisePeak) + 10;
    const unsigned long suggestedRelease = suggestedAttack > 6 ? suggestedAttack - 6 : suggestedAttack;

    Serial.print("BASE hints: quiet_rawSample_peak=");
    Serial.print(_baseSession.rawMax);
    Serial.print(" quiet_centeredSample_max=");
    Serial.print(_baseSession.deltaMax, 1);
    Serial.print(" quiet_centeredSample_peak=");
    Serial.print(quietNoisePeak, 1);
    Serial.print(" suggested_minStrength=");
    Serial.print(suggestedMinStrength);
    Serial.print(" suggested_attack=");
    Serial.print(suggestedAttack);
    Serial.print(" suggested_release=");
    Serial.println(suggestedRelease);
}

void AnalyzerApp::printCaptureSummary() const {
    const unsigned long completed = _captureSession.completed;
    const float avgRawSwing = completed > 0 ? static_cast<float>(_captureSession.totalRawSwing) / static_cast<float>(completed) : 0.0f;
    const float avgDeltaSwing = completed > 0 ? _captureSession.totalDeltaSwing / static_cast<float>(completed) : 0.0f;
    const unsigned long quietRawAvg = _captureSession.quietRawSamples > 0 ? _captureSession.quietRawSum / _captureSession.quietRawSamples : 0;
    const float quietDeltaAvg = _captureSession.quietDeltaSamples > 0 ? _captureSession.quietDeltaSum / static_cast<float>(_captureSession.quietDeltaSamples) : 0.0f;

    Serial.print("CAP done: tries=");
    Serial.print(_captureSession.totalTrials);
    Serial.print(" completed=");
    Serial.print(completed);
    Serial.print(" avg_rawSample_swing=");
    Serial.print(avgRawSwing, 1);
    Serial.print(" avg_centeredSample_swing=");
    Serial.print(avgDeltaSwing, 1);
    Serial.print(" best_rawSample_swing=");
    Serial.print(_captureSession.bestRawSwing);
    Serial.print(" best_centeredSample_swing=");
    Serial.println(_captureSession.bestDeltaSwing, 1);
    Serial.print("CAP quiet: rawSample_avg=");
    Serial.print(quietRawAvg);
    Serial.print(" rawSample_peak=");
    Serial.print(_captureSession.quietRawMax);
    Serial.print(" centeredSample_avg=");
    Serial.print(quietDeltaAvg, 1);
    Serial.print(" centeredSample_peak=");
    Serial.println(_captureSession.quietDeltaMax, 1);
    printCaptureHints();
    printAudioSourceSummary();
    printSignalSummary();
}

void AnalyzerApp::printCaptureHints() const {
    const unsigned long quietRawAvg = _captureSession.quietRawSamples > 0 ? _captureSession.quietRawSum / _captureSession.quietRawSamples : 0;
    const float quietDeltaPeak = _captureSession.quietDeltaMax >= 0.0f ? _captureSession.quietDeltaMax : -_captureSession.quietDeltaMax;
    const float quietDeltaFloor = _captureSession.quietDeltaMin <= 0.0f ? -_captureSession.quietDeltaMin : _captureSession.quietDeltaMin;
    const float quietNoisePeak = quietDeltaPeak > quietDeltaFloor ? quietDeltaPeak : quietDeltaFloor;
    const unsigned long suggestedMinStrength = static_cast<unsigned long>(quietNoisePeak) + 6;
    const unsigned long suggestedAttack = static_cast<unsigned long>(quietNoisePeak) + 10;
    const unsigned long suggestedRelease = suggestedAttack > 6 ? suggestedAttack - 6 : suggestedAttack;

    Serial.print("CAP hints: suggested_minStrength=");
    Serial.print(suggestedMinStrength);
    Serial.print(" suggested_attack=");
    Serial.print(suggestedAttack);
    Serial.print(" suggested_release=");
    Serial.print(suggestedRelease);
    Serial.print(" quiet_rawSample_avg=");
    Serial.print(quietRawAvg);
    Serial.print(" quiet_centeredSample_peak=");
    Serial.println(quietNoisePeak, 1);
}

void AnalyzerApp::printAudioSourceSummary() const {
    const AudioSourceStats& stats = _audioSource.stats();
    Serial.println("AUDIO summary:");
    Serial.print("reads=");
    Serial.print(stats.reads);
    Serial.print(" readBytes=");
    Serial.print(stats.readBytes);
    Serial.print(" zeroReads=");
    Serial.print(stats.zeroReads);
    Serial.print(" shortReads=");
    Serial.print(stats.shortReads);
    Serial.print(" maxReadBytes=");
    Serial.print(stats.maxReadBytes);
    Serial.print(" noSampleLoops=");
    Serial.print(stats.noSampleLoops);
    Serial.print(" readErrors=");
    Serial.print(stats.readErrors);
    Serial.print(" overflow=");
    Serial.print(stats.overflowCount);
    Serial.print(" droppedBlocks=");
    Serial.print(stats.droppedBlockCount);
    Serial.print(" totalSamples=");
    Serial.println(static_cast<unsigned long long>(stats.totalSamplesRead));
}

void AnalyzerApp::printSignalSummary() const {
    const AudioSignalStats& stats = _audioSignal.stats();
    Serial.println("SIGNAL summary:");
    Serial.print("blocks=");
    Serial.print(stats.blocksProcessed);
    Serial.print(" samples=");
    Serial.print(static_cast<unsigned long long>(stats.samplesProcessed));
    Serial.print(" lastBlockStart=");
    Serial.print(static_cast<unsigned long long>(_audioSignal.lastBlockStartSample()));
    Serial.print(" lastBlockCount=");
    Serial.print(_audioSignal.lastBlockSampleCount());
    Serial.print(" lastBlockMicros=");
    Serial.println(_audioSignal.lastBlockApproxStartMicros());
}

void AnalyzerApp::printValueFrame(unsigned long now) const {
    if (_lastPrintMs != 0 && now - _lastPrintMs < kPrintIntervalMs) {
        return;
    }

    _lastPrintMs = now;
    const bool onsetVisible = detectorOnsetDetected() || now < _valOnsetLatchedUntilMs;
    const bool transientVisible = detectorTransientDetected() || now < _valTransientLatchedUntilMs;

    // Compact frame: source sample, centered sample, detector level, and smoothing.
    Serial.print("rawSample:");
    Serial.print(_audioSignal.rawSignal());
    Serial.print('\t');
    Serial.print("baseline:");
    Serial.print(static_cast<int>(_audioSignal.baseline()));
    Serial.print('\t');
    Serial.print("centeredSample:");
    Serial.print(_audioSignal.centeredSignal());
    Serial.print('\t');
    Serial.print("signalLevel:");
    Serial.print(_audioSignal.signalMagnitude());
    Serial.print('\t');
    Serial.print("smoothedLevel:");
    Serial.print(static_cast<int>(_audioSignal.smoothedSignalMagnitude()));
    Serial.print('\t');
    Serial.print("onset:");
    Serial.print(onsetVisible ? 1 : 0);
    Serial.print('\t');
    Serial.print("transient:");
    Serial.println(transientVisible ? 1 : 0);
}
