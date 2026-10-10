#include "ResonantNodeApp.h"

#include <Arduino.h>
#include <string.h>

#include "../../selftest/AudioSelfTest.h"

/*
Node SELFTEST (NODE-015, docs/refactors/selftest.md)

Sequence for "SELFTEST":
  board -> quiet_boot (waits while the startup rebase is still running)
  -> mic, amp (blocking, on the I2S source; detection is reset afterwards
     and the baseline goes through Settle again)
  -> own_chirp_suppressed (normal loop: Behavior is asked for N single
     chirps, which go through the usual request -> ChirpOutput ->
     notifyChirpStarted/Finished path; any verdict heard during one of the
     node's own chirps or up to kEchoWindowMs after it is a FAIL)
  -> external_chirp SKIP (not requested) -> summary.
"SELFTEST external wait_ms=N": board -> external_chirp (a valid verdict
  not attributable to an own chirp within N ms) -> summary.
"SELFTEST chirp n=N": N own chirps through Behavior, no judgement (the
  helper side of another board's external_chirp).

Observes only what the Node already receives: verdicts popped from
DetectionRuntime and its own chirp start/finish events. No thresholds are
changed; the board keeps running normally between and after the phases.
*/

namespace {
constexpr unsigned long kQuietBootWaitMs = 15000;   // boot warmup 2 s + quiet timeout 8 s + margin
constexpr unsigned long kSettleAfterHardwareMs = 500; // same as the post-rebase settle
constexpr unsigned long kOutputsWaitMs = 15000;
constexpr unsigned long kEchoWindowMs = 300;       // after an own chirp's finish
constexpr unsigned long kOpenChirpSpanMs = 2000;   // own chirp still sounding: assumed length
constexpr unsigned long kOwnTailMs = 1000;
constexpr unsigned long kResponseWaitMs = 1500;    // after the first external verdict
constexpr unsigned long kChirpStartTimeoutMs = 3000;

const char* rebaseResultName(uint8_t result) {
    switch (result) {
        case 1:
            return "done";
        case 2:
            return "no_quiet";
        default:
            return "pending";
    }
}

bool deadlinePassed(unsigned long now, unsigned long deadline) {
    return static_cast<long>(now - deadline) >= 0;
}
} // namespace

void Node::noteRebaseResult(RbRebaseResult result) {
    if (_rbBootRebaseResult == RbRebaseResult::Pending) {
        _rbBootRebaseResult = result;
    }
    _rbLastRebaseResult = result;
    _rbLastRebaseAtMs = millis();
    ++_rbQuietRebaseCount;
}

void Node::handleSelfTestCommand(const char* line) {
    const selftest::Command command = selftest::parseCommand(line);
    if (command.kind == selftest::Command::Kind::Help || command.kind == selftest::Command::Kind::Unknown) {
        selftest::printHelp(Serial, true);
        return;
    }
    if (command.kind == selftest::Command::Kind::None) {
        return;
    }
    if (_selfTest.phase != SelfTestState::Phase::Idle) {
        Serial.println("SELFTEST busy");
        return;
    }

    const unsigned long now = millis();
    _selfTest = SelfTestState{};
    _selfTest.command = command;
    _selfTest.phaseStartMs = now;

    switch (command.kind) {
        case selftest::Command::Kind::Full:
            selftest::printBegin(Serial, "node", "full");
            selftest::printBoard(Serial, _selfTest.tally, "node");
            _selfTest.phase = SelfTestState::Phase::QuietBoot;
            _selfTest.deadlineMs = now + kQuietBootWaitMs;
            break;
        case selftest::Command::Kind::External:
            selftest::printBegin(Serial, "node", "external");
            selftest::printBoard(Serial, _selfTest.tally, "node");
            _selfTest.phase = SelfTestState::Phase::Settle;
            _selfTest.deadlineMs = now + kOutputsWaitMs;
            break;
        case selftest::Command::Kind::Chirp:
            _selfTest.phase = SelfTestState::Phase::Settle;
            _selfTest.deadlineMs = now + kOutputsWaitMs;
            break;
        default:
            break;
    }
}

