// Node configuration (owner 2026-10-25 split by role): the param registry's bindings, live
// apply, persistence (PARAM SAVE / LOAD / RESET) and the profile switch. Moved verbatim from
// ResonantNodeApp.cpp.
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

using namespace node_internal;

void Node::configureParameters() {
    configureI2SParameters();
    _paramRegistry.setGroupNamer(param::moduleNameForGroup);
    registerDetectionParams();
    registerOutputParams();
    // Behavior's BehaviorGateConfig on the registry (PAR-006, step 10 item 4):
    // bound into the Node-owned _activeBehaviorProfile; a PARAM SET applies it
    // through ResonantBehavior::configure() (setters only, no state reset).
    param::bindBehaviorParams(_paramRegistry, _activeBehaviorProfile);
    loadInspectionLimitsFromProfile();
    _paramRegistry.addFloat(param::ParamId::Detection_ContrastMedium, param::ModuleId::Detection,
                            "detection.contrast_medium", &_contrastMediumThreshold, 0.0f, 32767.0f);
    _paramRegistry.addFloat(param::ParamId::Detection_AmpMedium, param::ModuleId::Detection,
                            "detection.amp_medium", &_ampMediumThreshold, 0.0f, 32767.0f);
    param::bindShapeParams(_paramRegistry, _shapeConfig);
}

// The live detection params. The scalar fields bind inside _activeDetectionProfile,
// a stable Node member, so the pointers stay valid across value changes; a
// profile switch by kind replaces the struct's contents (setProfileFromName).
void Node::registerDetectionParams() {
    param::bindScalarTransientParams(_paramRegistry, _activeDetectionProfile.scalarTransient);
    // Network band (DET-013). Applied by applyBandParams() when the values
    // change; 0 = single target at the chirp tone.
    _paramRegistry.addUInt32(param::ParamId::Detection_BandLowHz, param::ModuleId::Detection,
                             "detection.band_low_hz", &_bandLowHz, 0U, runtime::kMaxToneHz);
    _paramRegistry.addUInt32(param::ParamId::Detection_BandHighHz, param::ModuleId::Detection,
                             "detection.band_high_hz", &_bandHighHz, 0U, runtime::kMaxToneHz);
}

void Node::registerOutputParams() {
    using param::ModuleId;
    using param::ParamId;
    _paramRegistry.addBool(ParamId::Output_HopEnabled, ModuleId::Output, "output.hop_enabled", &_hopEnabled);
    _paramRegistry.addUInt32(ParamId::Output_HopRotation, ModuleId::Output, "output.hop_rotation", &_hopRotation, 0U, 15U);
    // Bound so the longest emission fits OutputLimits.maxSpanMs (6000): 8 steps of 250 + 7 gaps of 250 = 3750 ms, and the
    // idle call adds announce (<= 2000) + its 200 ms gap = 5950 (review D1.2, 2026-10-26: 1000 / 2000 let a legal SET
    // turn every answer into a chirp).
    _paramRegistry.addUInt32(ParamId::Output_HopPulseMs, ModuleId::Output, "output.hop_pulse_ms", &_hopPulseMs, 1U, 250U);
    _paramRegistry.addUInt32(ParamId::Output_HopGapMs, ModuleId::Output, "output.hop_gap_ms", &_hopGapMs, 0U, 250U);
    _paramRegistry.addUInt32(ParamId::Output_HopSteps, ModuleId::Output, "output.hop_steps", &_hopSteps, 1U, 8U);
    _paramRegistry.addUInt32(ParamId::Output_IdleAnnounceHz, ModuleId::Output, "output.idle_announce_hz", &_idleAnnounceHz, 0U, runtime::kMaxToneHz);
    _paramRegistry.addUInt32(ParamId::Output_IdleAnnounceMs, ModuleId::Output, "output.idle_announce_ms", &_idleAnnounceMs, 20U, 2000U);
    _paramRegistry.addBool(ParamId::Output_Silent, ModuleId::Output, "output.silent", &_silent);
    _paramRegistry.addBool(ParamId::Node_RadioEnabled, ModuleId::Node, "node.radio_enabled", &_radioEnabled);
}

namespace {
// The plan module that inspects `target`, or nullptr (step 10 item 7).
detection::InspectionModuleConfig* moduleForTarget(detection::InspectionPlan& plan, detection::InspectionTarget target) {
    const size_t count = plan.count > detection::kMaxInspectionModules ? detection::kMaxInspectionModules : plan.count;
    for (size_t i = 0; i < count; ++i) {
        if (plan.modules[i].target == target) {
            return &plan.modules[i];
        }
    }
    return nullptr;
}
}  // namespace

