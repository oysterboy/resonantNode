#include "AnalyzerApp.h"

#include <Arduino.h>
#include <string.h>

#include "../../detection/features/FrequencyMatchEvaluation.h"
#include "../../detection/patterns/PatternNames.h"

namespace {

constexpr long kLateOnsetMinMs = 200L;

const char* sequenceCandidateClass(bool duplicateCandidate, bool inWindow, long dtFromTriggerMs) {
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

} // namespace

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
    Serial.print(" fields=t,current,env,peak,open");
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

detection::FrequencyFeatureFrame AnalyzerApp::captureFrequencyFeatureFrame(unsigned long observedAtMs) const {
    detection::FrequencyFeatureFrame evidence;
    evidence.observedAtMs = observedAtMs;
    const bool present = _freqBandStream.windowReady();
    const float totalEnergy = _freqBandStream.lastTotalEnergy();

    evidence.present = present;
    evidence.matched = false;
    evidence.updatedThisFrame = _freqBandStream.updatedOnLastObserve();
    evidence.targetHz = present ? _freqBandStream.targetFrequencyHz() : 0;
    evidence.windowSampleCount = _freqBandStream.sampleCount();
    evidence.ageSamples = _freqBandStream.evidenceAgeSamples();
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

const char* AnalyzerApp::sequenceTrialClassificationName(const char* result, long dtMs, long durMs, const SequenceTest::TrialDiagnostics& diagnostics) const {
    (void)dtMs;
    (void)durMs;
    (void)diagnostics;
    if (strcmp(result, "invalid_audio") == 0) {
        return "invalid_audio";
    }
    if (strcmp(result, "unexpected") == 0) {
        return "unexpected";
    }
    if (strcmp(result, "late") == 0) {
        return "late";
    }
    return result;
}

void AnalyzerApp::recordSequenceClassifierOutcome(const detection::PatternResult& patternResult, bool duplicateCandidate, bool unexpectedCandidate) {
    if (_valMode || !patternResult.patternCandidateAccepted) {
        return;
    }

    const auto freqEval = FrequencyMatchEvaluation::evaluate(patternResult.freq, _frequencyEvidenceTuning);
    const bool patternMatched = patternResult.valid;

    if (unexpectedCandidate) {
        if (patternMatched) {
            ++_sequenceTest.patternMatchedUnexpected;
        } else {
            ++_sequenceTest.patternUnmatchedUnexpected;
        }
    } else if (duplicateCandidate) {
        if (patternMatched) {
            ++_sequenceTest.patternMatchedDuplicates;
        } else {
            ++_sequenceTest.patternUnmatchedDuplicates;
        }
    } else {
        if (patternMatched) {
            ++_sequenceTest.patternMatchedExpected;
        } else {
            ++_sequenceTest.patternUnmatchedExpected;
        }
    }

    switch (freqEval.reason) {
        case FrequencyMatchEvaluation::Reason::None:
            break;
        case FrequencyMatchEvaluation::Reason::NoEvidence:
            ++_sequenceTest.freqRejectNoEvidence;
            break;
        case FrequencyMatchEvaluation::Reason::InvalidWindow:
            ++_sequenceTest.freqRejectInvalidWindow;
            break;
        case FrequencyMatchEvaluation::Reason::ScoreTooLow:
            ++_sequenceTest.freqRejectScore;
            break;
        case FrequencyMatchEvaluation::Reason::ContrastTooLow:
            ++_sequenceTest.freqRejectContrast;
            break;
        case FrequencyMatchEvaluation::Reason::ScoreAndContrastTooLow:
            ++_sequenceTest.freqRejectBoth;
            break;
    }
}

void AnalyzerApp::handleSequenceCandidate(const detection::PatternResult& patternResult, const detection::FrequencyFeatureFrame* liveFrequencyFrame) {
    if (_valMode) {
        return;
    }

    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0) {
        return;
    }

    auto& diagnostics = _sequenceTest.currentTrialDiagnostics;
    diagnostics.rawCandidateCount++;

    const auto& candidate = patternResult.candidate;
    const unsigned long onsetMs = candidate.startMs;
    const long dtFromTriggerMs = static_cast<long>(onsetMs) - static_cast<long>(_sequenceTest.currentTrialScheduledAtMs);
    const long dtFromTrialStartMs = static_cast<long>(onsetMs) - static_cast<long>(_sequenceTest.currentTrialStartMs);
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

    const bool preWindow = onsetMs < _sequenceTest.currentTrialStartMs + _sequenceTest.windowStartOffsetMs;
    const bool postWindow = onsetMs > _sequenceTest.currentTrialEndMs;
    const bool inWindow = !preWindow && !postWindow;
    const bool duplicateCandidate = _sequenceTest.primaryValidPatternCaptured && inWindow;
    const char* candidateClass = sequenceCandidateClass(duplicateCandidate, inWindow, dtFromTriggerMs);

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
        entry.onsetSample = candidate.onsetSample;
        entry.peakSample = candidate.peakSample;
        entry.releaseSample = candidate.releaseSample;
        entry.peakMs = candidate.startMs + peakOffsetMs;
        entry.endDtMs = dtFromTriggerMs >= 0 ? dtFromTriggerMs + static_cast<long>(candidate.durationMs) : -1;
        entry.processedAtMs = patternResult.processedAtMs;
        entry.processLagMs = processLagMs;
        entry.transientPresent = patternResult.candidate.transient.present;
        entry.freqPresent = patternResult.freq.present;
        entry.freqMatched = patternResult.freq.matched;
        entry.freqScore = patternResult.freq.score;
        entry.freqContrast = patternResult.freq.spectralContrast;
        entry.patternValid = patternResult.valid;
        entry.candidateAccepted = patternResult.patternCandidateAccepted;
        entry.patternMatched = patternResult.patternMatched;
        entry.supportMatched = patternResult.supportMatched;
        entry.behaviorEligible = patternResult.valid;
        entry.duplicateCandidate = duplicateCandidate;
        entry.candidateClass = candidateClass;
        entry.patternType = detection::patternTypeName(patternResult.type);
        entry.reason = detection::patternReasonName(patternResult.reasonCode);
        entry.rejectReason = detection::patternRejectReasonName(patternResult.rejectReason);
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

    if (!diagnostics.bestCandidateAccepted || candidate.peakStrength > diagnostics.bestCandidateStrength) {
        diagnostics.bestCandidateAccepted = true;
        diagnostics.bestCandidateDtFromTriggerMs = dtFromTriggerMs;
        diagnostics.bestCandidateDurationMs = candidate.durationMs;
        diagnostics.bestCandidateStrength = candidate.peakStrength;
        diagnostics.bestCandidateOrigin = origin;
    }

    if (!inWindow) {
        if (!_sequenceTest.trialHadAudioOverflow) {
            _sequenceTest.unexpected++;
            _sequenceTest.currentTrialUnexpected++;
        }
        return;
    }

    if (!patternResult.valid) {
        if (_sequenceTest.currentTrialRejected == 0) {
            _sequenceTest.firstRejectedInWindow = patternResult;
        }
        _sequenceTest.rejectedInWindowCount++;
        _sequenceTest.currentTrialRejected++;
        return;
    }

    if (!_sequenceTest.primaryValidPatternCaptured) {
        _sequenceTest.primaryValidPatternCaptured = true;
        _sequenceTest.primaryValidPattern = patternResult;
        _sequenceTest.primaryValidPatternDtMs = dtFromTriggerMs;
    }

    _sequenceTest.currentTrialDiagnostics.onsetSeen = true;
    if (_sequenceTest.currentTrialDiagnostics.firstOnsetMs == 0) {
        _sequenceTest.currentTrialDiagnostics.firstOnsetMs = onsetMs;
    }
    _sequenceTest.currentTrialDiagnostics.lastOnsetMs = onsetMs;
    if (_sequenceTest.currentTrialOnsetDetectedMs == 0) {
        _sequenceTest.currentTrialOnsetDetectedMs = onsetMs;
    }

    if (_sequenceTest.primaryValidPatternCaptured) {
        if (diagnostics.duplicateCount == 0) {
            diagnostics.duplicatePatternMs = onsetMs;
            diagnostics.duplicatePatternStrength = candidate.peakStrength;
            diagnostics.duplicatePatternDurationMs = candidate.durationMs;
            diagnostics.duplicatePatternOnsetSample = candidate.onsetSample;
            diagnostics.duplicatePatternPeakSample = candidate.peakSample;
            diagnostics.duplicatePatternReleaseSample = candidate.releaseSample;
            diagnostics.duplicatePatternPeakMs = candidate.startMs + peakOffsetMs;
            diagnostics.duplicatePatternReleaseMs = candidate.startMs + candidate.durationMs;
            diagnostics.duplicateFrequencyProcessedAtMs = patternResult.processedAtMs;
            diagnostics.duplicateDeltaFromPrimaryMs = diagnostics.patternAccepted
                ? static_cast<long>(onsetMs) - static_cast<long>(diagnostics.acceptedPatternMs)
                : 0;
            strncpy(diagnostics.duplicateReason, "duplicate_after_primary", sizeof(diagnostics.duplicateReason) - 1);
            diagnostics.duplicateReason[sizeof(diagnostics.duplicateReason) - 1] = '\0';
        }
        _sequenceTest.currentTrialDiagnostics.duplicateCount++;
        if (_sequenceTest.currentTrialDiagnostics.duplicateDtCount < SequenceTest::kMaxDuplicateDts) {
            _sequenceTest.currentTrialDiagnostics.duplicateDts[_sequenceTest.currentTrialDiagnostics.duplicateDtCount++] = onsetMs >= _sequenceTest.currentTrialPatternDetectedMs
                ? onsetMs - _sequenceTest.currentTrialPatternDetectedMs
                : 0;
        }
        return;
    }

    _sequenceTest.currentTrialDiagnostics.patternAccepted = true;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternMs = onsetMs;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternOnsetStrength = candidate.onsetStrength;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternStrength = candidate.peakStrength;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternDurationMs = candidate.durationMs;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternReleaseStrength = candidate.releaseStrength;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternOnsetSample = candidate.onsetSample;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternPeakSample = candidate.peakSample;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternReleaseSample = candidate.releaseSample;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternPeakMs = candidate.startMs + peakOffsetMs;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternReleaseMs = candidate.startMs + candidate.durationMs;
    _sequenceTest.currentTrialDiagnostics.acceptedAmbientBaseline = candidate.ambientBaseline;
    _sequenceTest.currentTrialDiagnostics.acceptedFrequencyProcessedAtMs = patternResult.processedAtMs;
    _sequenceTest.currentTrialDiagnostics.lastRejectStrength = 0.0f;
    _sequenceTest.currentTrialDiagnostics.lastRejectDurationMs = 0;
    _sequenceTest.currentTrialPatternDetectedMs = onsetMs;

}