void Node::updateSelfTest(unsigned long now) {
    SelfTestState& st = _selfTest;
    switch (st.phase) {
        case SelfTestState::Phase::Idle:
            return;

        case SelfTestState::Phase::QuietBoot: {
            const bool pending = _rbLastRebaseResult == RbRebaseResult::Pending
                || _rbBaselineState == RBBaselineState::Boot
                || _rbBaselineState == RBBaselineState::ListenForQuiet
                || _rbBaselineState == RBBaselineState::Rebase;
            if (pending && !deadlinePassed(now, st.deadlineMs)) {
                return;
            }
            printSelfTestQuietBoot();
            st.phase = SelfTestState::Phase::Hardware;
            return;
        }

        case SelfTestState::Phase::Hardware:
            runSelfTestHardware(now);
            st.phase = SelfTestState::Phase::Settle;
            st.deadlineMs = millis() + kOutputsWaitMs;
            return;

        case SelfTestState::Phase::Settle:
            if (!rbOutputsEnabled() && !deadlinePassed(now, st.deadlineMs)) {
                return;
            }
            if (st.command.kind == selftest::Command::Kind::External) {
                Serial.print("SELFTEST_EXTERNAL waiting wait_ms=");
                Serial.println(st.command.waitMs);
                st.phase = SelfTestState::Phase::External;
                st.phaseStartMs = now;
                st.deadlineMs = now + st.command.waitMs;
                st.otherVerdicts = 0;
                st.otherValid = 0;
                st.responses = 0;
                st.firstOtherValidMs = 0;
                return;
            }
            if (st.command.kind == selftest::Command::Kind::Chirp) {
                Serial.print("SELFTEST_CHIRP start n=");
                Serial.print(st.command.chirps);
                Serial.print(" freq_hz=");
                Serial.print(_chirpOutput.toneHz());
                Serial.print(" path=behavior gap_ms=");
                Serial.println(st.command.gapMs);
            }
            startSelfTestChirps(SelfTestState::Phase::OwnChirp, now);
            return;

        case SelfTestState::Phase::OwnChirp:
            driveSelfTestChirps(now);
            return;

        case SelfTestState::Phase::OwnChirpTail:
            if (deadlinePassed(now, st.deadlineMs)) {
                finishSelfTestOwnChirp(false);
            }
            return;

        case SelfTestState::Phase::External:
            if (deadlinePassed(now, st.deadlineMs)) {
                finishSelfTestExternal(now);
            }
            return;

        case SelfTestState::Phase::ExternalTail:
            if (st.responses > 0 || deadlinePassed(now, st.deadlineMs)) {
                finishSelfTestExternal(now);
            }
            return;
    }
}

void Node::printSelfTestQuietBoot() {
    const uint8_t last = static_cast<uint8_t>(_rbLastRebaseResult);
    selftest::Result result = selftest::Result::Pass;
    const char* reason = nullptr;
    if (_rbLastRebaseResult == RbRebaseResult::NoQuiet) {
        result = selftest::Result::Fail;
        reason = "no_quiet";
    } else if (_rbLastRebaseResult == RbRebaseResult::Pending) {
        result = selftest::Result::Fail;
        reason = "timeout";
    }
    selftest::begin(Serial, _selfTest.tally, "quiet_boot", result);
    if (reason != nullptr) {
        selftest::field(Serial, "reason", reason);
    }
    selftest::field(Serial, "state", rbBaselineStateName());
    selftest::field(Serial, "boot", rebaseResultName(static_cast<uint8_t>(_rbBootRebaseResult)));
    selftest::field(Serial, "last", rebaseResultName(last));
    selftest::field(Serial, "last_at_ms", _rbLastRebaseAtMs);
    selftest::field(Serial, "smooth_at_rebase", _rbLastRebaseSmooth, 1);
    selftest::field(Serial, "quiet_rebases", _rbQuietRebaseCount);
    selftest::field(Serial, "forced_rebases", _rbForcedRebaseCount);
    selftest::field(Serial, "baseline", _audioSignal.baseline(), 1);
    selftest::end(Serial);
}

void Node::runSelfTestHardware(unsigned long now) {
    // Nothing of ours may sound while the mic floor is measured.
    if (_chirpOutput.isActive()) {
        _chirpOutput.stop();
        _behavior.notifyChirpFinished(now);
        _debug.observeChirpFinished(now);
        selfTestNoteChirpFinished(now);
    }

    selftest::runMicCheck(Serial, _selfTest.tally, _i2sSource);
#if defined(BOARD_PIEZO)
    const char* path = "piezo";
#else
    const char* path = "i2s";
#endif
    selftest::runAmpCheck(Serial, _selfTest.tally, _i2sSource, _chirpOutput.toneOutput(),
                          _chirpOutput.toneHz(), path);
    restoreAfterSelfTestHardware();
}

