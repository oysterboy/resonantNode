#pragma once

#include <stdint.h>

#include <ParamRegistry.h>

class Print;

/*
ParamCommands (step 11e item 9)

The PARAM command surface, one handler for every mode: PARAM LIST | GET <path>
| SET <path> <value> | DUMP | SAVE | LOAD | RESET, printing the lines the Node
has printed since step 10 (`PARAM SET path=... old=... new=... status=...`;
tools/bench parses them). The Node calls it with its NVS store, the Analyzer
with no store (its tuning is per run).

The handler writes fields and prints; it does not know what a value means.
It returns what changed so the caller applies the owning module (the Node's
applyParamModule; the Analyzer applies at the next SEQ start and does nothing
here).
*/
namespace param {

enum class ParamCommandEffect : uint8_t {
    None,       // nothing written (LIST, GET, DUMP, SAVE, a refused SET)
    Group,      // SET wrote one field of `group`
    AllGroups,  // LOAD or RESET rewrote fields across groups
};

struct ParamCommandResult {
    ParamCommandEffect effect = ParamCommandEffect::None;
    uint8_t group = 0;  // the registry group tag (the app's ModuleId); valid for Group
};

// `line` starts at PARAM (the caller matched the verb) and is at most 255
// bytes, the LineReader's bound. `store` nullptr: the mode has no persistence;
// SAVE and LOAD answer status=no_store, RESET still restores the defaults.
ParamCommandResult handleParamCommand(const char* line, const ParamRegistry& registry, ParamStore* store, Print& out);

// The store's valid values into the fields, with the `PARAM load stored=...`
// line (PAR-010). Boot and PARAM LOAD share it; the caller applies.
ParamLoadReport loadStoredParams(const ParamRegistry& registry, ParamStore& store, Print& out);

}  // namespace param
