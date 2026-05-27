#include "AnalyzerApp.h"

#include <Arduino.h>

#include "../../TimingUtils.h"

namespace {

const char* analyzerProfileDetailNamespace(detection::DetectionProfileKind profileKind) {
    switch (profileKind) {
        case detection::DetectionProfileKind::Amp:
            return "amp";
        case detection::DetectionProfileKind::TonalPulse2:
            return "tonal_pulse_2";
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
        case detection::DetectionProfileKind::TonalPulse2:
            return "tonal_pulse_2 profile view";
        case detection::DetectionProfileKind::ChirpExperimental:
            return "chirp_experimental profile view";
        case detection::DetectionProfileKind::TonalPulse:
        default:
            return "generic tonal pulse profile view";
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
    Serial.print("SEQ_TRIAL #");
    Serial.print(report.context.trial);
    Serial.print(" profile=");
    Serial.print(report.context.profile != nullptr ? report.context.profile : "unknown");
    Serial.print(" source=");
    Serial.print(report.profileDetail.emitter != nullptr ? report.profileDetail.emitter : "unknown");
    Serial.print(" required_support_target=");
    Serial.print(report.profileDetail.requiredSupportTarget != nullptr ? report.profileDetail.requiredSupportTarget : "unknown");
    Serial.print(" support_gate=");
    Serial.print(report.profileDetail.ampStrength != nullptr ? report.profileDetail.ampStrength : "unknown");
    Serial.println();

    Serial.println("results:");
    Serial.print("A_result=");
    Serial.print(analyzerResultName(report.classification.result));
    Serial.print(" A_dt=");
    if (report.classification.dtMs >= 0) {
        Serial.print(report.classification.dtMs);
        Serial.print("ms");
    } else {
        Serial.print("-1ms");
    }
    Serial.print(" confidence=");
    Serial.print(report.primaryPattern.confidence, 2);
    Serial.println();
    Serial.print("pattern=");
    Serial.print(report.primaryPattern.type != nullptr ? report.primaryPattern.type : "unknown");
    Serial.print(" A_reason=");
    Serial.print(analyzerReasonName(report.classification.reason));
    Serial.print(" valid=");
    Serial.print(report.primaryPattern.accepted ? 1 : 0);
    Serial.print(" candidate_accepted=");
    Serial.print(report.primaryPattern.candidateAccepted ? 1 : 0);
    Serial.print(" pattern_matched=");
    Serial.print(report.primaryPattern.patternMatched ? 1 : 0);
    Serial.print(" support_matched=");
    Serial.print(report.primaryPattern.supportMatched ? 1 : 0);
    Serial.print(" behavior_eligible=");
    Serial.print(report.primaryPattern.behaviorEligible ? 1 : 0);
    Serial.print(" support=");
    Serial.print(report.primaryPattern.ampStrength != nullptr ? report.primaryPattern.ampStrength : "unknown");
    Serial.print(" reject_reason=");
    Serial.println(report.primaryPattern.rejectReason != nullptr ? report.primaryPattern.rejectReason : "none");

    Serial.println("measurements:");
    Serial.print("detector_strength=");
    Serial.print(report.occurrences.strength, 1);
    Serial.print(" inspector_strength=");
    Serial.print(report.inspection.moduleStrengthClass != nullptr ? report.inspection.moduleStrengthClass : "unknown");
    Serial.print(" inspector_evidence=");
    Serial.println(report.inspection.primaryEvidence != nullptr ? report.inspection.primaryEvidence : "none");
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
    const auto printModuleList = [](const char* modules) {
        if (modules == nullptr || modules[0] == '\0') {
            Serial.println("none");
            return;
        }
        const char* cursor = modules;
        while (*cursor != '\0') {
            const char* start = cursor;
            while (*cursor != '\0' && *cursor != ',') {
                ++cursor;
            }
            while (start < cursor && (*start == ' ' || *start == '\t')) {
                ++start;
            }
            const char* end = cursor;
            while (end > start && (*(end - 1) == ' ' || *(end - 1) == '\t')) {
                --end;
            }
            Serial.print("  - ");
            for (const char* p = start; p < end; ++p) {
                Serial.print(*p);
            }
            Serial.println();
            if (*cursor == ',') {
                ++cursor;
            }
        }
    };

    Serial.print("SEQ_EXPLAIN #");
    Serial.println(report.context.trial);
    Serial.println("1 pattern:");
    Serial.print("A_result=");
    Serial.print(analyzerResultName(report.classification.result));
    Serial.print(" A_dt=");
    printMs(report.classification.dtMs);
    Serial.print(" confidence=");
    Serial.print(report.primaryPattern.confidence, 2);
    Serial.println();
    Serial.print("pattern=");
    Serial.print(report.primaryPattern.type != nullptr ? report.primaryPattern.type : "unknown");
    Serial.print(" A_reason=");
    Serial.print(analyzerReasonName(report.classification.reason));
    Serial.print(" valid=");
    Serial.print(report.primaryPattern.accepted ? 1 : 0);
    Serial.print(" candidate_accepted=");
    Serial.print(report.primaryPattern.candidateAccepted ? 1 : 0);
    Serial.print(" pattern_matched=");
    Serial.print(report.primaryPattern.patternMatched ? 1 : 0);
    Serial.print(" support_matched=");
    Serial.print(report.primaryPattern.supportMatched ? 1 : 0);
    Serial.print(" behavior_eligible=");
    Serial.println(report.primaryPattern.behaviorEligible ? 1 : 0);

    Serial.println("2 detector:");
    Serial.print("kind=");
    Serial.print(report.occurrences.kind != nullptr ? report.occurrences.kind : "none");
    Serial.print(" source=");
    Serial.print(report.occurrences.primarySource != nullptr ? report.occurrences.primarySource : "none");
    Serial.print(" detector=");
    Serial.print(report.occurrences.detectorKind != nullptr ? report.occurrences.detectorKind : "unknown");
    Serial.print(" present=");
    Serial.print(report.occurrences.present ? 1 : 0);
    Serial.print(" valid=");
    Serial.print(report.occurrences.valid ? 1 : 0);
    Serial.print(" start_ms=");
    printUnsignedMs(report.occurrences.startMs);
    Serial.print(" peak_ms=");
    printUnsignedMs(report.occurrences.peakMs);
    Serial.print(" release_ms=");
    printUnsignedMs(report.occurrences.releaseMs);
    Serial.print(" dt=");
    printMs(report.occurrences.primaryDtMs);
    Serial.print(" dur=");
    printUnsignedMs(report.occurrences.primaryDurationMs);
    Serial.print(" strength=");
    Serial.print(report.occurrences.strength, 1);
    Serial.print(" confidence=");
    Serial.print(report.occurrences.confidence, 2);
    Serial.print(" reject_reason=");
    Serial.println(report.occurrences.rejectReason != nullptr ? report.occurrences.rejectReason : "none");

    Serial.println("3 inspectors:");
    Serial.print("module_results: module_target=");
    Serial.print(report.inspection.moduleTarget != nullptr ? report.inspection.moduleTarget : "unknown");
    Serial.print(" module_strength_class=");
    Serial.print(report.inspection.moduleStrengthClass != nullptr ? report.inspection.moduleStrengthClass : "unknown");
    Serial.print(" evidence=");
    Serial.print(report.inspection.primaryEvidence != nullptr ? report.inspection.primaryEvidence : "none");
    Serial.print(" inspected=");
    Serial.print(report.inspection.inspected);
    Serial.print(" accepted=");
    Serial.print(report.inspection.accepted);
    Serial.print(" rejected=");
    Serial.print(report.inspection.rejected);
    Serial.print(" main_reject=");
    Serial.println(report.inspection.mainRejectReason != nullptr ? report.inspection.mainRejectReason : "none");
    Serial.println("modules:");
    printModuleList(report.profileDetail.inspectionModules);

    Serial.println("4 field:");
    Serial.print("field_state=");
    Serial.print(report.field.state != nullptr ? report.field.state : "unknown");
    Serial.print(" field_rawActivity=");
    Serial.print(report.field.rawActivity, 2);
    Serial.print(" field_validPatternActivity=");
    Serial.print(report.field.validPatternActivity, 2);
    Serial.print(" field_recent_valid=");
    Serial.print(report.field.recentValidPatterns);
    Serial.print(" field_recent_rejects=");
    Serial.println(report.field.recentRejects);

    Serial.println("5 debug:");
    Serial.print("A_debug_occurrences=");
    Serial.print(report.debug.occurrences);
    Serial.print(" A_debug_inspected=");
    Serial.print(report.debug.inspected);
    Serial.print(" A_debug_patterns=");
    Serial.print(report.debug.patterns);
    Serial.print(" A_debug_rejects=");
    Serial.print(report.debug.rejects);
    Serial.print(" A_debug_duplicate_hits=");
    Serial.print(report.debug.duplicates);
    Serial.print(" A_debug_unexpected_hits=");
    Serial.print(report.debug.unexpected);
    Serial.print(" A_debug_main_reject=");
    Serial.println(report.debug.mainRejectReason != nullptr ? report.debug.mainRejectReason : "none");

    if (analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_CUSTOM)) {
        Serial.println("custom_expected:");
        Serial.print("pattern=");
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

        Serial.println("custom_occurrence:");
        Serial.print("A_total=");
        Serial.print(report.occurrences.total);
        Serial.print(" A_accepted=");
        Serial.print(report.occurrences.accepted);
        Serial.print(" A_rejected=");
        Serial.print(report.occurrences.rejected);
        Serial.print(" primary_source=");
        Serial.print(report.occurrences.primarySource != nullptr ? report.occurrences.primarySource : "none");
        Serial.print(" primary_dt=");
        printMs(report.occurrences.primaryDtMs);
        Serial.print(" primary_dur=");
        printUnsignedMs(report.occurrences.primaryDurationMs);
        Serial.print(" primary_strength=");
        Serial.print(report.occurrences.primaryStrength, 1);
        Serial.print(" confidence=");
        Serial.print(report.occurrences.confidence, 2);
        Serial.print(" A_main_reject=");
        Serial.println(report.occurrences.mainRejectReason != nullptr ? report.occurrences.mainRejectReason : "none");

        Serial.println("custom_pattern:");
        Serial.print("valid=");
        Serial.print(report.primaryPattern.accepted ? 1 : 0);
        Serial.print(" dt=");
        printMs(report.primaryPattern.dtMs);
        Serial.print(" confidence=");
        Serial.print(report.primaryPattern.confidence, 2);
        Serial.println();
        Serial.print("pattern=");
        Serial.print(report.primaryPattern.type != nullptr ? report.primaryPattern.type : "unknown");
        Serial.print(" amp_strength=");
        Serial.print(report.primaryPattern.ampStrength != nullptr ? report.primaryPattern.ampStrength : "unknown");
        Serial.print(" reject_reason=");
        Serial.print(report.primaryPattern.rejectReason != nullptr ? report.primaryPattern.rejectReason : "none");
        Serial.print(" occurrences=");
        Serial.println(report.primaryPattern.involvedOccurrences);

        Serial.println("custom_profile:");
        Serial.print("ns=");
        Serial.print(report.profileDetail.namespaceName != nullptr ? report.profileDetail.namespaceName : "none");
        Serial.print(" summary=");
        Serial.print(report.profileDetail.summary != nullptr ? report.profileDetail.summary : "");
        Serial.print(" amp_strength=");
        Serial.print(report.profileDetail.ampStrength != nullptr ? report.profileDetail.ampStrength : "unknown");
        Serial.print(" freq_score=");
        Serial.print(report.profileDetail.freqScore, 1);
        Serial.print(" freq_contrast=");
        Serial.print(report.profileDetail.freqContrast, 2);
        Serial.print(" amp_level=");
        Serial.print(report.profileDetail.ampLevel, 1);
        Serial.print(" amp_base=");
        Serial.print(report.profileDetail.ampBase, 1);
        Serial.print(" amp_lift=");
        Serial.println(report.profileDetail.ampLift, 1);
    }
}

void AnalyzerApp::printSequenceCandidateLogs(unsigned long trialNumber, const SequenceTest::TrialDiagnostics& diagnostics) const {
    if (_valMode) {
        return;
    }
    if (_sequenceTest.logFlags == AnalyzerApp::ANALYZER_LOG_CUSTOM) {
        return;
    }
    if (!analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_CANDIDATE) || _sequenceTest.quiet) {
        return;
    }

