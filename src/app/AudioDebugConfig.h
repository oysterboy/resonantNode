#pragma once

/*
AudioDebugConfig

Shared debug and test defaults used across analyzer, detector, and resonant
mode code. This stays in the shared app-level layer because it is consumed by
multiple subsystems, not just Analyzer.

Responsibilities:
- gate verbose audio-path diagnostics
- provide optional loop-stress knobs for validation runs
- supply a default setup label for run banners

Does NOT:
- own runtime behavior
- select a mode
- change detector tuning on its own
*/

#ifndef AUDIO_VERBOSE_DEBUG
#define AUDIO_VERBOSE_DEBUG 0
#endif

#ifndef TEST_LOOP_DELAY_MS
#define TEST_LOOP_DELAY_MS 0
#endif

#ifndef TEST_LOG_STRESS
#define TEST_LOG_STRESS 0
#endif

#ifndef TEST_SETUP_LABEL
#define TEST_SETUP_LABEL "default"
#endif
