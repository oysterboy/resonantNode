#pragma once

/*
The Analyzer's link to the Emitter (AnalyzerEmitterControl.cpp). Was forward-
declared by hand in three files (review B7, step 11f).
*/

// How long the Analyzer waits for the Emitter to confirm a mode change.
constexpr unsigned long kEmitterModeAckTimeoutMs = 1500;

// Waits up to timeoutMs for a line from the Emitter that starts with expectedPrefix.
bool waitForEmitterAck(const char* expectedPrefix, unsigned long timeoutMs);