    for (unsigned long i = 0; i < diagnostics.candidateCount; ++i) {
        const auto& candidate = diagnostics.candidates[i];
        Serial.print("SEQ_CAND trial=");
        Serial.print(trialNumber);
        Serial.print(" idx=");
        Serial.print(i + 1UL);
        Serial.print(" candidate_class=");
        Serial.print(candidate.candidateClass != nullptr ? candidate.candidateClass : "unknown");
        Serial.print(" onset_ms=");
        Serial.print(candidate.candidateMs);
        Serial.print(" onset_sample=");
        Serial.print(static_cast<unsigned long>(candidate.onsetSample));
        Serial.print(" peak_sample=");
        Serial.print(static_cast<unsigned long>(candidate.peakSample));
        Serial.print(" release_sample=");
        Serial.print(static_cast<unsigned long>(candidate.releaseSample));
        Serial.print(" onset_dt_ms=");
        Serial.print(candidate.dtFromTriggerMs);
        Serial.print(" peak_ms=");
        Serial.print(candidate.peakMs);
        Serial.print(" dur=");
        Serial.print(candidate.durationMs);
        Serial.print(" end_dt_ms=");
        if (candidate.endDtMs >= 0) {
            Serial.print(candidate.endDtMs);
            Serial.print("ms");
        } else {
            Serial.print("-");
        }
        Serial.print(" processed_at_ms=");
        Serial.print(candidate.processedAtMs);
        Serial.print(" process_lag_ms=");
        if (candidate.processLagMs >= 0) {
            Serial.print(candidate.processLagMs);
            Serial.print("ms");
        } else {
            Serial.print("-");
        }
        Serial.print(" strength=");
        Serial.print(candidate.strength, 1);
        Serial.print(" transient_present=");
        Serial.print(candidate.transientPresent ? 1 : 0);
        Serial.print(" freq_present=");
        Serial.print(candidate.freqPresent ? 1 : 0);
        Serial.print(" freq_matched=");
        Serial.print(candidate.freqMatched ? 1 : 0);
        Serial.print(" freq_score=");
        Serial.print(candidate.freqScore, 1);
        Serial.print(" pattern_valid=");
        Serial.print(candidate.patternValid ? 1 : 0);
        Serial.print(" candidateAccepted=");
        Serial.print(candidate.candidateAccepted ? 1 : 0);
        Serial.print(" patternMatched=");
        Serial.print(candidate.patternMatched ? 1 : 0);
        Serial.print(" supportMatched=");
        Serial.print(candidate.supportMatched ? 1 : 0);
        Serial.print(" behaviorEligible=");
        Serial.print(candidate.behaviorEligible ? 1 : 0);
        Serial.print(" reject_reason=");
        Serial.print(candidate.rejectReason != nullptr ? candidate.rejectReason : "none");
        Serial.print(" pattern_type=");
        Serial.print(candidate.patternType != nullptr ? candidate.patternType : "none");
        Serial.print(" reason=");
        Serial.println(candidate.reason != nullptr ? candidate.reason : "none");
    }

