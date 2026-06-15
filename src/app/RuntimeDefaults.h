#pragma once

#include <stdint.h>

/*
RuntimeDefaults

Shared compile-time defaults used by runtime modes and output hardware.
These are defaults, not live profile state.
*/
namespace runtime {

constexpr uint32_t kDefaultChirpFrequencyHz = 3200UL;
constexpr unsigned long kDefaultChirpDurationMs = 100UL;

} // namespace runtime
