// Node command dispatch (owner 2026-10-25 split by role): serial line assembly and the one
// dispatch every transport feeds (serial here, the radio in ResonantNodeNet.cpp, VEKTOR
// later; a CommandRouter class is NODE-005). Moved verbatim from ResonantNodeApp.cpp.
#include "ResonantNodeApp.h"
#include "../../app/FirmwareOta.h"
#include <Arduino.h>
#include <string.h>
#include <ParamRegistry.h>
#include "../../app/ParamIds.h"
#include "../../app/TimeBase.h"
#include "../../app/SerialOut.h"
#include "../../app/TextParse.h"
#include "ResonantNodeInternal.h"

using namespace node_internal;

void Node::pollSerialCommands() {
    while (Serial.available() > 0) {
        if (!_serialLine.feed(static_cast<char>(Serial.read()))) {
            continue;
        }
        if (_serialLine.tooLong()) {
            serialOut().print("RB ERR line_too_long max=");
            serialOut().println(static_cast<unsigned long>(_serialLine.kCapacity));
        } else if (_serialLine.length() > 0) {
            handleSerialLine(_serialLine.line());
        }
    }
}

void Node::handleSerialLine(const char* line) {
    if (startsWithTokenIgnoreCase(line, "NET")) {
        handleNetCommand(line);
        return;
    }
    if (startsWithTokenIgnoreCase(line, "OTA")) {
        // Firmware over WiFi (step 11d): silence, pull, reboot; on failure the old image runs on and the radio goes
        // back to its channel.
        _soundOutput.stop();
        ota::runCommand(line, serialOut());
        if (_radioEnabled) {
            _net.restoreChannel();
        }
        return;
    }
    if (equalsIgnoreCase(line, "RB help")) {
        serialOut().println("RB CMD: PARAM SET behavior.<key> <value> (Behavior timing, rest and idle call; PARAM LIST)");
        serialOut().println("RB CMD: RB STATUS");
        serialOut().println("RB CMD: RB PROFILE name=TonalPulseScalar");
        serialOut().println("RB CMD: RB PROFILE name=AmpExperimental");
        serialOut().println("RB CMD: RB rebase");
        serialOut().println("RB CMD: RB rebase force");
        serialOut().println("RB CMD: RB CALL (play the node's network signal now; a kick for multi-node runs)");
        serialOut().println("RB CMD: RB log off|minimal|full");
        serialOut().println("RB CMD: RB summary");
        serialOut().println("RB CMD: RB stop");
        serialOut().println("RB CMD: PARAM LIST");
        serialOut().println("RB CMD: PARAM GET <path>");
        serialOut().println("RB CMD: PARAM SET <path> <value>");
        serialOut().println("RB CMD: PARAM DUMP | PARAM SAVE | PARAM LOAD | PARAM RESET (NVS store)");
        serialOut().println("RB CMD: NET <target|*> <command> (PARAM, RB, SELFTEST to one target, NET PING) | NET STATUS (by radio)");
        serialOut().println("RB CMD: SELFTEST | SELFTEST external wait_ms=10000 | SELFTEST chirp n=5 | SELFTEST help");
        return;
    }
    if (startsWithTokenIgnoreCase(line, "SELFTEST")) {
        _selfTest.handleCommand(line, timebase::nowMs());
        return;
    }
    if (startsWithTokenIgnoreCase(line, "RB PROFILE")) {
        handleProfileCommand(line);
        return;
    }
    if (startsWithTokenIgnoreCase(line, "RB STATUS")) {
        printRbStatus(line);
        return;
    }
    if (startsWithTokenIgnoreCase(line, "RB PARAM")) {
        // RB PARAM tuned only the FrequencyMatch thresholds, removed 2026-10-22
        // (docs/decisions/2026-10-22-remove-frequencymatch.md). Detection params
        // are set through the registry.
        serialOut().println("RB PARAM ERR removed (FrequencyMatch removed); use PARAM SET detection.scalar_<key> <value>");
        return;
    }
    if (startsWithTokenIgnoreCase(line, "RB BEHAV")) {
        // RB BEHAV was removed in step 10 (decisions/2026-10-22-live-params-
        // profile-reset-no-rb-behav.md): Behavior's values are behavior.*
        // params; PARAM DUMP shows them, PARAM SET changes them.
        serialOut().println("RB BEHAV ERR removed; use PARAM SET behavior.<key> <value> (PARAM LIST shows the keys)");
        return;
    }
    if (startsWithTokenIgnoreCase(line, "PARAM")) {
        handleParamCommand(line);
        return;
    }
    if (startsWithTokenIgnoreCase(line, "RB rebase force")) {
        // A rebase stops and resets what SELFTEST is waiting on (issue #31
        // R2-13): finish or wait out the self-test first.
        if (_selfTest.active()) {
            serialOut().println("RB rebase refused busy=selftest");
            return;
        }
        performRbRebase();
        _baseline.noteForcedRebase(timebase::nowMs());
        serialOut().println("RB rebase forced");
        return;
    }
    if (startsWithTokenIgnoreCase(line, "RB rebase")) {
        // Only a bare "RB rebase" (or "force", above): "RB rebase forced"
        // used to start a quiet-gated rebase (issue #32 check 17).
        const char* extra = line + strlen("RB rebase");
        while (*extra == ' ') {
            ++extra;
        }
        if (*extra != '\0') {
            serialOut().print("RB ERR rebase unknown token=");
            serialOut().println(extra);
            return;
        }
        if (_selfTest.active()) {
            serialOut().println("RB rebase refused busy=selftest");
            return;
        }
        startRbQuietBaseline();
        serialOut().println("RB rebase requested quiet-gated");
        return;
    }
    if (startsWithTokenIgnoreCase(line, "RB log")) {
        handleLogCommand(line);
        return;
    }
    if (equalsIgnoreCase(line, "RB CALL") || equalsIgnoreCase(line, "RB call")) {
        // Step 5g: play the node's network signal now, through Behavior (kick).
        if (_silent) {
            serialOut().println("RB CALL ignored silent=1");
            return;
        }
        _behavior.requestNetworkCall();
        serialOut().print("RB CALL queued resting=");
        serialOut().print(_behavior.resting(timebase::nowMs()) ? 1 : 0);
        serialOut().print(" state=");
        serialOut().println(_behavior.stateName());
        return;
    }
    if (equalsIgnoreCase(line, "RB summary") || equalsIgnoreCase(line, "RB stop")) {
        printRbSummary();
        return;
    }
    // Anything else gets one line, so a typo is not mistaken for a lost command (review D1.9).
    serialOut().println("RB ERR unknown_command");
}

