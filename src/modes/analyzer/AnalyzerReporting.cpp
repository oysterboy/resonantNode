#include "AnalyzerApp.h"

#include <Arduino.h>

namespace {

const char* analyzerProfileDetailNamespace(detection::DetectionProfileKind profileKind) {
    switch (profileKind) {
        case detection::DetectionProfileKind::Chirp:
            return "chirp";
        case detection::DetectionProfileKind::FreqAmp:
        default:
            return "freq_amp";
    }
}

const char* analyzerProfileDetailSummary(detection::DetectionProfileKind profileKind) {
    switch (profileKind) {
        case detection::DetectionProfileKind::Chirp:
            return "chirp profile view";
        case detection::DetectionProfileKind::FreqAmp:
        default:
            return "generic freq-amp profile view";
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

const char* ampSupportName(detection::AmpSupportLevel value) {
    switch (value) {
        case detection::AmpSupportLevel::None:
            return "none";
        case detection::AmpSupportLevel::Weak:
            return "weak";
        case detection::AmpSupportLevel::Medium:
            return "medium";
        case detection::AmpSupportLevel::Strong:
            return "strong";
        case detection::AmpSupportLevel::Unknown:
        default:
            return "unknown";
    }
}

bool analyzerLogEnabled(uint32_t flags, AnalyzerApp::AnalyzerLogFlags flag) {
    return (flags & static_cast<uint32_t>(flag)) != 0;
}

const char* sequenceTrialDurationClass(long durMs) {
    if (durMs < 0) {
        return "invalid";
    }
    if (durMs < 80L) {
        return "short";
    }
    if (durMs <= 180L) {
        return "clean";
    }
    if (durMs <= 240L) {
        return "smeared";
    }
    return "too_long";
}

size_t analyzerReasonIndex(AnalyzerReason value) {
    return static_cast<size_t>(value);
}

} // namespace

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
    Serial.print(" profile=");
    Serial.print(report.context.profile != nullptr ? report.context.profile : "unknown");
    Serial.print(" emitters=");
    Serial.print(report.profileDetail.emitter != nullptr ? report.profileDetail.emitter : "unknown");
    Serial.print(" ampSupport=");
    Serial.print(report.profileDetail.ampSupport != nullptr ? report.profileDetail.ampSupport : "unknown");
    Serial.print(" ampSupportMin=");
    Serial.print(report.profileDetail.ampSupportMin != nullptr ? report.profileDetail.ampSupportMin : "unknown");
    Serial.print(" freqScoreMin=");
    Serial.print(report.profileDetail.freqScoreMin, 1);
    Serial.print(" freqContrastMin=");
    Serial.print(report.profileDetail.freqContrastMin, 2);
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
    Serial.print(" pattern_result=");
    Serial.print(report.primaryPattern.accepted ? "accepted" : "rejected");
    Serial.print(" candidateAccepted=");
    Serial.print(report.primaryPattern.candidateAccepted ? 1 : 0);
    Serial.print(" patternMatched=");
    Serial.print(report.primaryPattern.patternMatched ? 1 : 0);
    Serial.print(" supportMatched=");
    Serial.print(report.primaryPattern.supportMatched ? 1 : 0);
    Serial.print(" behaviorEligible=");
    Serial.print(report.primaryPattern.behaviorEligible ? 1 : 0);
    Serial.print(" profile=");
    Serial.print(report.context.profile != nullptr ? report.context.profile : "unknown");
    Serial.print(" confidence=");
    Serial.print(report.primaryPattern.confidence, 2);
    Serial.print(" support=");
    Serial.print(report.primaryPattern.ampSupport != nullptr ? report.primaryPattern.ampSupport : "unknown");
    Serial.print(" reject_reason=");
    Serial.print(report.primaryPattern.rejectReason != nullptr ? report.primaryPattern.rejectReason : "none");
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
    Serial.print(" profile=");
    Serial.print(report.context.profile != nullptr ? report.context.profile : "unknown");
    Serial.print(" emitters=");
    Serial.print(report.profileDetail.emitter != nullptr ? report.profileDetail.emitter : "unknown");
    Serial.print(" ampSupport=");
    Serial.print(report.profileDetail.ampSupport != nullptr ? report.profileDetail.ampSupport : "unknown");
    Serial.print(" ampSupportMin=");
    Serial.print(report.profileDetail.ampSupportMin != nullptr ? report.profileDetail.ampSupportMin : "unknown");
    Serial.print(" freqScoreMin=");
    Serial.print(report.profileDetail.freqScoreMin, 1);
    Serial.print(" freqContrastMin=");
    Serial.print(report.profileDetail.freqContrastMin, 2);
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
    Serial.print(" confidence=");
    Serial.print(report.primaryPattern.confidence, 2);
    Serial.print(" amp_support=");
    Serial.print(report.primaryPattern.ampSupport != nullptr ? report.primaryPattern.ampSupport : "unknown");
    Serial.print(" reason=");
    Serial.print(analyzerReasonName(report.classification.reason));
    Serial.println();

    Serial.print("SEQ_EXPLAIN_GATES candidateAccepted=");
    Serial.print(report.primaryPattern.candidateAccepted ? 1 : 0);
    Serial.print(" patternMatched=");
    Serial.print(report.primaryPattern.patternMatched ? 1 : 0);
    Serial.print(" supportMatched=");
    Serial.print(report.primaryPattern.supportMatched ? 1 : 0);
    Serial.print(" behaviorEligible=");
    Serial.print(report.primaryPattern.behaviorEligible ? 1 : 0);
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
    Serial.print(" support_win=");
    Serial.print(static_cast<long>(report.profileDetail.ampWindow.windowStartMs));
    Serial.print("..");
    Serial.print(static_cast<long>(report.profileDetail.ampWindow.windowEndMs));
    Serial.print("ms available=");
    Serial.println(report.profileDetail.ampWindow.available ? 1 : 0);

    Serial.print("SEQ_EXPLAIN_FIELD state=");
    Serial.print(report.field.state != nullptr ? report.field.state : "unknown");
    Serial.print(" rawActivity=");
    Serial.print(report.field.rawActivity, 2);
    Serial.print(" validPatternActivity=");
    Serial.print(report.field.validPatternActivity, 2);
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
        Serial.print(" pattern_result=");
        Serial.print(report.primaryPattern.accepted ? "accepted" : "rejected");
        Serial.print(" accepted=");
        Serial.print(report.primaryPattern.accepted ? 1 : 0);
        Serial.print(" dt=");
        printMs(report.primaryPattern.dtMs);
        Serial.print(" confidence=");
        Serial.print(report.primaryPattern.confidence, 2);
        Serial.print(" support=");
        Serial.print(report.primaryPattern.ampSupport != nullptr ? report.primaryPattern.ampSupport : "unknown");
        Serial.print(" reason=");
        Serial.print(report.primaryPattern.reason != nullptr ? report.primaryPattern.reason : "none");
        Serial.print(" reject_reason=");
        Serial.print(report.primaryPattern.rejectReason != nullptr ? report.primaryPattern.rejectReason : "none");
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
        Serial.print(" support=");
        Serial.print(report.profileDetail.ampSupport != nullptr ? report.profileDetail.ampSupport : "unknown");
        Serial.println();

        Serial.print("SEQ_EXPLAIN_PIPELINE_SOURCE source=");
        Serial.print(report.debug.pipelineSource != nullptr ? report.debug.pipelineSource : "unknown");
        Serial.print(" fallback=");
        Serial.print(report.debug.pipelineFallback ? 1 : 0);
        Serial.println();

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
    Serial.print(" pattern_result=");
    Serial.print(report.primaryPattern.accepted ? "accepted" : "rejected");
    Serial.print(" pattern_type=");
    Serial.print(report.primaryPattern.type != nullptr ? report.primaryPattern.type : "none");
    Serial.print(" reject_reason=");
    Serial.print(report.primaryPattern.rejectReason != nullptr ? report.primaryPattern.rejectReason : "none");
    Serial.print(" amp_support=");
    Serial.print(report.profileDetail.ampWindow.supportClass != nullptr ? report.profileDetail.ampWindow.supportClass : "unknown");
    Serial.print(" peak=");
    Serial.print(report.profileDetail.ampWindow.peak, 1);
    Serial.print(" support_win=");
    Serial.print(static_cast<long>(report.profileDetail.ampWindow.windowStartMs));
    Serial.print("..");
    Serial.print(static_cast<long>(report.profileDetail.ampWindow.windowEndMs));
    Serial.print("ms available=");
    Serial.print(report.profileDetail.ampWindow.available ? 1 : 0);
    Serial.println();
}

void AnalyzerApp::printSequenceTrialResult(unsigned long trialNumber, AnalyzerResult result, long dtMs, long durMs, float strength, bool audioOverflow, unsigned long duplicateCount, const SequenceTest::TrialDiagnostics& diagnostics) const {
    if (_valMode) {
        return;
    }
    if (_sequenceTest.logFlags == AnalyzerApp::ANALYZER_LOG_CUSTOM) {
        return;
    }
    if (!analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_TRIAL)) {
        return;
    }