// The inspectors' medium limits as live params (step 10 item 7). They are Node
// values copied from the active profile's plan, so the binding does not depend
// on the plan's module order; applyLiveDetectionParams() writes them back into
// the module with the matching target. A profile without such a module ignores
// the value.
void Node::loadInspectionLimitsFromProfile() {
    detection::InspectionModuleConfig* contrast = moduleForTarget(_activeDetectionProfile.inspectionPlan, detection::InspectionTarget::Contrast);
    detection::InspectionModuleConfig* amp = moduleForTarget(_activeDetectionProfile.inspectionPlan, detection::InspectionTarget::Amp);
    _contrastMediumThreshold = contrast != nullptr ? contrast->magnitude.supportStrength.mediumPeakThreshold : 0.0f;
    _ampMediumThreshold = amp != nullptr ? amp->magnitude.supportStrength.mediumPeakThreshold : 0.0f;
    detection::InspectionModuleConfig* shape = moduleForTarget(_activeDetectionProfile.inspectionPlan, detection::InspectionTarget::Shape);
    _shapeConfig = shape != nullptr ? shape->shape : detection::ShapeInspectionConfig{};
}

// The registered detection params (detection.scalar_*, detection.band_*) applied
// in place: scalar thresholds through the detector's setters, the band through
// FreqBandStream. No detector reset and no FeatureHistory wipe, so a live PARAM
// SET does not drop an occurrence in flight (step 10 item 2, issue #14,
// decisions/2026-10-22-live-params-profile-reset-no-rb-behav.md D2). A profile
// switch still goes through applyActiveDetectionProfile().
void Node::applyLiveDetectionParams() {
    _detection.setScalarTransientConfig(_activeDetectionProfile.scalarTransient);
    detection::InspectionModuleConfig* contrast = moduleForTarget(_activeDetectionProfile.inspectionPlan, detection::InspectionTarget::Contrast);
    detection::InspectionModuleConfig* amp = moduleForTarget(_activeDetectionProfile.inspectionPlan, detection::InspectionTarget::Amp);
    if (contrast != nullptr) {
        contrast->magnitude.supportStrength.mediumPeakThreshold = _contrastMediumThreshold;
    }
    if (amp != nullptr) {
        amp->magnitude.supportStrength.mediumPeakThreshold = _ampMediumThreshold;
    }
    detection::InspectionModuleConfig* shape = moduleForTarget(_activeDetectionProfile.inspectionPlan, detection::InspectionTarget::Shape);
    if (shape != nullptr) {
        shape->shape = _shapeConfig;
    }
    _detection.setInspectionPlan(_activeDetectionProfile.inspectionPlan);  // same streams: history kept
    applyBandParams();
}

// PARAM LIST / GET / SET / DUMP / SAVE / LOAD / RESET: the shared handler
// (app/ParamCommands, step 11e item 9) with the Node's NVS store. The one
// command path for every registered knob since step 10 (RB PARAM and RB BEHAV
// were removed; docs/roadmaps/roadmap-param-config.md PAR-015). What it wrote,
// the owning module applies here.
void Node::handleParamCommand(const char* line) {
    const param::ParamCommandResult result = param::handleParamCommand(line, _paramRegistry, &_paramStore, serialOut());
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
    const param::ParamLoadReport report = param::loadStoredParams(_paramRegistry, _paramStore, serialOut());
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
    static double before[param::ParamRegistry::kMaxParams];  // static: not on the loop stack (3.4 KB headroom, step 8b)
    const size_t count = _paramRegistry.count();
    for (size_t i = 0; i < count; ++i) {
        before[i] = _paramRegistry.numericValue(_paramRegistry.at(i));
    }
    _profileKind = parsed;
    _activeDetectionProfile = detection::detectionProfileForKind(_profileKind);
    _activeBehaviorProfile = makeTonalPulseBehaviorProfile();
    loadInspectionLimitsFromProfile();
    applyActiveProfiles();
    unsigned long resetCount = 0;
    for (size_t i = 0; i < count; ++i) {
        const param::ParamBinding& binding = _paramRegistry.at(i);
        if (_paramRegistry.numericValue(binding) == before[i]) {
            continue;
        }
        ++resetCount;
        serialOut().print("RB PROFILE reset path=");
        serialOut().print(binding.path);
        serialOut().print(" old=");
        serialOut().print(before[i], binding.type == param::ParamType::Float ? 3 : 0);
        serialOut().print(" new=");
        _paramRegistry.printValue(serialOut(), binding);
        serialOut().println();
    }
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
