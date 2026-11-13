// Node command dispatch: serial line assembly and the one
// dispatch every transport feeds (serial here, the radio in ResonantNodeNet.cpp, VEKTOR
// later; a CommandRouter class is NODE-005): the kCommands table and its handlers.
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

using text_parse::equalsIgnoreCase;
using text_parse::startsWithWordIgnoreCase;

void Node::pollSerialCommands() {
    serial_line::pollLines(Serial, _serialLine, serialOut(),
                           "RB ERR line_too_long max=", [this](const char* line) { handleSerialLine(line); });
}

// The Node's commands, one row per verb: the first row whose verb matches handles the line. Word: the
// verb is a prefix ending on a token boundary (text_parse::startsWithWordIgnoreCase); Exact: the whole line, any
// case. The order matters where verbs share a prefix ("RB rebase force" before
// "RB rebase"). Shaped for the later CommandRouter (NODE-005): a move of the rows, not a redesign.
const Node::Command Node::kCommands[] = {
    {"NET", Command::Match::Word, &Node::handleNetCommand},
    {"OTA", Command::Match::Word, &Node::handleOtaCommand},
    {"RB help", Command::Match::Exact, &Node::printHelp},
    {"SELFTEST", Command::Match::Word, &Node::handleSelfTestCommand},
    {"LIGHT", Command::Match::Word, &Node::handleLightCommand},
    {"RB PROFILE", Command::Match::Word, &Node::handleProfileCommand},
    {"RB STATUS", Command::Match::Word, &Node::printRbStatus},
    {"RB PARAM", Command::Match::Word, &Node::refuseRbParam},
    {"RB BEHAV", Command::Match::Word, &Node::refuseRbBehav},
    {"PARAM", Command::Match::Word, &Node::handleParamCommand},
    {"RB rebase force", Command::Match::Word, &Node::handleRebaseForceCommand},
    {"RB rebase", Command::Match::Word, &Node::handleRebaseCommand},
    {"RB log", Command::Match::Word, &Node::handleLogCommand},
    {"RB CALL", Command::Match::Exact, &Node::handleCallCommand},
    {"RB summary", Command::Match::Exact, &Node::handleSummaryCommand},
    {"RB stop", Command::Match::Exact, &Node::handleSummaryCommand},
};

void Node::handleSerialLine(const char* line) {
    for (const Command& command : kCommands) {
        const bool matches = command.match == Command::Match::Exact ? equalsIgnoreCase(line, command.verb)
                                                                    : startsWithWordIgnoreCase(line, command.verb);
        if (matches) {
            (this->*command.handler)(line);
            return;
        }
    }
    // Anything else gets one line, so a typo is not mistaken for a lost command.
    serialOut().println("RB ERR unknown_command");
}

void Node::handleOtaCommand(const char* line) {
    // Firmware over WiFi (decisions/2026-10-28-ota-firmware-pull-over-http.md): silence, pull, reboot; on failure
    // the old image runs on and the radio goes back to its channel. A sounding emission is closed as
    // runSelfTestHardware closes it (review 12.0 H1): Behavior leaves Chirping, the own-chirp time and the self-test
    // see the end, so a failed OTA does not leave the Node deaf.
    if (_soundOutput.busy() || _chirpOutput.isActive()) {
        _soundOutput.stop();
        const unsigned long now = timebase::nowMs();
        _lastOwnChirpEndMs = now;
        _behavior.notifyChirpFinished(now);
        _selfTest.noteChirpFinished(now);
    }
    _soundOutput.stop();  // also an idle call waiting in its gap
    if (_net.started()) {
        // A NET * OTA from this node is still in the outbox (and a remote OTA's reply): send it before the WiFi join
        // takes the radio off its channel (found on the bench 2026-10-29: E4 / E5 never got the broadcast).
        _net.flush(1500);
    }
    ota::runCommand(line, serialOut());
    if (_switches.radioEnabled) {
        _net.restoreChannel();
    }
}