    const detection::PatternResult* runtimePatternResult = diagnostics.runtimePatternCaptured ? &diagnostics.runtimePatternResult : nullptr;
    const auto buildTrialSignal = [&]() {
        if (!diagnostics.patternAccepted) {
            detection::SignalCandidate signal = {};
            if (runtimePatternResult == nullptr) {
                return signal;
            }
            signal.kind = detection::SignalKind::FrequencyMatch;
            signal.source = detection::SignalSource::Frequency;
            signal.detectorKind = detection::SignalDetectorKind::FrequencyMatch;
            signal.present = true;
            signal.valid = runtimePatternResult->valid;
            signal.startSample = runtimePatternResult->candidate.onsetSample;
            signal.peakSample = runtimePatternResult->candidate.peakSample;
            signal.releaseSample = runtimePatternResult->candidate.releaseSample;
            signal.startMs = runtimePatternResult->candidate.startMs;
            signal.peakMs = runtimePatternResult->candidate.acceptedMs != 0
                ? runtimePatternResult->candidate.acceptedMs
                : runtimePatternResult->candidate.startMs;
            signal.releaseMs = runtimePatternResult->candidate.durationMs > 0
                ? signal.startMs + runtimePatternResult->candidate.durationMs
                : signal.peakMs;
            signal.endMs = signal.releaseMs;
            signal.durationMs = runtimePatternResult->candidate.durationMs;
            signal.strength = runtimePatternResult->candidate.peakStrength;
            signal.score = runtimePatternResult->freq.score;
            signal.contrast = runtimePatternResult->freq.spectralContrast;
            signal.confidence = runtimePatternResult->confidence;
            signal.signalConfidence = signal.confidence;
            signal.frequencyConfidence = signal.confidence;
            signal.ampEvidencePresent = true;
            signal.frequency = runtimePatternResult->freq;
            return signal;
        }

        detection::SignalCandidate signal = {};
        signal.kind = detection::SignalKind::FrequencyMatch;
        signal.source = detection::SignalSource::Frequency;
        signal.detectorKind = detection::SignalDetectorKind::FrequencyMatch;
        signal.present = true;
        signal.valid = true;
        signal.startSample = diagnostics.acceptedPatternOnsetSample;
        signal.peakSample = diagnostics.acceptedPatternPeakSample;
        signal.releaseSample = diagnostics.acceptedPatternReleaseSample;
        signal.startMs = diagnostics.acceptedPatternMs;
        signal.peakMs = diagnostics.acceptedPatternPeakMs != 0 ? diagnostics.acceptedPatternPeakMs : diagnostics.acceptedPatternMs;
        signal.releaseMs = diagnostics.acceptedPatternReleaseMs != 0 ? diagnostics.acceptedPatternReleaseMs : signal.peakMs;
        signal.endMs = signal.releaseMs;
        signal.durationMs = diagnostics.acceptedPatternDurationMs;
        signal.strength = diagnostics.acceptedPatternStrength;
        signal.score = diagnostics.acceptedFrequencyEvidence.present ? diagnostics.acceptedFrequencyEvidence.score : diagnostics.acceptedPatternStrength;
        signal.contrast = diagnostics.acceptedFrequencyEvidence.present ? diagnostics.acceptedFrequencyEvidence.spectralContrast : 0.0f;
        signal.confidence = diagnostics.acceptedFrequencyEvidence.present && diagnostics.acceptedFrequencyEvidence.matched ? 1.0f : 0.0f;
        signal.signalConfidence = signal.confidence;
        signal.frequencyConfidence = signal.confidence;
        signal.ampLevel = diagnostics.acceptedPatternStrength;
        signal.ampBaseline = diagnostics.acceptedAmbientBaseline;
        signal.ampEvidencePresent = true;
        signal.frequency = diagnostics.acceptedFrequencyEvidence;
        signal.frequency.present = true;
        signal.frequency.observedAtMs = diagnostics.acceptedFrequencyProcessedAtMs;
        return signal;
    };

