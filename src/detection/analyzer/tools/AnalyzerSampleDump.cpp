#include "../../../modes/analyzer/AnalyzerModeApp.h"

#include <Arduino.h>
#include <string.h>

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

// Sample-dump output is diagnostic-only.
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
