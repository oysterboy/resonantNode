#include "AnalyzerApp.h"

#include <Arduino.h>
#include <string.h>

#include "../../detection/features/FrequencyMeasurementPacketBuilder.h"
#include "../../detection/features/FrequencyMatchEvaluation.h"
#include "../../detection/patterns/PatternNames.h"

/*
AnalyzerSequenceHelpers

Analyzer helper plumbing for sample-dump tooling and classifier bookkeeping.
Canonical detector output lives in AnalyzerReporting.
*/

namespace {

constexpr long kLateOnsetMinMs = 200L;

enum class SequencePendingClass : uint8_t {
    Unknown = 0,
    ExpectedPrimary,
    Late,
    UnexpectedNoise,
    Duplicate,
};

uint8_t sequencePendingClass(bool duplicatePending, bool inWindow, long dtFromTriggerMs) {
    if (duplicatePending) {
        return static_cast<uint8_t>(SequencePendingClass::Duplicate);
    }
    if (!inWindow) {
        return static_cast<uint8_t>(SequencePendingClass::UnexpectedNoise);
    }
    if (dtFromTriggerMs >= kLateOnsetMinMs) {
        return static_cast<uint8_t>(SequencePendingClass::Late);
    }
    return static_cast<uint8_t>(SequencePendingClass::ExpectedPrimary);
}

} // namespace