    if (diagnostics.candidateOverflowCount > 0) {
        Serial.print("SEQ_CAND_WARN trial=");
        Serial.print(trialNumber);
        Serial.print(" overflow=");
        Serial.println(diagnostics.candidateOverflowCount);
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
    Serial.print("SEQ_AMP_STRENGTH #");
    Serial.print(report.context.trial);
    Serial.print(" dt=");
    printMs(report.primaryPattern.dtMs);
    Serial.print(" pattern=");
    Serial.print(report.primaryPattern.type != nullptr ? report.primaryPattern.type : "none");
    Serial.print(" result=");
    Serial.print(report.primaryPattern.accepted ? "accepted" : "rejected");
    Serial.print(" reject_reason=");
    Serial.print(report.primaryPattern.rejectReason != nullptr ? report.primaryPattern.rejectReason : "none");
    Serial.println();

    Serial.print("  module_target=");
    Serial.print(report.inspection.moduleTarget != nullptr ? report.inspection.moduleTarget : "unknown");
    Serial.print(" module_strength=");
    Serial.print(report.inspection.moduleStrengthClass != nullptr ? report.inspection.moduleStrengthClass : "unknown");
    Serial.print(" peak=");
    Serial.print(report.profileDetail.ampStrengthObservation.peak, 1);
    Serial.print(" window=");
    Serial.print(static_cast<long>(report.profileDetail.ampStrengthObservation.windowStartMs));
    Serial.print("..");
    Serial.print(static_cast<long>(report.profileDetail.ampStrengthObservation.windowEndMs));
    Serial.print("ms available=");
    Serial.println(report.profileDetail.ampStrengthObservation.available ? 1 : 0);
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
    const auto buildTrialOccurrence = [&]() {
        if (!diagnostics.patternAccepted) {
            detection::Occurrence occurrence = {};
            if (runtimePatternResult == nullptr) {
                return occurrence;
            }
            occurrence.kind = detection::OccurrenceKind::FrequencyMatch;
            occurrence.source = detection::OccurrenceSource::Frequency;
            occurrence.detectorKind = detection::OccurrenceDetectorKind::FrequencyMatch;
            occurrence.present = true;
            occurrence.valid = runtimePatternResult->valid;
            occurrence.startSample = runtimePatternResult->candidate.onsetSample;
            occurrence.peakSample = runtimePatternResult->candidate.peakSample;
            occurrence.releaseSample = runtimePatternResult->candidate.releaseSample;
            occurrence.startMs = runtimePatternResult->candidate.startMs;
            occurrence.peakMs = runtimePatternResult->candidate.acceptedMs != 0
                ? runtimePatternResult->candidate.acceptedMs
                : runtimePatternResult->candidate.startMs;
            occurrence.releaseMs = runtimePatternResult->candidate.durationMs > 0
                ? occurrence.startMs + runtimePatternResult->candidate.durationMs
                : occurrence.peakMs;
            occurrence.endMs = occurrence.releaseMs;
            occurrence.durationMs = runtimePatternResult->candidate.durationMs;
            occurrence.strength = runtimePatternResult->candidate.peakStrength;
            occurrence.score = runtimePatternResult->freq.score;
            occurrence.contrast = runtimePatternResult->freq.spectralContrast;
            occurrence.confidence = runtimePatternResult->confidence;
            occurrence.ampEvidencePresent = true;
            occurrence.frequency = runtimePatternResult->freq;
            return occurrence;
        }

        detection::Occurrence occurrence = {};
        occurrence.kind = detection::OccurrenceKind::FrequencyMatch;
        occurrence.source = detection::OccurrenceSource::Frequency;
        occurrence.detectorKind = detection::OccurrenceDetectorKind::FrequencyMatch;
        occurrence.present = true;
        occurrence.valid = true;
        occurrence.startSample = diagnostics.acceptedPatternOnsetSample;
        occurrence.peakSample = diagnostics.acceptedPatternPeakSample;
        occurrence.releaseSample = diagnostics.acceptedPatternReleaseSample;
        occurrence.startMs = diagnostics.acceptedPatternMs;
        occurrence.peakMs = diagnostics.acceptedPatternPeakMs != 0 ? diagnostics.acceptedPatternPeakMs : diagnostics.acceptedPatternMs;
        occurrence.releaseMs = diagnostics.acceptedPatternReleaseMs != 0 ? diagnostics.acceptedPatternReleaseMs : occurrence.peakMs;
        occurrence.endMs = occurrence.releaseMs;
        occurrence.durationMs = diagnostics.acceptedPatternDurationMs;
        occurrence.strength = diagnostics.acceptedPatternStrength;
        occurrence.score = diagnostics.acceptedFrequencyFrame.present ? diagnostics.acceptedFrequencyFrame.score : diagnostics.acceptedPatternStrength;
        occurrence.contrast = diagnostics.acceptedFrequencyFrame.present ? diagnostics.acceptedFrequencyFrame.spectralContrast : 0.0f;
        occurrence.confidence = diagnostics.acceptedFrequencyFrame.present && diagnostics.acceptedFrequencyFrame.matched ? 1.0f : 0.0f;
        occurrence.ampLevel = diagnostics.acceptedPatternStrength;
        occurrence.ampBaseline = diagnostics.acceptedAmbientBaseline;
        occurrence.ampEvidencePresent = true;
        occurrence.frequency = diagnostics.acceptedFrequencyFrame;
        occurrence.frequency.present = true;
        occurrence.frequency.observedAtMs = diagnostics.acceptedFrequencyProcessedAtMs;
        return occurrence;
    };

    const detection::Occurrence trialOccurrence = buildTrialOccurrence();
    const unsigned long probeStartMs = trialOccurrence.startMs > 20UL ? trialOccurrence.startMs - 20UL : 0UL;
    const unsigned long probeEndMs = trialOccurrence.endMs != 0
        ? trialOccurrence.endMs + 20UL
        : (trialOccurrence.releaseMs != 0 ? trialOccurrence.releaseMs + 20UL : trialOccurrence.peakMs + 20UL);
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
    const detection::ScalarWindow ampStrengthWindow = _sequenceFeatureHistory != nullptr
        ? _sequenceFeatureHistory->getWindow(detection::FeatureStreamId::AmpEnvelope, probeStartMs, probeEndMs)
        : detection::ScalarWindow{};
    const detection::ScalarWindow floorWindow = _sequenceFeatureHistory != nullptr
        ? _sequenceFeatureHistory->getWindow(detection::FeatureStreamId::AmbientFloor, probeStartMs, probeEndMs)
        : detection::ScalarWindow{};

    const bool summaryTrial = analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_TRIAL_SUMMARY);
    const float freqScore = runtimePatternResult != nullptr ? runtimePatternResult->freq.score : (diagnostics.patternAccepted ? diagnostics.acceptedPatternStrength : trialOccurrence.score);
    const float freqContrast = runtimePatternResult != nullptr ? runtimePatternResult->freq.spectralContrast : (diagnostics.patternAccepted ? diagnostics.acceptedFrequencyFrame.spectralContrast : trialOccurrence.contrast);
    const char* ampStrength = runtimePatternResult != nullptr ? strengthClassName(runtimePatternResult->ampStrength) : "Unknown";
    const float ampPeak = runtimePatternResult != nullptr ? runtimePatternResult->candidate.peakStrength : trialOccurrence.ampLevel;
    const float ampBaseline = runtimePatternResult != nullptr ? runtimePatternResult->candidate.ambientBaseline : trialOccurrence.ampBaseline;
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
    Serial.print(" amp_strength=");
    Serial.print(ampStrength);
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
    Serial.print(ampStrengthWindow.valid ? 1 : 0);
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
    const unsigned long expectedCount = 0;
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
    const AnalyzerReport* profileReport = _sequenceTest.trialReports != nullptr && _sequenceTest.trialReportCount > 0
        ? &_sequenceTest.trialReports[0]
        : nullptr;

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