void Node::printHelp(const char* line) {
    (void)line;
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
    serialOut().println(
        "RB CMD: NET <target|*> <command> (PARAM, RB, SELFTEST to one target, NET PING) | NET STATUS (by radio)");
    serialOut().println("RB CMD: SELFTEST | SELFTEST external wait_ms=10000 | SELFTEST chirp n=5 | SELFTEST help");
    serialOut().println(
        "RB CMD: LIGHT <0..100> | LIGHT off | LIGHT alt | LIGHT alt4 | LIGHT rgb <r> <g> <b> | LIGHT rgbw <r> <g> <b> <w> | LIGHT w <0..100>");
}

// The light test (issue #94): one colour on all pixels of the strip on LIGHT_PIN, under the build-time current cap.
// rgb: 3 bytes per pixel (WS2812); rgbw / w: 4 bytes per pixel (SK6812 RGBW), so a W-only frame shows which kind a
// strip is. A plain number is the strip's white (LIGHT_WHITE_*) at a perceived level: 100 is the brightest the cap
// allows, 50 looks about half as bright (decisions/2026-11-13-light-level-scalar-cap-in-hal.md); rgb / rgbw stay raw
// bytes, capped. hsb h s b: a colour as the light's effects give it (hue degrees, saturation and brightness in %).
// Every one of these holds the strip until LIGHT auto hands it back to the effects (NodeLight).
void Node::handleLightCommand(const char* line) {
    if (!_light.ready()) {
        serialOut().println("LIGHT ERR no_light (LIGHT_PIN < 0 in this build, or the RMT driver failed)");
        return;
    }
    const char* args = text_parse::rest(line, "LIGHT");
    if (equalsIgnoreCase(args, "auto")) {
        _lightManual = false;
        _lightFx.reset();  // the effects start dark, from Behavior's state now
        serialOut().println("LIGHT auto ok");
        return;
    }
    int a = 0, b = 0, c = 0, d = 0;
    if (sscanf(args, "hsb %d %d %d", &a, &b, &c) == 3 && a >= 0 && a <= 359 && b >= 0 && b <= 100 && c >= 0 &&
        c <= 100) {
        _lightManual = true;
        const bool shown = _light.showHsb(static_cast<float>(a), b / 100.0f, c / 100.0f);
        serialOut().print("LIGHT ");
        serialOut().print(shown ? "ok" : "ERR busy");
        serialOut().print(" hsb sent=");
        serialOut().print(_light.sentR());
        serialOut().print(",");
        serialOut().print(_light.sentG());
        serialOut().print(",");
        serialOut().print(_light.sentB());
        serialOut().print(" cap_ma=");
        serialOut().println(_light.capMa());
        return;
    }
    bool ok = false;
    bool four = false;
    uint8_t r = 0, g = 0, bl = 0, w = 0;
    int levelPct = -1;  // >= 0: a perceived level of the white (or the W channel alone)
    auto inByte = [](int v) { return v >= 0 && v <= 255; };
    // alt / alt4: even pixels red, odd pixels blue (3 bytes per pixel) or white-only (4 bytes), so a strip of the other
    // kind, or a colour order that is not GRB, shows per pixel.
    const uint8_t altRed[4] = {255, 0, 0, 0};
    const uint8_t altBlue[4] = {0, 0, 255, 0};
    const uint8_t altWhite[4] = {0, 0, 0, 255};
    bool alternating = false;
    if (equalsIgnoreCase(args, "off")) {
        ok = true;
    } else if (equalsIgnoreCase(args, "alt") || equalsIgnoreCase(args, "alt4")) {
        four = equalsIgnoreCase(args, "alt4"), alternating = true, ok = true;
    } else if (sscanf(args, "rgbw %d %d %d %d", &a, &b, &c, &d) == 4 && inByte(a) && inByte(b) && inByte(c) &&
               inByte(d)) {
        r = a, g = b, bl = c, w = d, four = true, ok = true;
    } else if (sscanf(args, "rgb %d %d %d", &a, &b, &c) == 3 && inByte(a) && inByte(b) && inByte(c)) {
        r = a, g = b, bl = c, ok = true;
    } else if (sscanf(args, "w %d", &a) == 1 && a >= 0 && a <= 100) {
        w = 255, levelPct = a, four = true, ok = true;
    } else if (sscanf(args, "%d", &a) == 1 && a >= 0 && a <= 100 && args[0] >= '0' && args[0] <= '9') {
        r = LIGHT_WHITE_R, g = LIGHT_WHITE_G, bl = LIGHT_WHITE_B, levelPct = a, ok = true;
    }
    if (!ok) {
        serialOut().println(
            "LIGHT ERR usage: LIGHT <0..100> | off | alt | alt4 | rgb r g b | rgbw r g b w | w <0..100> | "
            "hsb <0..359> <0..100> <0..100> | auto");
        return;
    }
    _lightManual = true;
    const uint8_t colour[4] = {r, g, bl, w};
    const bool sent = alternating ? _light.showAlternating(altRed, four ? altWhite : altBlue, four ? 4 : 3)
        : levelPct >= 0           ? _light.showLevel(levelPct / 100.0f, colour, four ? 4 : 3)
        : four                    ? _light.showRgbw(r, g, bl, w)
                                  : _light.showRgb(r, g, bl);
    serialOut().print("LIGHT ");
    serialOut().print(sent ? "ok" : "ERR send_failed");
    if (levelPct >= 0) {
        serialOut().print(" level_pct=");
        serialOut().print(levelPct);
    }
    serialOut().print(" bytes_per_pixel=");
    serialOut().print(four ? 4 : 3);
    serialOut().print(" pixels=");
    serialOut().print(_light.pixels());
    serialOut().print(" sent=");
    serialOut().print(_light.sentR());
    serialOut().print(",");
    serialOut().print(_light.sentG());
    serialOut().print(",");
    serialOut().print(_light.sentB());
    serialOut().print(",");
    serialOut().print(_light.sentW());
    serialOut().print(" est_ma=");
    serialOut().print(_light.estimatedMa());
    serialOut().print(" cap_ma=");
    serialOut().print(_light.capMa());
    serialOut().print(" scale_pct=");
    serialOut().println(_light.scalePct());
}

