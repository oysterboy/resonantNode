#pragma once

#include <stddef.h>

#include <ParamRegistry.h>

#include "../../app/NvsParamStore.h"
#include "../../app/ParamCommands.h"
#include "../../app/ParamIds.h"
#include "../../behavior/BehaviorProfile.h"
#include "../../detection/detectors/scalar/ScalarTransientConfig.h"
#include "../../detection/features/NetworkBand.h"
#include "../../detection/inspection/InspectionLimits.h"
#include "ResonantNodeLight.h"
#include "ResonantNodeSignalComposer.h"

class Print;

// output.silent, node.radio_enabled and node.radio_log: the Node's own switches. silent holds the
// outputs-off gate shut (detection runs, Behavior never fires); radioEnabled is read at boot (PAR-013);
// radioLog is the console's lines per second mirrored to the radio, 0 off (decisions/2026-11-05-radio-log-mirror.md).
struct NodeSwitches {
    bool silent = false;
    bool radioEnabled = true;
    uint32_t radioLog = 0;
};

namespace param {

inline void describeParams(ParamRegistry& registry, NodeSwitches& node) {
    registry.addBool(ParamId::Output_Silent, ModuleId::Output, "output.silent", &node.silent);
    registry.addBool(ParamId::Node_RadioEnabled, ModuleId::Node, "node.radio_enabled", &node.radioEnabled);
    registry.addUInt32(ParamId::Node_RadioLog, ModuleId::Node, "node.radio_log", &node.radioLog, 0U, 20U);
}

}  // namespace param

/*
NodeParams

The Node's param registry and its NVS store. `describe()` registers each owner's params (each config struct
describes its own, `param::describeParams`, app/ParamBindings.h lists where) in the one order PARAM LIST / DUMP
print. The PARAM command, the boot load and the profile switch's reset report go through here; what a command
wrote comes back as the module to apply, and the Node applies it through the owners' setters
(Node::applyParamModule).
Owns: the registry, its order and the NVS store.
Does not: own the params' values (each owner's config struct) or apply them (Node::applyParamModule).
*/
class NodeParams {
public:
    // The live fields the params bind to; each lives in its owner and stays put (the registry keeps pointers).
    struct Fields {
        detection::ScalarTransientConfig& scalar;
        NetworkBand& band;
        NodeSignalComposer::Values& output;
        NodeSwitches& node;
        BehaviorGateConfig& behavior;
        detection::InspectionLimits& inspection;
        NodeLight::Values& light;
    };

    // detection.scalar_*, detection.band_*, output.hop_* / idle_announce_*, output.silent, node.radio_enabled,
    // behavior.*, detection.contrast_medium / amp_medium, detection.shape_*, light.*.
    void describe(const Fields& fields);

    // The fields hold the firmware defaults now (PAR-010 boot order).
    void captureDefaults() { _registry.captureDefaults(); }
    void printRefusedAdds(Print& out) { param::printRefusedAdds(_registry, out); }
    // Stored values into the fields at boot; the report says how many were applied.
    param::ParamLoadReport loadStored(Print& out);
    // PARAM LIST / GET / SET / DUMP / SAVE / LOAD / RESET (app/ParamCommands) with the NVS store.
    param::ParamCommandResult handleCommand(const char* line, Print& out);
    // Registered and no longer at its firmware default (a stored or SET value).
    bool changedFromDefault(const char* path) const;

    // A profile switch (RB PROFILE) resets values: remember them, switch, then print one
    // "RB PROFILE reset path=..." line per changed param and return the count.
    void rememberValues();
    unsigned long printChangedSinceRemembered(Print& out) const;

private:
    param::ParamRegistry _registry;
    NvsParamStore _store;
    double _remembered[param::ParamRegistry::kMaxParams] = {};  // not on the loop stack (3.4 KB headroom)
};
