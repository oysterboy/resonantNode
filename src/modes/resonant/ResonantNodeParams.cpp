// Node configuration (owner 2026-10-25 split by role): NodeParams (the registry, the NVS store, the
// owners' param descriptions in PARAM LIST order; step 11i N7), and the Node's side: live apply per
// module through the owners' setters, persistence at boot, the profile switch.
#include "ResonantNodeApp.h"
#include <Arduino.h>
#include <string.h>
#include "../../detection/InspectionPlanContract.h"
#include <ParamRegistry.h>
#include "../../app/ParamBindings.h"
#include "../../app/ParamCommands.h"
#include "../../app/ParamIds.h"
#include "../../app/SerialOut.h"
#include "ResonantNodeInternal.h"


// --- NodeParams ---

// The PARAM LIST / DUMP order since step 11g B2; a new param goes at the end of its owner's describeParams or here.
void NodeParams::describe(const Fields& f) {
    _registry.setGroupNamer(param::moduleNameForGroup);
    param::describeParams(_registry, f.scalar);      // detection.scalar_* (13)
    param::describeParams(_registry, f.band);        // detection.band_low_hz / band_high_hz
    param::describeParams(_registry, f.output);      // output.hop_* / idle_announce_* (7)
    param::describeParams(_registry, f.node);        // output.silent, node.radio_enabled
    param::describeParams(_registry, f.behavior);    // behavior.* (13)
    param::describeParams(_registry, f.inspection);  // detection.contrast_medium / amp_medium, detection.shape_* (8)
}

param::ParamLoadReport NodeParams::loadStored(Print& out) {
    return param::loadStoredParams(_registry, _store, out);
}

param::ParamCommandResult NodeParams::handleCommand(const char* line, Print& out) {
    return param::handleParamCommand(line, _registry, &_store, out);
}

bool NodeParams::changedFromDefault(const char* path) const {
    const param::ParamBinding* binding = _registry.find(path);
    return binding != nullptr && !_registry.isDefault(*binding);
}

void NodeParams::rememberValues() {
    const size_t count = _registry.count();
    for (size_t i = 0; i < count; ++i) {
        _remembered[i] = _registry.numericValue(_registry.at(i));
    }
}

unsigned long NodeParams::printChangedSinceRemembered(Print& out) const {
    unsigned long changed = 0;
    const size_t count = _registry.count();
    for (size_t i = 0; i < count; ++i) {
        const param::ParamBinding& binding = _registry.at(i);
        if (_registry.numericValue(binding) == _remembered[i]) {
            continue;
        }
        ++changed;
        out.print("RB PROFILE reset path=");
        out.print(binding.path);
        out.print(" old=");
        out.print(_remembered[i], binding.type == param::ParamType::Float ? 3 : 0);
        out.print(" new=");
        _registry.printValue(out, binding);
        out.println();
    }
    return changed;
}

// --- the Node's side ---

// The scalar fields bind inside _activeDetectionProfile, a stable Node member, so the pointers stay valid across
// value changes; a profile switch by kind replaces the struct's contents (setProfileFromName). Behavior's
// BehaviorGateConfig binds into the Node-owned _activeBehaviorProfile (PAR-006); a PARAM SET applies it through
// ResonantBehavior::configure() (setters only, no state reset). The inspection limits are the Node's copy of the
// active plan's (InspectionLimits), written back by applyLiveDetectionParams().
void Node::configureParameters() {
    configureI2SParameters();
    loadInspectionLimitsFromProfile();
    _params.describe(NodeParams::Fields{_activeDetectionProfile.scalarTransient, _band, _composer.values(), _switches,
                                        _activeBehaviorProfile, _inspectionLimits});
}

// The inspectors' medium limits and the shape settings as live params (step 10 item 7, step 11g B2), read from the
// active profile's plan by target (detection::inspectionLimitsFromPlan).
void Node::loadInspectionLimitsFromProfile() {
    _inspectionLimits = detection::inspectionLimitsFromPlan(_activeDetectionProfile.inspectionPlan);
}

// The registered detection params (detection.scalar_*, detection.band_*) applied
// in place: scalar thresholds through the detector's setters, the band through
// FreqBandStream. No detector reset and no FeatureHistory wipe, so a live PARAM
// SET does not drop an occurrence in flight (step 10 item 2, issue #14,
// decisions/2026-10-22-live-params-profile-reset-no-rb-behav.md D2). A profile
// switch still goes through applyActiveDetectionProfile().
void Node::applyLiveDetectionParams() {
    _detection.setScalarTransientConfig(_activeDetectionProfile.scalarTransient);
    detection::applyInspectionLimits(_activeDetectionProfile.inspectionPlan, _inspectionLimits);
    _detection.setInspectionPlan(_activeDetectionProfile.inspectionPlan);  // same streams: history kept
    applyBandParams();
}

