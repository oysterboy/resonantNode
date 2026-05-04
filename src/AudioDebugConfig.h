#pragma once

// Shared switch for audio-path verbosity.
// Keep this false by default so timing-critical paths stay quiet unless a
// developer explicitly opts back in.
#ifndef AUDIO_VERBOSE_DEBUG
#define AUDIO_VERBOSE_DEBUG 0
#endif

// Optional loop-stress knobs for validation runs.
#ifndef TEST_LOOP_DELAY_MS
#define TEST_LOOP_DELAY_MS 0
#endif

#ifndef TEST_LOG_STRESS
#define TEST_LOG_STRESS 0
#endif

// Manual physical-setup label for run banners.
#ifndef TEST_SETUP_LABEL
#define TEST_SETUP_LABEL "default"
#endif