void Node::handleLogCommand(const char* line) {
    const char* mode = text_parse::rest(line, "RB log");

    if (equalsIgnoreCase(mode, "off")) {
        _rbLogMode = RbLogMode::Off;
    } else if (equalsIgnoreCase(mode, "minimal")) {
        _rbLogMode = RbLogMode::Minimal;
    } else if (equalsIgnoreCase(mode, "full")) {
        _rbLogMode = RbLogMode::Full;
    } else {
        serialOut().print("RB log mode=");
        serialOut().print(rbLogModeName());
        serialOut().println(" (use off|minimal|full)");
        return;
    }

    serialOut().print("RB log mode=");
    serialOut().println(rbLogModeName());
}

void Node::handleProfileCommand(const char* line) {
    const char* name = text_parse::rest(line, "RB PROFILE");

    if (*name == '\0') {
        serialOut().print("RB PROFILE name=");
        serialOut().println(profileName());
        return;
    }

    if (startsWithTokenIgnoreCase(name, "name=")) {
        name = text_parse::tokenValue(name);
    }

    if (setProfileFromName(name)) {
        serialOut().print("RB PROFILE name=");
        serialOut().print(profileName());
        printProfileComposition(activeDetectionProfile());
        serialOut().println();
    } else {
        serialOut().println("RB PROFILE usage=name=TonalPulseScalar|AmpExperimental");
    }
}

const char* Node::rbLogModeName() const {
    switch (_rbLogMode) {
        case RbLogMode::Off:
            return "off";
        case RbLogMode::Minimal:
            return "minimal";
        case RbLogMode::Full:
            return "full";
    }
    return "off";
}