void Node::restoreAfterSelfTestHardware() {
    // The tone is off; put the output device back on the chirp frequency.
    _chirpOutput.stop();
    _chirpOutput.setToneHz(_chirpOutput.toneHz());

    // The checks read several seconds of audio that never reached the
    // detection chain: restart it on fresh audio (same reset as a rebase,
    // without touching the AudioSignal baseline or the Behavior config).
    _audioSignal.resetSignalState();
    _detection.resetState();
    applyActiveDetectionProfile();
    _freqBandStream.resetState();
    _freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
    _freqBandStream.setTargetFrequencyHz(_chirpOutput.toneHz());
    _wasSelfChirpSuppressed = false;

    if (_rbBaselineState == RBBaselineState::Active || _rbBaselineState == RBBaselineState::Settle) {
        _rbBaselineState = RBBaselineState::Settle;
        _rbBaselineSettleUntilMs = millis() + kSettleAfterHardwareMs;
    }
    _debug.markLoopStart(micros());
    Serial.print("SELFTEST_RESTORE detection=reset baseline=");
    Serial.println(rbBaselineStateName());
}

void Node::startSelfTestChirps(SelfTestState::Phase phase, unsigned long now) {
    SelfTestState& st = _selfTest;
    st.phase = phase;
    st.phaseStartMs = now;
    st.chirpsRequested = 0;
    st.chirpsStarted = 0;
    st.chirpsFinished = 0;
    st.waitingForChirp = false;
    st.nextChirpAtMs = now;
    st.ownVerdicts = 0;
    st.ownValid = 0;
    st.otherVerdicts = 0;
    st.otherValid = 0;
    const unsigned long perChirp = st.command.gapMs + 1000UL + kChirpStartTimeoutMs;
    st.deadlineMs = now + static_cast<unsigned long>(st.command.chirps) * perChirp + 5000UL;
}

void Node::driveSelfTestChirps(unsigned long now) {
    SelfTestState& st = _selfTest;
    if (st.chirpsFinished >= st.command.chirps) {
        if (st.command.kind == selftest::Command::Kind::Chirp) {
            Serial.print("SELFTEST_CHIRP done n=");
            Serial.print(st.command.chirps);
            Serial.print(" started=");
            Serial.println(st.chirpsStarted);
            finishSelfTest();
            return;
        }
        st.phase = SelfTestState::Phase::OwnChirpTail;
        st.deadlineMs = now + kOwnTailMs;
        return;
    }
    if (deadlinePassed(now, st.deadlineMs)) {
        if (st.command.kind == selftest::Command::Kind::Chirp) {
            Serial.print("SELFTEST_CHIRP done n=");
            Serial.print(st.command.chirps);
            Serial.print(" started=");
            Serial.print(st.chirpsStarted);
            Serial.println(" reason=timeout");
            finishSelfTest();
            return;
        }
        finishSelfTestOwnChirp(true);
        return;
    }
    if (!st.waitingForChirp && st.chirpsRequested < st.command.chirps
        && deadlinePassed(now, st.nextChirpAtMs) && rbOutputsEnabled()) {
        _behavior.requestTestChirp();
        ++st.chirpsRequested;
        st.waitingForChirp = true;
    }
}

void Node::finishSelfTestOwnChirp(bool timedOut) {
    SelfTestState& st = _selfTest;
    const char* reason = nullptr;
    if (st.ownVerdicts > 0) {
        reason = "own_verdict";
    } else if (timedOut || st.chirpsStarted < st.command.chirps) {
        reason = "chirp_not_started";
    }
    selftest::begin(Serial, st.tally, "own_chirp_suppressed",
                    reason == nullptr ? selftest::Result::Pass : selftest::Result::Fail);
    if (reason != nullptr) {
        selftest::field(Serial, "reason", reason);
    }
    selftest::field(Serial, "chirps", static_cast<unsigned long>(st.command.chirps));
    selftest::field(Serial, "started", static_cast<unsigned long>(st.chirpsStarted));
    selftest::field(Serial, "verdicts", st.ownVerdicts);
    selftest::field(Serial, "valid", st.ownValid);
    selftest::field(Serial, "other_verdicts", st.otherVerdicts);
    selftest::field(Serial, "other_valid", st.otherValid);
    selftest::field(Serial, "echo_window_ms", kEchoWindowMs);
    selftest::field(Serial, "suppress_tail_ms", _behavior.detectionSuppressTailMsOwnEmit());
    selftest::end(Serial);

    selftest::skip(Serial, st.tally, "external_chirp", "not_requested");
    finishSelfTest();
}