    const detection::SignalCandidate trialSignal = buildTrialSignal();
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

    const bool summaryTrial = analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_TRIAL_SUMMARY);
    const float freqScore = runtimePatternResult != nullptr ? runtimePatternResult->freq.score : (diagnostics.patternAccepted ? diagnostics.acceptedPatternStrength : trialSignal.score);
    const float freqContrast = runtimePatternResult != nullptr ? runtimePatternResult->freq.spectralContrast : (diagnostics.patternAccepted ? diagnostics.acceptedFrequencyEvidence.spectralContrast : trialSignal.contrast);
    const char* ampSupport = runtimePatternResult != nullptr ? ampSupportName(runtimePatternResult->ampSupport) : "Unknown";
    const float ampPeak = runtimePatternResult != nullptr ? runtimePatternResult->candidate.peakStrength : trialSignal.ampLevel;
    const float ampBaseline = runtimePatternResult != nullptr ? runtimePatternResult->candidate.ambientBaseline : trialSignal.ampBaseline;
    const float ampLift = ampPeak - ampBaseline;

    Serial.println();
    Serial.print("SEQ_TRIAL trial=");
    Serial.print(trialNumber);
    if (summaryTrial) {
        const bool accepted = result == AnalyzerResult::Expected || result == AnalyzerResult::Late;
        Serial.print(" accept=");
        Serial.print(accepted ? 1 : 0);
    } else {
        Serial.print(" result=");
        Serial.print(analyzerResultName(result));
        Serial.print(" dt=");
        if (dtMs >= 0) {
            Serial.print(dtMs);
            Serial.print("ms");
        } else {
            Serial.print("-1ms");
        }
    }
    Serial.print(" dur=");
    if (durMs >= 0) {
        Serial.print(durMs);
        Serial.print("ms");
    } else {
        Serial.print("-1ms");
    }
    if (!summaryTrial) {
        Serial.print(" dur_class=");
        Serial.print(sequenceTrialDurationClass(durMs));
        Serial.print(" strength=");
        Serial.print(strength, 1);
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

    (void)audioOverflow;
    (void)diagnostics;
}

