#pragma once

#include <ParamRegistry.h>

#include "../../app/NvsParamStore.h"
#include "../../app/ParamCommands.h"
#include "../../app/ParamIds.h"

class Print;

/*
EchoParams (ECHO-003, 2026-11-04)

The echo shell's param registry and its NVS store: the echo room's echo.* or Whispernet's whisper.* (each build
registers its own mode's), PARAM LIST / GET / SET / DUMP / SAVE / LOAD / RESET through the shared handler, and the
stored values at boot. The store is the Node's namespace under the modes' own ids (roadmap-echo.md ECHO-003 item
3): a Node save never lands in these fields and theirs never in the Node's; PARAM RESET in any mode clears all.
Owns: the registry and the store.
Does not: own the values (EchoApp's Gate, Whispernet's Values) or apply them (EchoApp::applyParams).
*/
class EchoParams {
public:
    EchoParams() { _registry.setGroupNamer(param::moduleNameForGroup); }

    param::ParamRegistry& registry() { return _registry; }
    // The fields hold the firmware defaults now (the boot order of PAR-010).
    void captureDefaults() { _registry.captureDefaults(); }
    void printRefusedAdds(Print& out) { param::printRefusedAdds(_registry, out); }
    param::ParamLoadReport loadStored(Print& out) { return param::loadStoredParams(_registry, _store, out); }
    param::ParamCommandResult handleCommand(const char* line, Print& out) {
        return param::handleParamCommand(line, _registry, &_store, out);
    }

private:
    param::ParamRegistry _registry;
    NvsParamStore _store;
};