            if (report.occurrences.primaryDurationMs > 0) {
                durSumMs += report.occurrences.primaryDurationMs;
                ++durCount;
            }

            confidenceSum += report.primaryPattern.confidence;

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

    Serial.println();
    Serial.print("SEQ_SUMMARY profile=");
    Serial.print(summary.profileName != nullptr ? summary.profileName : "unknown");
    Serial.print(" source=");
    Serial.print(profileReport != nullptr && profileReport->profileDetail.emitter != nullptr ? profileReport->profileDetail.emitter : "unknown");
    Serial.print(" required_support_target=");
    Serial.print(profileReport != nullptr && profileReport->profileDetail.requiredSupportTarget != nullptr ? profileReport->profileDetail.requiredSupportTarget : "unknown");
    Serial.print(" support_gate=");
    Serial.print(profileReport != nullptr && profileReport->profileDetail.ampStrength != nullptr ? profileReport->profileDetail.ampStrength : "unknown");
    Serial.print(" trials=");
    Serial.print(summary.trials);
    Serial.print(" completed=");
    Serial.print(summary.completed);
    Serial.print(" avg_pattern_confidence=");
    Serial.print(summary.avgConfidence, 2);
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
    Serial.println();

    Serial.print("  A_counts: expected=");
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
    Serial.println(summary.invalidAudio);