// PARAM LIST / GET / SET / DUMP / SAVE / LOAD / RESET: the shared handler
// (app/ParamCommands, step 11e item 9) with the Node's NVS store. The one
// command path for every registered knob since step 10 (RB PARAM and RB BEHAV
// were removed; docs/roadmaps/roadmap-param-config.md PAR-015). What it wrote,
// the owning module applies here.
void Node::handleParamCommand(const char* line) {
    const param::ParamCommandResult result = _params.handleCommand(line, serialOut());
    switch (result.effect) {
        case param::ParamCommandEffect::Group:
            applyParamModule(static_cast<param::ModuleId>(result.group));
            break;
        case param::ParamCommandEffect::AllGroups:
            // LOAD / RESET rewrote fields across modules: every module applies
            // once (applySilent directly: the Detection route prints hop_check).
            applyParamModule(param::ModuleId::Behavior);
            applySilent();
            applyParamModule(param::ModuleId::Detection);  // also prints hop_check and plan_check
            break;
        case param::ParamCommandEffect::None:
            break;
    }
}

// Stored values into the fields at boot (PAR-010), then the modules apply
// once. Runs before the signal config prints; PARAM LOAD goes through
// handleParamCommand and applies every module.
void Node::loadStoredParams() {
    const param::ParamLoadReport report = _params.loadStored(serialOut());
    if (report.applied > 0) {
        applyLiveDetectionParams();
        applyActiveBehaviorGateConfig();
    }
}

// Dirty ModuleId apply route: a successful PARAM SET only writes the bound
// field, so the owning module still needs to be told to pick the change up.
void Node::applyParamModule(param::ModuleId module) {
    switch (module) {
        case param::ModuleId::Detection:
            applyLiveDetectionParams();
            printHopGapCheck();
            printInspectionPlanCheck();
            break;
        case param::ModuleId::Output:
            applySilent();
            printHopGapCheck();
            break;
        case param::ModuleId::Behavior:
            applyActiveBehaviorGateConfig();
            break;
        case param::ModuleId::Node:
            break;
    }
}

bool Node::setProfileFromName(const char* name) {
    detection::DetectionProfileKind parsed = _profileKind;
    if (!detection::detectionProfileKindFromName(name, parsed)) {
        return false;
    }

    // D1 (decisions/2026-10-22-live-params-profile-reset-no-rb-behav.md): a
    // profile switch starts from the new profile's defaults (detection.scalar_*
    // and behavior.*; band and output.* are Node values and stay), and says
    // which live values it reset.
    _params.rememberValues();
    _profileKind = parsed;
    _activeDetectionProfile = detection::detectionProfileForKind(_profileKind);
    _activeBehaviorProfile = makeTonalPulseBehaviorProfile();
    loadInspectionLimitsFromProfile();
    applyActiveProfiles();
    const unsigned long resetCount = _params.printChangedSinceRemembered(serialOut());
    serialOut().print("RB PROFILE reset_count=");
    serialOut().println(resetCount);
    return true;
}

const char* Node::profileName() const {
    return detectionProfileKindName(_profileKind);
}

const detection::DetectionProfile& Node::activeDetectionProfile() const {
    return _activeDetectionProfile;
}

const BehaviorGateConfig& Node::activeBehaviorProfile() const {
    // One behavior program for every detection profile (decisions/2026-10-17-
    // behavior-contract-and-reproducible-sessions.md); its live values are the
    // behavior.* params.
    return _activeBehaviorProfile;
}

void Node::applyActiveProfiles() {
    applyActiveDetectionProfile();
    applyActiveBehaviorGateConfig();
}

void Node::applyActiveDetectionProfile() {
    const detection::DetectionProfile& detectionProfile = activeDetectionProfile();

    _detection.setDetectorSelection(detectionProfile.detectorSelection);
    _detection.setScalarTransientConfig(detectionProfile.scalarTransient);
    _detection.setInspectionPlan(detectionProfile.inspectionPlan);
    _detection.setFieldStateConfig(detectionProfile.fieldStateConfig);
    _detection.setVerdictQueueEnabled(true);
    _freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
    _freqBandStream.setTargetFrequencyHz(_chirpOutput.toneHz());
    _freqBandStream.setBand(0, 0);  // force applyBandParams() to re-apply after a profile switch
    applyBandParams();
}

void Node::applyActiveBehaviorGateConfig() {
    _behavior.configure(activeBehaviorProfile());
}