const char* sequencePendingClassName(uint8_t value) {
    switch (static_cast<SequencePendingClass>(value)) {
        case SequencePendingClass::ExpectedPrimary:
            return "expected_primary";
        case SequencePendingClass::Late:
            return "late";
        case SequencePendingClass::UnexpectedNoise:
            return "unexpected_noise";
        case SequencePendingClass::Duplicate:
            return "duplicate";
        case SequencePendingClass::Unknown:
        default:
            return "unknown";
    }
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

// Sample-dump tooling path: explicit diagnostic report, not analyzer truth.
void AnalyzerApp::printSequenceSampleReport(unsigned long trialNumber) const {
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

detection::FrequencyBandMeasurementPacket AnalyzerApp::captureFrequencyMeasurementPacket(const AudioSamplePacket& audioSamplePacket) const {
    return detection::buildFrequencyMeasurementPacket(_freqBandStream, audioSamplePacket);
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

void AnalyzerApp::handleSequencePending(const detection::PatternResult& patternResult, const detection::FrequencyBandMeasurementPacket* liveFrequencyMeasurementPacket) {
    if (!_sequenceTest.active || _sequenceTest.currentTrial == 0) {
        return;
    }

    auto& diagnostics = _sequenceTest.currentTrialDiagnostics;
    diagnostics.rawPendingCount++;

    const unsigned long onsetMs = patternResult.primaryStartMs;
    const long dtFromTriggerMs = static_cast<long>(onsetMs) - static_cast<long>(_sequenceTest.currentTrialScheduledAtMs);
    const long dtFromTrialStartMs = static_cast<long>(onsetMs) - static_cast<long>(_sequenceTest.currentTrialStartMs);

    const bool overflowSeenNow = patternResult.primaryAudioOverflow
                                 || _audioSource.stats().overflowCount != _sequenceTest.trialOverflowCountAtStart;
    if (overflowSeenNow) {
        _sequenceTest.trialHadAudioOverflow = true;
    }

    const bool preWindow = onsetMs < _sequenceTest.currentTrialStartMs + _sequenceTest.windowStartOffsetMs;
    const bool postWindow = onsetMs > _sequenceTest.currentTrialEndMs;
    const bool inWindow = !preWindow && !postWindow;
    const bool duplicatePending = _sequenceTest.primaryValidPatternCaptured && inWindow;
    const auto pendingClass = sequencePendingClass(duplicatePending, inWindow, dtFromTriggerMs);

    const SequenceTest::PendingOrigin origin = preWindow
        ? SequenceTest::PendingOrigin::PreWindow
        : postWindow
            ? SequenceTest::PendingOrigin::PostWindow
            : SequenceTest::PendingOrigin::InWindow;

    if (diagnostics.firstPendingMs == 0) {
        diagnostics.firstPendingMs = onsetMs;
    }

    if (diagnostics.pendingCount < SequenceTest::kMaxTrialPending) {
        auto& entry = diagnostics.pendingSamples[diagnostics.pendingCount++];
        entry.pendingMs = onsetMs;
        entry.dtFromTriggerMs = dtFromTriggerMs;
        entry.dtFromTrialStartMs = dtFromTrialStartMs;
        entry.durationMs = patternResult.primaryDurationMs;
        entry.strength = patternResult.primaryStrength;
        entry.origin = origin;
        entry.peakMs = patternResult.primaryPeakMs;
        entry.endDtMs = dtFromTriggerMs >= 0 ? dtFromTriggerMs + static_cast<long>(patternResult.primaryDurationMs) : -1;
        entry.patternValid = patternResult.valid;
        entry.patternAccepted = patternResult.patternAccepted;
        entry.patternMatched = patternResult.patternMatched;
        entry.supportMatched = patternResult.supportMatched;
        entry.behaviorEligible = patternResult.valid;
        entry.duplicatePending = duplicatePending;
        entry.pendingClass = pendingClass;
        entry.patternType = patternResult.type;
        entry.reasonCode = patternResult.reasonCode;
        entry.rejectReasonCode = patternResult.rejectReason;
    } else {
        diagnostics.pendingOverflowCount++;
    }

    if (origin == SequenceTest::PendingOrigin::PreWindow) {
        diagnostics.pendingPreWindowCount++;
    } else if (origin == SequenceTest::PendingOrigin::InWindow) {
        diagnostics.pendingInWindowCount++;
    } else {
        diagnostics.pendingPostWindowCount++;
    }

    if (!diagnostics.bestPendingAccepted || patternResult.primaryStrength > diagnostics.bestPendingStrength) {
        diagnostics.bestPendingAccepted = true;
        diagnostics.bestPendingDtFromTriggerMs = dtFromTriggerMs;
        diagnostics.bestPendingDurationMs = patternResult.primaryDurationMs;
        diagnostics.bestPendingStrength = patternResult.primaryStrength;
        diagnostics.bestPendingOrigin = origin;
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

    const bool hadPrimaryBeforePending = _sequenceTest.primaryValidPatternCaptured;

    if (!hadPrimaryBeforePending) {
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

    if (hadPrimaryBeforePending) {
        if (diagnostics.duplicateCount == 0) {
            diagnostics.duplicatePatternMs = onsetMs;
            diagnostics.duplicatePatternStrength = patternResult.primaryStrength;
            diagnostics.duplicatePatternDurationMs = patternResult.primaryDurationMs;
            diagnostics.duplicatePatternPeakMs = patternResult.primaryPeakMs;
            diagnostics.duplicatePatternReleaseMs = patternResult.primaryStartMs + patternResult.primaryDurationMs;
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
    _sequenceTest.currentTrialDiagnostics.acceptedPatternOnsetStrength = patternResult.primaryOnsetStrength;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternStrength = patternResult.primaryStrength;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternDurationMs = patternResult.primaryDurationMs;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternReleaseStrength = patternResult.primaryReleaseStrength;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternPeakMs = patternResult.primaryPeakMs;
    _sequenceTest.currentTrialDiagnostics.acceptedPatternReleaseMs = patternResult.primaryStartMs + patternResult.primaryDurationMs;
    _sequenceTest.currentTrialDiagnostics.acceptedAmbientBaseline = patternResult.primaryAmbientBaseline;
    _sequenceTest.currentTrialDiagnostics.lastRejectStrength = 0.0f;
    _sequenceTest.currentTrialDiagnostics.lastRejectDurationMs = 0;
    _sequenceTest.currentTrialPatternDetectedMs = onsetMs;

}
