#include "ResonantNodeSelfTest.h"

#include <string.h>

#include "../../app/TimingUtils.h"
#include "../../app/SerialOut.h"

/*
Node SELFTEST (NODE-015, docs/refactors/archive/selftest.md)

Sequence for "SELFTEST":
  board -> (waits while the startup rebase is still running) mic
  -> quiet_boot, amp (blocking, on the I2S source; detection is reset
     afterwards and the baseline goes through Settle again)
  A failed mic SKIPs quiet_boot, amp and own_chirp_suppressed
  (reason=mic_failed), a failed amp SKIPs own_chirp_suppressed
  (amp_failed): with the mic dead, a floating data line picks up the I2S
  TX and those checks would pass vacuously. The summary still FAILs.
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
The hardware checks (mic, amp) and the restore after them are Node glue
(Node::runSelfTestHardware in ResonantNodeApp.cpp, step 11i N4).
*/

namespace {
constexpr unsigned long kQuietBootWaitMs = 15000;   // boot warmup 2 s + quiet timeout 8 s + margin
constexpr unsigned long kOutputsWaitMs = 15000;
constexpr unsigned long kEchoWindowMs = 300;       // after an own chirp's finish
constexpr unsigned long kOpenChirpSpanMs = 2000;   // own chirp still sounding: assumed length
// An own echo is an occurrence that starts while the chirp sounds or right
// after it, once the own-emit suppression (whole chirp plus its tail) lets
// the mic through again; a margin covers the room tail. Another node's answer
// starts later: its verdict, its wait (waitAfterHeardMs) and its train come
// first. Judging by the occurrence's start, not when it was heard, keeps
// other Nodes' answers out of the own count (2026-10-17: with five Nodes on
// the bench they landed inside the 300 ms echo window and failed the check).
constexpr unsigned long kOwnEchoStartMarginMs = 50;
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

void NodeSelfTest::begin() {
    _st = State{};
}

void NodeSelfTest::handleCommand(const char* line, unsigned long now) {
    const selftest::Command command = selftest::parseCommand(line);
    if (command.kind == selftest::Command::Kind::Help || command.kind == selftest::Command::Kind::Unknown) {
        selftest::printHelp(serialOut(), true);
        return;
    }
    if (command.kind == selftest::Command::Kind::None) {
        return;
    }
    if (_st.phase != Phase::Idle) {
        serialOut().println("SELFTEST busy");
        return;
    }

    _st = State{};
    _st.command = command;
    _st.phaseStartMs = now;

    switch (command.kind) {
        case selftest::Command::Kind::Full:
            selftest::printBegin(serialOut(), "node", "full");
            selftest::printBoard(serialOut(), _st.tally, "node");
            _st.phase = Phase::QuietBoot;
            _st.deadlineMs = now + kQuietBootWaitMs;
            break;
        case selftest::Command::Kind::External:
            selftest::printBegin(serialOut(), "node", "external");
            selftest::printBoard(serialOut(), _st.tally, "node");
            _st.phase = Phase::Settle;
            _st.deadlineMs = now + kOutputsWaitMs;
            break;
        case selftest::Command::Kind::Chirp:
            _st.phase = Phase::Settle;
            _st.deadlineMs = now + kOutputsWaitMs;
            break;
        default:
            break;
    }
}

NodeSelfTest::Request NodeSelfTest::update(unsigned long now, const Inputs& in) {
    State& st = _st;
    switch (st.phase) {
        case Phase::Idle:
            return Request::None;

        case Phase::QuietBoot: {
            const bool pending = _baseline.lastResult() == NodeBaseline::RebaseResult::Pending ||
                _baseline.state() == NodeBaseline::State::Boot ||
                _baseline.state() == NodeBaseline::State::ListenForQuiet;
            if (pending && !deadlinePassed(now, st.deadlineMs)) {
                return Request::None;
            }
            st.phase = Phase::Hardware;
            return Request::None;
        }

        case Phase::Hardware:
            // The Node runs the checks, then hardwareDone() moves on to Settle.
            return Request::RunHardware;

        case Phase::Settle: {
            // A chirp from before SELFTEST started has no own-chirp window;
            // its echo would count as external (issue #31 R2-16). Wait for
            // it to stop and for kEchoWindowMs after.
            const bool ownChirpEcho = in.chirpActive || !timing::elapsedSince(now, in.lastOwnChirpEndMs, kEchoWindowMs);
            if ((!in.outputsEnabled || ownChirpEcho) && !deadlinePassed(now, st.deadlineMs)) {
                return Request::None;
            }
            if (st.command.kind == selftest::Command::Kind::External) {
                serialOut().print("SELFTEST_EXTERNAL waiting wait_ms=");
                serialOut().println(st.command.waitMs);
                st.phase = Phase::External;
                st.phaseStartMs = now;
                st.deadlineMs = now + st.command.waitMs;
                st.otherVerdicts = 0;
                st.otherValid = 0;
                st.responses = 0;
                st.firstOtherValidMs = 0;
                return Request::None;
            }
            if (st.command.kind == selftest::Command::Kind::Full &&
                (st.micResult != selftest::Result::Pass || st.ampResult != selftest::Result::Pass)) {
                selftest::skip(serialOut(), st.tally, "own_chirp_suppressed",
                               st.micResult != selftest::Result::Pass ? "mic_failed" : "amp_failed");
                selftest::skip(serialOut(), st.tally, "external_chirp", "not_requested");
                finish();
                return Request::None;
            }
            if (st.command.kind == selftest::Command::Kind::Chirp) {
                serialOut().print("SELFTEST_CHIRP start n=");
                serialOut().print(st.command.chirps);
                serialOut().print(" freq_hz=");
                serialOut().print(in.toneHz);
                serialOut().print(" path=behavior gap_ms=");
                serialOut().println(st.command.gapMs);
            }
            startChirps(Phase::OwnChirp, now);
            return Request::None;
        }

        case Phase::OwnChirp:
            driveChirps(now, in);
            return Request::None;

        case Phase::OwnChirpTail:
            if (deadlinePassed(now, st.deadlineMs)) {
                finishOwnChirp(false, in.suppressTailMs);
            }
            return Request::None;

        case Phase::External:
            if (deadlinePassed(now, st.deadlineMs)) {
                finishExternal(now);
            }
            return Request::None;

        case Phase::ExternalTail:
            if (st.responses > 0 || deadlinePassed(now, st.deadlineMs)) {
                finishExternal(now);
            }
            return Request::None;
    }
    return Request::None;
}

void NodeSelfTest::hardwareDone(selftest::Result mic, selftest::Result amp, unsigned long nowMs) {
    _st.micResult = mic;
    _st.ampResult = amp;
    _st.phase = Phase::Settle;
    _st.deadlineMs = nowMs + kOutputsWaitMs;
}

void NodeSelfTest::printQuietBoot(float audioBaseline) {
    const uint8_t last = static_cast<uint8_t>(_baseline.lastResult());
    selftest::Result result = selftest::Result::Pass;
    const char* reason = nullptr;
    if (_baseline.lastResult() == NodeBaseline::RebaseResult::NoQuiet) {
        result = selftest::Result::Fail;
        reason = "no_quiet";
    } else if (_baseline.lastResult() == NodeBaseline::RebaseResult::Pending) {
        result = selftest::Result::Fail;
        reason = "timeout";
    }
    selftest::begin(serialOut(), _st.tally, "quiet_boot", result);
    if (reason != nullptr) {
        selftest::field(serialOut(), "reason", reason);
    }
    selftest::field(serialOut(), "state", _baseline.stateName());
    selftest::field(serialOut(), "boot", rebaseResultName(static_cast<uint8_t>(_baseline.bootResult())));
    selftest::field(serialOut(), "last", rebaseResultName(last));
    selftest::field(serialOut(), "last_at_ms", _baseline.lastRebaseAtMs());
    selftest::field(serialOut(), "smooth_at_rebase", _baseline.lastRebaseSmooth(), 1);
    selftest::field(serialOut(), "quiet_rebases", _baseline.quietRebaseCount());
    selftest::field(serialOut(), "forced_rebases", _baseline.forcedRebaseCount());
    selftest::field(serialOut(), "baseline", audioBaseline, 1);
    selftest::end(serialOut());
}

void NodeSelfTest::startChirps(Phase phase, unsigned long now) {
    State& st = _st;
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

void NodeSelfTest::driveChirps(unsigned long now, const Inputs& in) {
    State& st = _st;
    if (st.chirpsFinished >= st.command.chirps) {
        if (st.command.kind == selftest::Command::Kind::Chirp) {
            serialOut().print("SELFTEST_CHIRP done n=");
            serialOut().print(st.command.chirps);
            serialOut().print(" started=");
            serialOut().println(st.chirpsStarted);
            finish();
            return;
        }
        st.phase = Phase::OwnChirpTail;
        st.deadlineMs = now + kOwnTailMs;
        return;
    }
    if (deadlinePassed(now, st.deadlineMs)) {
        if (st.command.kind == selftest::Command::Kind::Chirp) {
            serialOut().print("SELFTEST_CHIRP done n=");
            serialOut().print(st.command.chirps);
            serialOut().print(" started=");
            serialOut().print(st.chirpsStarted);
            serialOut().println(" reason=timeout");
            finish();
            return;
        }
        finishOwnChirp(true, in.suppressTailMs);
        return;
    }
    if (!st.waitingForChirp && st.chirpsRequested < st.command.chirps && deadlinePassed(now, st.nextChirpAtMs) &&
        in.outputsEnabled) {
        _chirps.request(_chirps.ctx);
        ++st.chirpsRequested;
        st.waitingForChirp = true;
    }
}

void NodeSelfTest::finishOwnChirp(bool timedOut, uint32_t suppressTailMs) {
    State& st = _st;
    const char* reason = nullptr;
    if (st.ownVerdicts > 0) {
        reason = "own_verdict";
    } else if (timedOut || st.chirpsStarted < st.command.chirps) {
        reason = "chirp_not_started";
    }
    selftest::begin(serialOut(), st.tally, "own_chirp_suppressed",
                    reason == nullptr ? selftest::Result::Pass : selftest::Result::Fail);
    if (reason != nullptr) {
        selftest::field(serialOut(), "reason", reason);
    }
    selftest::field(serialOut(), "chirps", static_cast<unsigned long>(st.command.chirps));
    selftest::field(serialOut(), "started", static_cast<unsigned long>(st.chirpsStarted));
    selftest::field(serialOut(), "verdicts", st.ownVerdicts);
    selftest::field(serialOut(), "valid", st.ownValid);
    selftest::field(serialOut(), "other_verdicts", st.otherVerdicts);
    selftest::field(serialOut(), "other_valid", st.otherValid);
    selftest::field(serialOut(), "echo_window_ms", kEchoWindowMs);
    selftest::field(serialOut(), "suppress_tail_ms", static_cast<unsigned long>(suppressTailMs));
    selftest::end(serialOut());

    selftest::skip(serialOut(), st.tally, "external_chirp", "not_requested");
    finish();
}

void NodeSelfTest::finishExternal(unsigned long now) {
    State& st = _st;
    if (st.otherValid > 0) {
        selftest::begin(serialOut(), st.tally, "external_chirp", selftest::Result::Pass);
    } else {
        selftest::begin(serialOut(), st.tally, "external_chirp", selftest::Result::Skip);
        selftest::field(serialOut(), "reason", "no_external_chirp");
    }
    selftest::field(serialOut(), "wait_ms", static_cast<unsigned long>(st.command.waitMs));
    selftest::field(serialOut(), "latency_ms",
                    st.firstOtherValidMs != 0 ? st.firstOtherValidMs - st.phaseStartMs : 0UL);
    selftest::field(serialOut(), "valid", st.otherValid);
    selftest::field(serialOut(), "verdicts", st.otherVerdicts);
    selftest::field(serialOut(), "own_verdicts", st.ownVerdicts);
    selftest::field(serialOut(), "responded", st.responses > 0 ? 1UL : 0UL);
    selftest::field(serialOut(), "baseline", _baseline.stateName());
    selftest::end(serialOut());
    (void)now;
    finish();
}

void NodeSelfTest::finish() {
    // A request Behavior never took (timeout) must not fire later.
    _chirps.cancel(_chirps.ctx);
    if (_st.command.kind != selftest::Command::Kind::Chirp) {
        selftest::printSummary(serialOut(), _st.tally, "node");
    }
    _st.phase = Phase::Idle;
}

bool NodeSelfTest::verdictIsOwn(unsigned long occurrenceStartMs, uint32_t suppressTailMs) const {
    const State& st = _st;
    const unsigned long ownStartLimitMs = suppressTailMs + kOwnEchoStartMarginMs;
    for (uint8_t i = 0; i < st.windowCount; ++i) {
        const unsigned long start = st.chirpStartMs[i];
        const unsigned long end = st.chirpEndMs[i] != 0 ? st.chirpEndMs[i] : start + kOpenChirpSpanMs;
        if (static_cast<long>(occurrenceStartMs - (start - 20UL)) >= 0 &&
            static_cast<long>(occurrenceStartMs - (end + ownStartLimitMs)) <= 0) {
            return true;
        }
    }
    return false;
}

void NodeSelfTest::observeVerdict(const detection::OccurrenceVerdict& verdict, unsigned long now,
                                  uint32_t suppressTailMs) {
    State& st = _st;
    const bool watching = st.phase == Phase::OwnChirp || st.phase == Phase::OwnChirpTail ||
        st.phase == Phase::External || st.phase == Phase::ExternalTail;
    if (!watching) {
        return;
    }
    const unsigned long startMs = verdict.primaryStartMs != 0
        ? verdict.primaryStartMs
        : (verdict.primaryHeardAtMs != 0 ? verdict.primaryHeardAtMs : now);
    if (verdictIsOwn(startMs, suppressTailMs)) {
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
    if (st.phase == Phase::External && st.firstOtherValidMs == 0) {
        st.firstOtherValidMs = now;
        st.phase = Phase::ExternalTail;
        st.deadlineMs = now + kResponseWaitMs;
    }
}

void NodeSelfTest::noteChirpStarted(unsigned long now, const char* sourceName) {
    State& st = _st;
    if (st.phase == Phase::Idle) {
        return;
    }
    st.chirpStartMs[st.windowNext] = now;
    st.chirpEndMs[st.windowNext] = 0;
    st.windowNext = static_cast<uint8_t>((st.windowNext + 1) % kWindows);
    if (st.windowCount < kWindows) {
        ++st.windowCount;
    }
    const bool selfTestChirp = sourceName != nullptr && strcmp(sourceName, "selftest") == 0;
    st.selfTestChirpSounding = selfTestChirp;
    if (selfTestChirp && st.phase == Phase::OwnChirp) {
        ++st.chirpsStarted;
    }
    const bool response = sourceName != nullptr && strcmp(sourceName, "heard_pattern") == 0;
    if (response && (st.phase == Phase::External || st.phase == Phase::ExternalTail)) {
        ++st.responses;
    }
}

void NodeSelfTest::noteChirpFinished(unsigned long now) {
    State& st = _st;
    if (st.phase == Phase::Idle || st.windowCount == 0) {
        return;
    }
    const uint8_t last = static_cast<uint8_t>((st.windowNext + kWindows - 1) % kWindows);
    if (st.chirpEndMs[last] == 0) {
        st.chirpEndMs[last] = now;
    }
    const bool selfTestChirp = st.selfTestChirpSounding;
    st.selfTestChirpSounding = false;
    if (st.phase == Phase::OwnChirp && st.waitingForChirp && selfTestChirp) {
        st.waitingForChirp = false;
        ++st.chirpsFinished;
        st.nextChirpAtMs = now + st.command.gapMs;
    }
}
