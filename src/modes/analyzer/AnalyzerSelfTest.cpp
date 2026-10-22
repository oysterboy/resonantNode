#include "AnalyzerModeApp.h"

#include <Arduino.h>

#include "../../selftest/AudioSelfTest.h"
#include "../../app/TimeBase.h"
#include "../../app/SerialOut.h"

/*
Analyzer SELFTEST (NODE-015, docs/refactors/archive/selftest.md)

"SELFTEST": board, mic, amp. The Analyzer has no reactive output; on the
D-AMP board the amp check plays its tones through an I2sToneOutput on the
same port, whose writer task runs only for the check. The Node-only checks
(quiet_boot, own_chirp_suppressed, external_chirp) are not printed.
"SELFTEST chirp n=N": N tones, the helper side of a Node's external_chirp.
Refused while a SEQ run is active or pending. Blocking; afterwards the
detection chain restarts on fresh audio (a SEQ start resets it anyway).
*/

void AnalyzerApp::handleSelfTestCommand(const char* line) {
    const selftest::Command command = selftest::parseCommand(line);
    if (command.kind == selftest::Command::Kind::None) {
        return;
    }
    if (command.kind == selftest::Command::Kind::Help || command.kind == selftest::Command::Kind::Unknown) {
        selftest::printHelp(serialOut(), false);
        return;
    }
    if (_sequenceTest.active || _pendingSequenceStart.active) {
        serialOut().println("SELFTEST busy reason=seq_active");
        return;
    }

    if (command.kind == selftest::Command::Kind::Chirp) {
#if defined(BOARD_PIEZO)
        serialOut().print("SELFTEST_CHIRP done n=");
        serialOut().print(command.chirps);
        serialOut().println(" started=0 reason=no_output_in_build");
#else
        _selfTestTone.begin();
        selftest::emitChirpsBlocking(serialOut(), &_i2sSource, _selfTestTone, runtime::kDefaultChirpFrequencyHz,
                                     command.chirps, command.durMs, command.gapMs);
        _selfTestTone.end();
        restoreAfterSelfTest();
#endif
        return;
    }

    selftest::Tally tally;
    const bool external = command.kind == selftest::Command::Kind::External;
    selftest::printBegin(serialOut(), "analyzer", external ? "external" : "full");
    selftest::printBoard(serialOut(), tally, "analyzer");
    if (external) {
        selftest::skip(serialOut(), tally, "external_chirp", "not_node");
        selftest::printSummary(serialOut(), tally, "analyzer");
        return;
    }

    const selftest::Result mic = selftest::runMicCheck(serialOut(), tally, _i2sSource);
#if defined(BOARD_PIEZO)
    selftest::skip(serialOut(), tally, "amp", "no_output_in_build");
#else
    if (mic != selftest::Result::Pass) {
        selftest::skip(serialOut(), tally, "amp", "mic_failed");
    } else {
        _selfTestTone.begin();
        selftest::runAmpCheck(serialOut(), tally, _i2sSource, _selfTestTone, runtime::kDefaultChirpFrequencyHz, "i2s");
        _selfTestTone.end();
    }
#endif
    restoreAfterSelfTest();
    selftest::printSummary(serialOut(), tally, "analyzer");
}

void AnalyzerApp::restoreAfterSelfTest() {
    _audioSignal.resetSignalState();
    _detection.resetDetectionState();
    _freqBandStream.resetState();
    _freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
    _freqBandStream.setTargetFrequencyHz(runtime::kDefaultChirpFrequencyHz);
    // The check held the loop for seconds; keep it out of the loop health.
    _loopLastUs = timebase::nowUs();
    serialOut().println("SELFTEST_RESTORE detection=reset");
}
