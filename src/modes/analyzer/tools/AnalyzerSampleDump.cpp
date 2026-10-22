#include "../AnalyzerModeApp.h"

#include <Arduino.h>
#include <string.h>

#include "../../../app/TimeBase.h"
#include "../../../app/TimingUtils.h"
#include "../../../app/SerialOut.h"

namespace {

const char* detectorInputName(detection::DetectorSelection selection, detection::FeatureStreamId stream) {
    switch (selection) {
        case detection::DetectorSelection::ScalarTransient:
            return detection::featureStreamName(stream);
    }

    return "unknown";
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
    _sequenceTest.sampleDumpDetectorSelection = detection::DetectorSelection::ScalarTransient;
    _sequenceTest.sampleDumpObservedStream = detection::FeatureStreamId::Unknown;
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
    if (timing::atOrAfter(_sequenceTest.sampleHistoryPending.sampleMs, currentSampleMs)) {
        return;
    }

    const DetectorInputSample committed = _sequenceTest.sampleHistoryPending;
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

    if (!timing::inWindow(committed.sampleMs, _sequenceTest.sampleDumpCaptureStartMs, _sequenceTest.sampleDumpCaptureEndMs)) {
        return;
    }
    if (timing::beforeDeadline(committed.sampleMs, _sequenceTest.sampleDumpNextEmitMs)) {
        return;
    }

    if (_sequenceTest.sampleRowCount >= SequenceTest::kMaxSampleRows) {
        if (!_sequenceTest.sampleDumpWarned) {
            serialOut().print("SAMPLES_WARN reason=too_many_samples requested=");
            serialOut().print(_sequenceTest.sampleRowCount + 1UL);
            serialOut().print(" max_allowed=");
            serialOut().println(SequenceTest::kMaxSampleRows);
            _sequenceTest.sampleDumpWarned = true;
        }
        _sequenceTest.sampleDumpCapturing = false;
        return;
    }

    _sequenceTest.sampleRows[_sequenceTest.sampleRowCount++] = committed;
    _sequenceTest.sampleDumpNextEmitMs = committed.sampleMs + _sequenceTest.sampleDumpStepMs;
}

void AnalyzerApp::recordSequenceDetectorInputSample(unsigned long sampleMs, float value) {
    DetectorInputSample sample;
    sample.sampleMs = sampleMs;
    sample.value = value;

    if (!_sequenceTest.sampleHistoryHasPending) {
        _sequenceTest.sampleHistoryPending = sample;
        _sequenceTest.sampleHistoryHasPending = true;
        return;
    }

    if (sampleMs == _sequenceTest.sampleHistoryPending.sampleMs) {
        _sequenceTest.sampleHistoryPending = sample;
        return;
    }

    flushSequenceSampleHistory(sampleMs);
    _sequenceTest.sampleHistoryPending = sample;
    _sequenceTest.sampleHistoryHasPending = true;
}

void AnalyzerApp::beginSequenceSampleDump(unsigned long trialNumber) {
    _sequenceTest.sampleDumpSelectedForTrial = sequenceSampleDumpSelected(trialNumber);
    _sequenceTest.sampleDumpCurrentTrial = trialNumber;
    _sequenceTest.sampleDumpCapturing = _sequenceTest.sampleDumpSelectedForTrial;
    _sequenceTest.sampleDumpTriggerMs = _sequenceTest.currentTrialStartMs;
    _sequenceTest.sampleDumpTriggerSampleMs = timebase::msFromUs64(_audioSignal.sampleTimeUs64());
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
        if (timing::beforeDeadline(snapshot.sampleMs, _sequenceTest.sampleDumpCaptureStartMs)) {
            continue;
        }
        if (timing::after(snapshot.sampleMs, _sequenceTest.sampleDumpTriggerSampleMs)) {
            break;
        }
        if (timing::atOrAfter(snapshot.sampleMs, _sequenceTest.sampleDumpNextEmitMs)) {
            if (_sequenceTest.sampleRowCount < SequenceTest::kMaxSampleRows) {
                _sequenceTest.sampleRows[_sequenceTest.sampleRowCount++] = snapshot;
                _sequenceTest.sampleDumpNextEmitMs = snapshot.sampleMs + _sequenceTest.sampleDumpStepMs;
            } else if (!_sequenceTest.sampleDumpWarned) {
                serialOut().print("SAMPLES_WARN reason=too_many_samples requested=");
                serialOut().print(_sequenceTest.sampleRowCount + 1UL);
                serialOut().print(" max_allowed=");
                serialOut().println(SequenceTest::kMaxSampleRows);
                _sequenceTest.sampleDumpWarned = true;
                _sequenceTest.sampleDumpCapturing = false;
                break;
            }
        }
    }
}

// Sample-dump output is detector-input tooling, not detector truth.
void AnalyzerApp::printSequenceSampleReport(unsigned long trialNumber) const {
    if (!_sequenceTest.sampleDumpEnabled || !_sequenceTest.sampleDumpSelectedForTrial || _sequenceTest.sampleDumpCurrentTrial != trialNumber) {
        return;
    }

    serialOut().print("SAMPLES_BEGIN trial=");
    serialOut().print(trialNumber);
    serialOut().print(" source=detector_input trigger_ms=");
    serialOut().print(_sequenceTest.sampleDumpTriggerMs);
    serialOut().print(" sample_rate_ms=");
    serialOut().print(_sequenceTest.sampleDumpStepMs);
    serialOut().print(" fields=t,value");
    serialOut().println();
    serialOut().print("SAMPLES_NOTE trial=");
    serialOut().print(trialNumber);
    serialOut().print(" input=");
    serialOut().print(detectorInputName(_sequenceTest.sampleDumpDetectorSelection, _sequenceTest.sampleDumpObservedStream));
    serialOut().print(" rows_cap=");
    serialOut().print(SequenceTest::kMaxSampleRows);
    serialOut().print(" sample_max_budget=");
    serialOut().print(_sequenceTest.sampleDumpMaxRows);
    serialOut().print(" value=detector_input_scalar");
    serialOut().println();

    for (size_t i = 0; i < _sequenceTest.sampleRowCount; ++i) {
        const auto& sample = _sequenceTest.sampleRows[i];
        const long tMs = static_cast<long>(timing::delta(sample.sampleMs, _sequenceTest.sampleDumpTriggerSampleMs));
        serialOut().print(tMs);
        serialOut().print(",");
        serialOut().println(sample.value, 3);
    }

    serialOut().print("SAMPLES_END trial=");
    serialOut().println(trialNumber);
}