void Node::finishSelfTestExternal(unsigned long now) {
    SelfTestState& st = _selfTest;
    if (st.otherValid > 0) {
        selftest::begin(Serial, st.tally, "external_chirp", selftest::Result::Pass);
    } else {
        selftest::begin(Serial, st.tally, "external_chirp", selftest::Result::Skip);
        selftest::field(Serial, "reason", "no_external_chirp");
    }
    selftest::field(Serial, "wait_ms", static_cast<unsigned long>(st.command.waitMs));
    selftest::field(Serial, "latency_ms",
                    st.firstOtherValidMs != 0 ? st.firstOtherValidMs - st.phaseStartMs : 0UL);
    selftest::field(Serial, "valid", st.otherValid);
    selftest::field(Serial, "verdicts", st.otherVerdicts);
    selftest::field(Serial, "own_verdicts", st.ownVerdicts);
    selftest::field(Serial, "responded", st.responses > 0 ? 1UL : 0UL);
    selftest::field(Serial, "baseline", rbBaselineStateName());
    selftest::end(Serial);
    (void)now;
    finishSelfTest();
}

void Node::finishSelfTest() {
    // A request Behavior never took (timeout) must not fire later.
    _behavior.cancelTestChirp();
    if (_selfTest.command.kind != selftest::Command::Kind::Chirp) {
        selftest::printSummary(Serial, _selfTest.tally, "node");
    }
    _selfTest.phase = SelfTestState::Phase::Idle;
}

bool Node::selfTestVerdictIsOwn(unsigned long heardMs) const {
    const SelfTestState& st = _selfTest;
    for (uint8_t i = 0; i < st.windowCount; ++i) {
        const unsigned long start = st.chirpStartMs[i];
        const unsigned long end = st.chirpEndMs[i] != 0 ? st.chirpEndMs[i] : start + kOpenChirpSpanMs;
        if (static_cast<long>(heardMs - (start - 20UL)) >= 0
            && static_cast<long>(heardMs - (end + kEchoWindowMs)) <= 0) {
            return true;
        }
    }
    return false;
}

void Node::selfTestObserveVerdict(const OccurrenceVerdict& verdict, unsigned long now) {
    SelfTestState& st = _selfTest;
    const bool watching = st.phase == SelfTestState::Phase::OwnChirp
        || st.phase == SelfTestState::Phase::OwnChirpTail
        || st.phase == SelfTestState::Phase::External
        || st.phase == SelfTestState::Phase::ExternalTail;
    if (!watching) {
        return;
    }
    const unsigned long heardMs = verdict.primaryHeardAtMs != 0 ? verdict.primaryHeardAtMs
        : (verdict.primaryStartMs != 0 ? verdict.primaryStartMs : now);
    if (selfTestVerdictIsOwn(heardMs)) {
        ++st.ownVerdicts;
        if (verdict.valid) {
            ++st.ownValid;
        }
        return;
    }
    ++st.otherVerdicts;
    if (!verdict.valid) {
        return;
    }
    ++st.otherValid;
    if (st.phase == SelfTestState::Phase::External && st.firstOtherValidMs == 0) {
        st.firstOtherValidMs = now;
        st.phase = SelfTestState::Phase::ExternalTail;
        st.deadlineMs = now + kResponseWaitMs;
    }
}

void Node::selfTestNoteChirpStarted(unsigned long now, const char* sourceName) {
    SelfTestState& st = _selfTest;
    if (st.phase == SelfTestState::Phase::Idle) {
        return;
    }
    st.chirpStartMs[st.windowNext] = now;
    st.chirpEndMs[st.windowNext] = 0;
    st.windowNext = static_cast<uint8_t>((st.windowNext + 1) % SelfTestState::kWindows);
    if (st.windowCount < SelfTestState::kWindows) {
        ++st.windowCount;
    }
    const bool selfTestChirp = sourceName != nullptr && strcmp(sourceName, "selftest") == 0;
    st.selfTestChirpSounding = selfTestChirp;
    if (selfTestChirp && st.phase == SelfTestState::Phase::OwnChirp) {
        ++st.chirpsStarted;
    }
    const bool response = sourceName != nullptr && strcmp(sourceName, "heard_pattern") == 0;
    if (response && (st.phase == SelfTestState::Phase::External || st.phase == SelfTestState::Phase::ExternalTail)) {
        ++st.responses;
    }
}

void Node::selfTestNoteChirpFinished(unsigned long now) {
    SelfTestState& st = _selfTest;
    if (st.phase == SelfTestState::Phase::Idle || st.windowCount == 0) {
        return;
    }
    const uint8_t last = static_cast<uint8_t>((st.windowNext + SelfTestState::kWindows - 1) % SelfTestState::kWindows);
    if (st.chirpEndMs[last] == 0) {
        st.chirpEndMs[last] = now;
    }
    const bool selfTestChirp = st.selfTestChirpSounding;
    st.selfTestChirpSounding = false;
    if (st.phase == SelfTestState::Phase::OwnChirp && st.waitingForChirp && selfTestChirp) {
        st.waitingForChirp = false;
        ++st.chirpsFinished;
        st.nextChirpAtMs = now + st.command.gapMs;
    }
}
