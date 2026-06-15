#pragma once

#include <stdint.h>

/*
RuntimeDefaults

Shared compile-time defaults used by runtime modes, output hardware, and the
debug/test knobs that multiple subsystems still share.
These are defaults, not live profile state.
*/

#ifndef AUDIO_VERBOSE_DEBUG
#define AUDIO_VERBOSE_DEBUG 0
#endif

#ifndef RB_VERBOSE_DEBUG
#define RB_VERBOSE_DEBUG 0
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

namespace runtime {

constexpr uint32_t kDefaultChirpFrequencyHz = 3200UL;
constexpr unsigned long kDefaultChirpDurationMs = 100UL;

} // namespace runtime
