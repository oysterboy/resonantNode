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
    if (!_sequenceTest.config.sampleDumpEnabled) {
        return false;
    }

    const bool firstSelected = _sequenceTest.config.sampleDumpFirstTrials > 0 && trialNumber <= _sequenceTest.config.sampleDumpFirstTrials;
    const bool everySelected = _sequenceTest.config.sampleDumpEveryNth > 0 && trialNumber % _sequenceTest.config.sampleDumpEveryNth == 0;
    return firstSelected || everySelected;
}

void AnalyzerApp::clearSequenceSampleDump() {
    _sequenceTest.sampleDump.selectedForTrial = false;
    _sequenceTest.sampleDump.capturing = false;
    _sequenceTest.sampleDump.trialNumber = 0;
    _sequenceTest.sampleDump.triggerMs = 0;
    _sequenceTest.sampleDump.triggerSampleMs = 0;
    _sequenceTest.sampleDump.captureStartMs = 0;
    _sequenceTest.sampleDump.captureEndMs = 0;
    _sequenceTest.sampleDump.nextEmitMs = 0;
    _sequenceTest.sampleDump.detectorSelection = detection::DetectorSelection::ScalarTransient;
    _sequenceTest.sampleDump.observedStream = detection::FeatureStreamId::Unknown;
    _sequenceTest.sampleDump.rowCount = 0;
    _sequenceTest.sampleDump.historyStart = 0;
    _sequenceTest.sampleDump.historyCount = 0;
    _sequenceTest.sampleDump.historyLastMs = 0;
    _sequenceTest.sampleDump.historyHasPending = false;
    _sequenceTest.sampleDump.historyPending = {};
}

void AnalyzerApp::flushSequenceSampleHistory(unsigned long currentSampleMs) {
    if (!_sequenceTest.sampleDump.historyHasPending) {
        return;
    }
    if (timing::atOrAfter(_sequenceTest.sampleDump.historyPending.sampleMs, currentSampleMs)) {
        return;
    }

    const DetectorInputSample committed = _sequenceTest.sampleDump.historyPending;
    _sequenceTest.sampleDump.historyHasPending = false;
    _sequenceTest.sampleDump.historyPending = {};

    if (_sequenceTest.sampleDump.historyCount < SampleDumpState::kMaxSampleHistory) {
        const size_t index = (_sequenceTest.sampleDump.historyStart + _sequenceTest.sampleDump.historyCount) % SampleDumpState::kMaxSampleHistory;
        _sequenceTest.sampleDump.history[index] = committed;
        ++_sequenceTest.sampleDump.historyCount;
    } else {
        _sequenceTest.sampleDump.history[_sequenceTest.sampleDump.historyStart] = committed;
        _sequenceTest.sampleDump.historyStart = (_sequenceTest.sampleDump.historyStart + 1) % SampleDumpState::kMaxSampleHistory;
    }

    _sequenceTest.sampleDump.historyLastMs = committed.sampleMs;

    if (!_sequenceTest.sampleDump.capturing
        || !_sequenceTest.sampleDump.selectedForTrial
        || _sequenceTest.sampleDump.trialNumber != _sequenceTest.trial.number) {
        return;
    }

    if (!timing::inWindow(committed.sampleMs, _sequenceTest.sampleDump.captureStartMs, _sequenceTest.sampleDump.captureEndMs)) {
        return;
    }
    if (timing::beforeDeadline(committed.sampleMs, _sequenceTest.sampleDump.nextEmitMs)) {
        return;
    }

    if (_sequenceTest.sampleDump.rowCount >= SampleDumpState::kMaxSampleRows) {
        if (!_sequenceTest.sampleDump.warned) {
            serialOut().print("SAMPLES_WARN reason=too_many_samples requested=");
            serialOut().print(_sequenceTest.sampleDump.rowCount + 1UL);
            serialOut().print(" max_allowed=");
            serialOut().println(SampleDumpState::kMaxSampleRows);
            _sequenceTest.sampleDump.warned = true;
        }
        _sequenceTest.sampleDump.capturing = false;
        return;
    }

    _sequenceTest.sampleDump.rows[_sequenceTest.sampleDump.rowCount++] = committed;
    _sequenceTest.sampleDump.nextEmitMs = committed.sampleMs + _sequenceTest.config.sampleDumpStepMs;
}

void AnalyzerApp::recordSequenceDetectorInputSample(unsigned long sampleMs, float value) {
    DetectorInputSample sample;
    sample.sampleMs = sampleMs;
    sample.value = value;

    if (!_sequenceTest.sampleDump.historyHasPending) {
        _sequenceTest.sampleDump.historyPending = sample;
        _sequenceTest.sampleDump.historyHasPending = true;
        return;
    }

    if (sampleMs == _sequenceTest.sampleDump.historyPending.sampleMs) {
        _sequenceTest.sampleDump.historyPending = sample;
        return;
    }

    flushSequenceSampleHistory(sampleMs);
    _sequenceTest.sampleDump.historyPending = sample;
    _sequenceTest.sampleDump.historyHasPending = true;
}