void Node::handleSelfTestCommand(const char* line) {
    _selfTest.handleCommand(line, timebase::nowMs());
}

void Node::refuseRbParam(const char* line) {
    (void)line;
    // RB PARAM tuned only the FrequencyMatch thresholds, removed 2026-10-22
    // (docs/decisions/2026-10-22-remove-frequencymatch.md). Detection params
    // are set through the registry.
    serialOut().println("RB PARAM ERR removed (FrequencyMatch removed); use PARAM SET detection.scalar_<key> <value>");
}

void Node::refuseRbBehav(const char* line) {
    (void)line;
    // RB BEHAV is gone (decisions/2026-10-22-live-params-
    // profile-reset-no-rb-behav.md): Behavior's values are behavior.*
    // params; PARAM DUMP shows them, PARAM SET changes them.
    serialOut().println("RB BEHAV ERR removed; use PARAM SET behavior.<key> <value> (PARAM LIST shows the keys)");
}

void Node::handleRebaseForceCommand(const char* line) {
    (void)line;
    // A rebase stops and resets what SELFTEST is waiting on (issue #31
    // R2-13): finish or wait out the self-test first.
    if (_selfTest.active()) {
        serialOut().println("RB rebase refused busy=selftest");
        return;
    }
    performRbRebase();
    _baseline.noteForcedRebase(timebase::nowMs());
    serialOut().println("RB rebase forced");
}

void Node::handleRebaseCommand(const char* line) {
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
}

void Node::handleCallCommand(const char* line) {
    (void)line;
    // Play the node's network signal now, through Behavior (kick).
    if (_switches.silent) {
        serialOut().println("RB CALL ignored silent=1");
        return;
    }
    _behavior.requestNetworkCall();
    const BehaviorStatus b = _behavior.status(timebase::nowMs());
    serialOut().print("RB CALL queued resting=");
    serialOut().print(b.resting ? 1 : 0);
    serialOut().print(" state=");
    serialOut().println(b.stateName);
}

void Node::handleSummaryCommand(const char* line) {
    (void)line;
    printRbSummary();
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

    if (startsWithWordIgnoreCase(name, "name=")) {
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