void AnalyzerApp::printDetectionParameters() const {
    if (_valMode) {
        return;
    }
    Serial.print("SEQ tuning freqScore=");
    Serial.print(_frequencyEvidenceTuning.scoreMin, 1);
    Serial.print(" freqContrast=");
    Serial.print(_frequencyEvidenceTuning.contrastMin, 1);
    Serial.print(" transientDetector=fixed");
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
    const detection::AmpDiagnosticSnapshot probeSnapshot = _ampTransientDiagnosticProbe.snapshot();
    const unsigned long expectedCount = (elapsedMs + (probeSnapshot.cooldownAfterOnsetMs / 2)) / probeSnapshot.cooldownAfterOnsetMs;
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

    AnalyzerSummary summary = {};
    summary.profileName = activeAnalyzerProfileName();
    summary.trials = _sequenceTest.totalTrials;

    const size_t reasonCount = static_cast<size_t>(AnalyzerReason::Unknown) + 1U;
    unsigned int missReasonCounts[reasonCount] = {};
    unsigned int rejectReasonCounts[reasonCount] = {};
    unsigned long dtSumMs = 0;
    unsigned long durSumMs = 0;
    unsigned long completedTrialCount = 0;
    size_t dtCount = 0;
    size_t durCount = 0;
    float confidenceSum = 0.0f;

    const bool capturedSummaryAvailable = _sequenceTest.trialReports != nullptr;
    if (capturedSummaryAvailable) {
        completedTrialCount = static_cast<unsigned long>(_sequenceTest.trialReportCount);
        for (size_t i = 0; i < _sequenceTest.trialReportCount; ++i) {
            const AnalyzerReport& report = _sequenceTest.trialReports[i];
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
                case AnalyzerResult::Duplicate:
                    ++summary.duplicate;
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
        Serial.print("SEQ_PROFILE_SUMMARY pattern_matched_expected=");
        Serial.print(_sequenceTest.patternMatchedExpected);
        Serial.print(" pattern_unmatched_expected=");
        Serial.print(_sequenceTest.patternUnmatchedExpected);
        Serial.print(" pattern_matched_duplicates=");
        Serial.print(_sequenceTest.patternMatchedDuplicates);
        Serial.print(" pattern_unmatched_duplicates=");
        Serial.print(_sequenceTest.patternUnmatchedDuplicates);
        Serial.print(" pattern_matched_unexpected=");
        Serial.print(_sequenceTest.patternMatchedUnexpected);
        Serial.print(" pattern_unmatched_unexpected=");
        Serial.print(_sequenceTest.patternUnmatchedUnexpected);
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
    const detection::AmpDiagnosticSnapshot probeSnapshot = _ampTransientDiagnosticProbe.snapshot();
    const bool onsetVisible = probeSnapshot.onsetVisible || now < _valOnsetLatchedUntilMs;
    const bool transientVisible = probeSnapshot.transientVisible || now < _valTransientLatchedUntilMs;

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
