#pragma once

#include <string.h>
#include <strings.h>

/*
Which Node commands the radio carries (PAR-013, step 11a). Kept out of NetFrame.h, the wire format, which
should not know the Node's command names. Pure; host-tested in test/test_net_frame.
*/
namespace net {

// What travels by radio (owner 2026-10-25): every Node command, PARAM, RB and
// SELFTEST, plus NET PING; ECHO for the echo modes' shell (ECHO-003: ECHO STATUS on a
// board running it; a Node answers it as an unknown command). Not the other NET commands (the radio does not
// address itself). Large replies are paced by the sender (NodeNet), so they
// cost the gateway no audio. Operator rules for the ones that sound, block or
// reset: docs/notes_manual.md, "Remote commands".
inline bool startsWithWord(const char* text, const char* word) {
    const size_t n = strlen(word);
    return strncasecmp(text, word, n) == 0 && (text[n] == '\0' || text[n] == ' ');
}

inline bool remoteCommandAllowed(const char* text) {
    if (text == nullptr) {
        return false;
    }
    if (startsWithWord(text, "PARAM") || startsWithWord(text, "SELFTEST") || startsWithWord(text, "OTA") ||
        startsWithWord(text, "ECHO")) {
        return true;
    }
    if (strncasecmp(text, "RB ", 3) == 0) {
        return true;
    }
    return strcasecmp(text, "NET PING") == 0;
}

// SELFTEST blocks its node for seconds and plays tones: by radio one named
// node at a time, never "*" (owner 2026-10-25; the others silent is the
// operator's part).
inline bool remoteCommandNeedsOneTarget(const char* text) {
    return text != nullptr && startsWithWord(text, "SELFTEST");
}

}  // namespace net