void AnalyzerApp::beginSequenceSampleDump(unsigned long trialNumber) {
    _sequenceTest.sampleDump.selectedForTrial = sequenceSampleDumpSelected(trialNumber);
    _sequenceTest.sampleDump.trialNumber = trialNumber;
    _sequenceTest.sampleDump.capturing = _sequenceTest.sampleDump.selectedForTrial;
    _sequenceTest.sampleDump.triggerMs = _sequenceTest.trial.startMs;
    _sequenceTest.sampleDump.triggerSampleMs = timebase::msFromUs64(_audioSignal.sampleTimeUs64());
    _sequenceTest.sampleDump.captureStartMs = _sequenceTest.sampleDump.triggerSampleMs > _sequenceTest.config.sampleDumpLeadMs
        ? _sequenceTest.sampleDump.triggerSampleMs - _sequenceTest.config.sampleDumpLeadMs
        : 0;
    _sequenceTest.sampleDump.captureEndMs = _sequenceTest.sampleDump.triggerSampleMs + _sequenceTest.config.sampleDumpTailMs;
    _sequenceTest.sampleDump.nextEmitMs = _sequenceTest.sampleDump.captureStartMs;
    _sequenceTest.sampleDump.rowCount = 0;

    flushSequenceSampleHistory(_sequenceTest.sampleDump.triggerSampleMs + 1UL);

    if (!_sequenceTest.sampleDump.capturing) {
        return;
    }

    for (size_t i = 0; i < _sequenceTest.sampleDump.historyCount; ++i) {
        const size_t index = (_sequenceTest.sampleDump.historyStart + i) % SampleDumpState::kMaxSampleHistory;
        const auto& snapshot = _sequenceTest.sampleDump.history[index];
        if (timing::beforeDeadline(snapshot.sampleMs, _sequenceTest.sampleDump.captureStartMs)) {
            continue;
        }
        if (timing::after(snapshot.sampleMs, _sequenceTest.sampleDump.triggerSampleMs)) {
            break;
        }
        if (timing::atOrAfter(snapshot.sampleMs, _sequenceTest.sampleDump.nextEmitMs)) {
            if (_sequenceTest.sampleDump.rowCount < SampleDumpState::kMaxSampleRows) {
                _sequenceTest.sampleDump.rows[_sequenceTest.sampleDump.rowCount++] = snapshot;
                _sequenceTest.sampleDump.nextEmitMs = snapshot.sampleMs + _sequenceTest.config.sampleDumpStepMs;
            } else if (!_sequenceTest.sampleDump.warned) {
                serialOut().print("SAMPLES_WARN reason=too_many_samples requested=");
                serialOut().print(_sequenceTest.sampleDump.rowCount + 1UL);
                serialOut().print(" max_allowed=");
                serialOut().println(SampleDumpState::kMaxSampleRows);
                _sequenceTest.sampleDump.warned = true;
                _sequenceTest.sampleDump.capturing = false;
                break;
            }
        }
    }
}

// Sample-dump output is detector-input tooling, not detector truth.
void AnalyzerApp::printSequenceSampleReport(unsigned long trialNumber) const {
    if (!_sequenceTest.config.sampleDumpEnabled || !_sequenceTest.sampleDump.selectedForTrial || _sequenceTest.sampleDump.trialNumber != trialNumber) {
        return;
    }

    serialOut().print("SAMPLES_BEGIN trial=");
    serialOut().print(trialNumber);
    serialOut().print(" source=detector_input trigger_ms=");
    serialOut().print(_sequenceTest.sampleDump.triggerMs);
    serialOut().print(" sample_rate_ms=");
    serialOut().print(_sequenceTest.config.sampleDumpStepMs);
    serialOut().print(" fields=t,value");
    serialOut().println();
    serialOut().print("SAMPLES_NOTE trial=");
    serialOut().print(trialNumber);
    serialOut().print(" input=");
    serialOut().print(detectorInputName(_sequenceTest.sampleDump.detectorSelection, _sequenceTest.sampleDump.observedStream));
    serialOut().print(" rows_cap=");
    serialOut().print(SampleDumpState::kMaxSampleRows);
    serialOut().print(" sample_max_budget=");
    serialOut().print(_sequenceTest.config.sampleDumpMaxRows);
    serialOut().print(" value=detector_input_scalar");
    serialOut().println();

    for (size_t i = 0; i < _sequenceTest.sampleDump.rowCount; ++i) {
        const auto& sample = _sequenceTest.sampleDump.rows[i];
        const long tMs = static_cast<long>(timing::delta(sample.sampleMs, _sequenceTest.sampleDump.triggerSampleMs));
        serialOut().print(tMs);
        serialOut().print(",");
        serialOut().println(sample.value, 3);
    }

    serialOut().print("SAMPLES_END trial=");
    serialOut().println(trialNumber);
}