    Serial.print("  A_rates: duplicate=");
    Serial.print(summary.duplicateRate, 2);
    Serial.print(" unexpected=");
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
        Serial.print("  A_reason_counts: result_expected_window=");
        Serial.print(missReasonCounts[analyzerReasonIndex(AnalyzerReason::ValidPatternInExpectedWindow)]);
        Serial.print(" result_before_window=");
        Serial.print(missReasonCounts[analyzerReasonIndex(AnalyzerReason::ValidPatternBeforeWindow)]);
        Serial.print(" result_after_window=");
        Serial.print(missReasonCounts[analyzerReasonIndex(AnalyzerReason::ValidPatternAfterWindow)]);
        Serial.print(" no_occurrence=");
        Serial.print(missReasonCounts[analyzerReasonIndex(AnalyzerReason::NoOccurrence)]);
        Serial.print(" occ_rejected=");
        Serial.print(missReasonCounts[analyzerReasonIndex(AnalyzerReason::OccurrenceSeenButRejected)]);
        Serial.print(" inspection_failed=");
        Serial.print(missReasonCounts[analyzerReasonIndex(AnalyzerReason::InspectionFailed)]);
        Serial.print(" pattern_rejected=");
        Serial.print(missReasonCounts[analyzerReasonIndex(AnalyzerReason::PatternCandidateRejected)]);
        Serial.print(" dup_after_primary=");
        Serial.print(rejectReasonCounts[analyzerReasonIndex(AnalyzerReason::DuplicatePatternAfterPrimary)]);
        Serial.print(" unexpected_without_trigger=");
        Serial.print(rejectReasonCounts[analyzerReasonIndex(AnalyzerReason::UnexpectedValidPatternWithoutTrigger)]);
        Serial.print(" invalid_audio=");
        Serial.println(rejectReasonCounts[analyzerReasonIndex(AnalyzerReason::InvalidAudio)]);
    }

    if (analyzerLogEnabled(_sequenceTest.logFlags, AnalyzerApp::ANALYZER_LOG_CUSTOM)) {
        Serial.print("  A_profile_summary: matched_expected=");
        Serial.print(_sequenceTest.patternMatchedExpected);
        Serial.print(" unmatched_expected=");
        Serial.print(_sequenceTest.patternUnmatchedExpected);
        Serial.print(" matched_duplicates=");
        Serial.print(_sequenceTest.patternMatchedDuplicates);
        Serial.print(" unmatched_duplicates=");
        Serial.print(_sequenceTest.patternUnmatchedDuplicates);
        Serial.print(" matched_unexpected=");
        Serial.print(_sequenceTest.patternMatchedUnexpected);
        Serial.print(" unmatched_unexpected=");
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
    printOccurrenceSummary();
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
    printOccurrenceSummary();
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
    printOccurrenceSummary();
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

void AnalyzerApp::printOccurrenceSummary() const {
    const AudioSignalStats& stats = _audioSignal.stats();
    Serial.println("OCCURRENCE summary:");
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
    if (_lastPrintMs != 0 && !timing::elapsedSince(now, _lastPrintMs, kPrintIntervalMs)) {
        return;
    }

    _lastPrintMs = now;
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
    Serial.print(now < _valOnsetLatchedUntilMs ? 1 : 0);
    Serial.print('\t');
    Serial.print("transient:");
    Serial.println(now < _valTransientLatchedUntilMs ? 1 : 0);
}

