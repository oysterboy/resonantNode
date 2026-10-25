// Node configuration (owner 2026-10-25 split by role): the param registry's bindings, live
// apply, persistence (PARAM SAVE / LOAD / RESET) and the profile switch. Moved verbatim from
// ResonantNodeApp.cpp.
#include "ResonantNodeApp.h"
#include <Arduino.h>
#include <string.h>
#include "../../detection/InspectionPlanContract.h"
#include <ParamRegistry.h>
#include "../../app/ParamIds.h"
#include "../../app/SerialOut.h"
#include "ResonantNodeInternal.h"

using namespace node_internal;

void Node::configureParameters() {
    configureI2SParameters();
    _paramRegistry.setGroupNamer(param::moduleNameForGroup);
    registerDetectionParams();
    registerOutputParams();
    registerBehaviorParams();
    loadInspectionLimitsFromProfile();
    _paramRegistry.addFloat(param::ParamId::Detection_ContrastMedium, param::ModuleId::Detection,
                            "detection.contrast_medium", &_contrastMediumThreshold, 0.0f, 32767.0f);
    _paramRegistry.addFloat(param::ParamId::Detection_AmpMedium, param::ModuleId::Detection,
                            "detection.amp_medium", &_ampMediumThreshold, 0.0f, 32767.0f);
}

// Bind the currently-editable detection fields into the flat ParamRegistry.
// Fields live inside _activeDetectionProfile, which is a stable Node member,
// so the bound pointers stay valid across profile value changes; switching
// profiles by kind still replaces the whole struct's contents, same as
// before this registry existed.
void Node::registerDetectionParams() {
    // TonalPulseScalar (the Node's production profile, decisions/
    // 2026-10-14-node-ships-tonalpulsescalar.md). Same flat names as the
    // Analyzer's PARAM keys, under "detection.". The ms fields are
    // `unsigned long` in the config; on this 32-bit target it is the same
    // size as uint32_t, which the registry stores.
    static_assert(sizeof(unsigned long) == sizeof(uint32_t), "scalar ms fields are bound as uint32_t");
    detection::ScalarTransientConfig& sc = _activeDetectionProfile.scalarTransient;
    using param::ModuleId;
    using param::ParamId;
    _paramRegistry.addFloat(ParamId::Detection_ScalarOnsetThreshold, ModuleId::Detection,
                             "detection.scalar_onset_threshold", &sc.onsetDetectionThreshold, 0.0f, 200000.0f);
    _paramRegistry.addFloat(ParamId::Detection_ScalarReleaseThreshold, ModuleId::Detection,
                             "detection.scalar_release_threshold", &sc.onsetReleaseThreshold, 0.0f, 200000.0f);
    _paramRegistry.addUInt32(ParamId::Detection_ScalarCooldownMs, ModuleId::Detection,
                             "detection.scalar_cooldown_ms", reinterpret_cast<uint32_t*>(&sc.cooldownAfterOnsetMs), 0U, 10000U);
    _paramRegistry.addUInt32(ParamId::Detection_ScalarMinDurationMs, ModuleId::Detection,
                             "detection.scalar_min_duration_ms", reinterpret_cast<uint32_t*>(&sc.minTransientDurationMs), 0U, 5000U);
    _paramRegistry.addUInt32(ParamId::Detection_ScalarMaxDurationMs, ModuleId::Detection,
                             "detection.scalar_max_duration_ms", reinterpret_cast<uint32_t*>(&sc.maxTransientDurationMs), 0U, 5000U);
    _paramRegistry.addFloat(ParamId::Detection_ScalarMinPeakStrength, ModuleId::Detection,
                             "detection.scalar_min_peak_strength", &sc.minTransientPeakStrength, 0.0f, 200000.0f);
    _paramRegistry.addUInt32(ParamId::Detection_ScalarReleaseDebounceMs, ModuleId::Detection,
                             "detection.scalar_release_debounce_ms", reinterpret_cast<uint32_t*>(&sc.releaseDebounceMs), 0U, 1000U);
    _paramRegistry.addBool(ParamId::Detection_ScalarRequireCarrierQuality, ModuleId::Detection,
                            "detection.scalar_require_carrier_quality", &sc.requireCarrierQuality);
    _paramRegistry.addBool(ParamId::Detection_ScalarRequireMinStrength, ModuleId::Detection,
                            "detection.scalar_require_min_strength", &sc.requireMinStrength);
    _paramRegistry.addFloat(ParamId::Detection_ScalarMinStrength, ModuleId::Detection,
                             "detection.scalar_min_strength", &sc.minMatchedMeanStrength, 0.0f, 200000.0f);
    _paramRegistry.addUInt32(ParamId::Detection_ScalarMinReleaseCoverageMs, ModuleId::Detection,
                             "detection.scalar_min_release_coverage_ms", reinterpret_cast<uint32_t*>(&sc.minCoverageAboveReleaseMs), 0U, 5000U);
    _paramRegistry.addUInt32(ParamId::Detection_ScalarMinLongestIslandMs, ModuleId::Detection,
                             "detection.scalar_min_longest_island_ms", reinterpret_cast<uint32_t*>(&sc.minLongestIslandMs), 0U, 5000U);
    _paramRegistry.addUInt32(ParamId::Detection_ScalarMaxGapMs, ModuleId::Detection,
                             "detection.scalar_max_gap_ms", reinterpret_cast<uint32_t*>(&sc.maxGapMs), 0U, 5000U);
    // Network band (DET-013). Applied by applyBandParams() when the values
    // change; 0 = single target at the chirp tone.
    _paramRegistry.addUInt32(ParamId::Detection_BandLowHz, ModuleId::Detection,
                             "detection.band_low_hz", &_bandLowHz, 0U, 7900U);
    _paramRegistry.addUInt32(ParamId::Detection_BandHighHz, ModuleId::Detection,
                             "detection.band_high_hz", &_bandHighHz, 0U, 7900U);
}

void Node::registerOutputParams() {
    using param::ModuleId;
    using param::ParamId;
    _paramRegistry.addBool(ParamId::Output_HopEnabled, ModuleId::Output, "output.hop_enabled", &_hopEnabled);
    _paramRegistry.addUInt32(ParamId::Output_HopRotation, ModuleId::Output, "output.hop_rotation", &_hopRotation, 0U, 15U);
    _paramRegistry.addUInt32(ParamId::Output_HopPulseMs, ModuleId::Output, "output.hop_pulse_ms", &_hopPulseMs, 1U, 1000U);
    _paramRegistry.addUInt32(ParamId::Output_HopGapMs, ModuleId::Output, "output.hop_gap_ms", &_hopGapMs, 0U, 2000U);
    _paramRegistry.addUInt32(ParamId::Output_HopSteps, ModuleId::Output, "output.hop_steps", &_hopSteps, 1U, 8U);
    _paramRegistry.addUInt32(ParamId::Output_IdleAnnounceHz, ModuleId::Output, "output.idle_announce_hz", &_idleAnnounceHz, 0U, 7900U);
    _paramRegistry.addUInt32(ParamId::Output_IdleAnnounceMs, ModuleId::Output, "output.idle_announce_ms", &_idleAnnounceMs, 20U, 2000U);
    _paramRegistry.addBool(ParamId::Output_Silent, ModuleId::Output, "output.silent", &_silent);
    _paramRegistry.addBool(ParamId::Node_RadioEnabled, ModuleId::Node, "node.radio_enabled", &_radioEnabled);
}

// The registered detection params (detection.scalar_*, detection.band_*) applied
// in place: scalar thresholds through the detector's setters, the band through
// FreqBandStream. No detector reset and no FeatureHistory wipe, so a live PARAM
// SET does not drop an occurrence in flight (step 10 item 2, issue #14,
// decisions/2026-10-22-live-params-profile-reset-no-rb-behav.md D2). A profile
// switch still goes through applyActiveDetectionProfile().
// Behavior's BehaviorGateConfig on the registry (PAR-006, step 10 item 4). The
// params bind into the Node-owned _activeBehaviorProfile; a PARAM SET applies it
// through ResonantBehavior::configure(), which only calls setters (no state
// reset). Paths are the snake_case field names; the RB BEHAV keys they replace
// are mapped in decisions/2026-10-22-live-params-profile-reset-no-rb-behav.md.
void Node::registerBehaviorParams() {
    static_assert(sizeof(unsigned long) == sizeof(uint32_t), "behavior ms fields are bound as uint32_t");
    BehaviorGateConfig& b = _activeBehaviorProfile;
    using param::ModuleId;
    using param::ParamId;
    auto u32 = [](unsigned long* field) { return reinterpret_cast<uint32_t*>(field); };
    _paramRegistry.addBool(ParamId::Behavior_IdleEnabled, ModuleId::Behavior, "behavior.idle_enabled", &b.idleEnabled);
    _paramRegistry.addUInt32(ParamId::Behavior_WaitAfterHeardMs, ModuleId::Behavior,
                             "behavior.wait_after_heard_ms", u32(&b.waitAfterHeardMs), 0U, 10000U);
    _paramRegistry.addUInt32(ParamId::Behavior_RefractoryAfterEmitMs, ModuleId::Behavior,
                             "behavior.refractory_after_emit_ms", u32(&b.refractoryAfterEmitMs), 0U, 60000U);
    _paramRegistry.addUInt32(ParamId::Behavior_SuppressSelfChirpMs, ModuleId::Behavior,
                             "behavior.suppress_self_chirp_ms", u32(&b.behaviorSuppressSelfChirpMs), 0U, 10000U);
    _paramRegistry.addUInt32(ParamId::Behavior_DetectionSuppressTailMs, ModuleId::Behavior,
                             "behavior.detection_suppress_tail_ms", u32(&b.detectionSuppressTailMsOwnEmit), 0U, 5000U);
    _paramRegistry.addUInt32(ParamId::Behavior_IdleTimeoutMs, ModuleId::Behavior,
                             "behavior.idle_timeout_ms", u32(&b.idleTimeoutMs), 0U, 3600000U);
    _paramRegistry.addUInt32(ParamId::Behavior_IdleTimeoutVariationMs, ModuleId::Behavior,
                             "behavior.idle_timeout_variation_ms", u32(&b.idleTimeVariationMs), 0U, 3600000U);
    _paramRegistry.addUInt32(ParamId::Behavior_IdleBlockedAfterHeardMs, ModuleId::Behavior,
                             "behavior.idle_blocked_after_heard_ms", u32(&b.idleBlockedAfterHeardMs), 0U, 600000U);
    _paramRegistry.addUInt32(ParamId::Behavior_IdleBlockedAfterOwnEmitMs, ModuleId::Behavior,
                             "behavior.idle_blocked_after_own_emit_ms", u32(&b.idleBlockedAfterOwnEmitMs), 0U, 600000U);
    _paramRegistry.addUInt32(ParamId::Behavior_RestAfterFieldPatterns, ModuleId::Behavior,
                             "behavior.rest_after_field_patterns", u32(&b.restAfterFieldPatterns), 0U, 100U);
    _paramRegistry.addUInt32(ParamId::Behavior_OwnEmitBudget, ModuleId::Behavior,
                             "behavior.own_emit_budget", u32(&b.ownEmitBudget), 0U, 100U);
    _paramRegistry.addUInt32(ParamId::Behavior_OwnEmitBudgetWindowMs, ModuleId::Behavior,
                             "behavior.own_emit_budget_window_ms", u32(&b.ownEmitBudgetWindowMs), 0U, 600000U);
    _paramRegistry.addUInt32(ParamId::Behavior_RestMs, ModuleId::Behavior,
                             "behavior.rest_ms", u32(&b.restMs), 0U, 600000U);
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
}

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
    _detection.setInspectionPlan(_activeDetectionProfile.inspectionPlan);  // same streams: history kept
    applyBandParams();
}

// PARAM LIST / GET / SET / DUMP: the generic ParamRegistry command surface.
// The one command path for every registered knob since step 10 (RB PARAM and
// RB BEHAV were removed; docs/roadmaps/roadmap-param-config.md PAR-015).
void Node::handleParamCommand(const char* line) {
    char buffer[kSerialLineCapacity];
    strncpy(buffer, line, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    char* savePtr = nullptr;
    char* token = strtok_r(buffer, " ", &savePtr);
    token = token != nullptr ? strtok_r(nullptr, " ", &savePtr) : nullptr;

    if (token == nullptr) {
        serialOut().println("PARAM usage=PARAM LIST|GET <path>|SET <path> <value>|DUMP|SAVE|LOAD|RESET");
        return;
    }

    if (equalsIgnoreCase(token, "LIST")) {
        _paramRegistry.list(serialOut());
        return;
    }

    if (equalsIgnoreCase(token, "DUMP")) {
        _paramRegistry.dump(serialOut());
        return;
    }

    // Persistence (PAR-011): only SAVE writes flash. LOAD re-reads the store,
    // RESET clears it and restores the firmware defaults; both apply at once.
    if (equalsIgnoreCase(token, "SAVE")) {
        const param::ParamSaveReport report = _paramRegistry.save(_paramStore);
        serialOut().print("PARAM SAVE written=");
        serialOut().print(static_cast<unsigned long>(report.written));
        serialOut().print(" unchanged=");
        serialOut().print(static_cast<unsigned long>(report.unchanged));
        serialOut().print(" failed=");
        serialOut().print(static_cast<unsigned long>(report.failed));
        serialOut().print(" status=");
        serialOut().println(report.failed == 0 ? "ok" : "error");
        return;
    }

    if (equalsIgnoreCase(token, "LOAD")) {
        loadStoredParams();
        applyParamModule(param::ModuleId::Detection);
        return;
    }

    if (equalsIgnoreCase(token, "RESET")) {
        const bool cleared = _paramStore.clear();
        _paramRegistry.restoreDefaults();
        serialOut().print("PARAM RESET store=");
        serialOut().print(cleared ? "cleared" : "error");
        serialOut().println(" values=defaults");
        applyParamModule(param::ModuleId::Behavior);
        applyParamModule(param::ModuleId::Detection);  // also prints hop_check and plan_check
        return;
    }

    if (equalsIgnoreCase(token, "GET")) {
        const char* path = strtok_r(nullptr, " ", &savePtr);
        const param::ParamBinding* binding = path != nullptr ? _paramRegistry.find(path) : nullptr;
        if (binding == nullptr) {
            serialOut().print("PARAM GET path=");
            serialOut().print(path != nullptr ? path : "");
            serialOut().print(" status=");
            serialOut().println(param::paramSetStatusName(param::ParamSetStatus::UnknownParam));
            return;
        }
        serialOut().print("PARAM GET path=");
        serialOut().print(path);
        serialOut().print(" module=");
        serialOut().print(_paramRegistry.groupName(binding->group));
        serialOut().print(" value=");
        _paramRegistry.printValue(serialOut(), *binding);
        serialOut().println();
        return;
    }

    if (equalsIgnoreCase(token, "SET")) {
        const char* path = strtok_r(nullptr, " ", &savePtr);
        const char* value = strtok_r(nullptr, " ", &savePtr);
        // One value only: "1.5 junk" is an error, not 1.5 (issue #31 R2-19).
        const char* extra = strtok_r(nullptr, " ", &savePtr);
        if (extra != nullptr) {
            value = nullptr;
        }
        const param::ParamBinding* binding = path != nullptr ? _paramRegistry.find(path) : nullptr;

        if (binding == nullptr) {
            serialOut().print("PARAM SET path=");
            serialOut().print(path != nullptr ? path : "");
            serialOut().print(" status=");
            serialOut().println(param::paramSetStatusName(param::ParamSetStatus::UnknownParam));
            return;
        }

        serialOut().print("PARAM SET path=");
        serialOut().print(path);
        serialOut().print(" module=");
        serialOut().print(_paramRegistry.groupName(binding->group));
        serialOut().print(" old=");
        _paramRegistry.printValue(serialOut(), *binding);

        const param::ParamSetStatus status = _paramRegistry.applyValue(*binding, value);

        serialOut().print(" new=");
        _paramRegistry.printValue(serialOut(), *binding);
        serialOut().print(" status=");
        serialOut().println(param::paramSetStatusName(status));

        if (status == param::ParamSetStatus::Ok) {
            applyParamModule(static_cast<param::ModuleId>(binding->group));
        }
        return;
    }

    serialOut().println("PARAM usage=PARAM LIST|GET <path>|SET <path> <value>|DUMP|SAVE|LOAD|RESET");
}

// Stored values into the fields (PAR-010), then every module applies once. At
// boot this runs before the signal config prints; PARAM LOAD prints the checks
// after it. Each value that differs from the default, and each stale or invalid
// entry, has its own line (ParamRegistry::load).
void Node::loadStoredParams() {
    const param::ParamLoadReport report = _paramRegistry.load(_paramStore, serialOut());
    serialOut().print("PARAM load stored=");
    serialOut().print(static_cast<unsigned long>(report.stored));
    serialOut().print(" applied=");
    serialOut().print(static_cast<unsigned long>(report.applied));
    serialOut().print(" stale=");
    serialOut().print(static_cast<unsigned long>(report.stale));
    serialOut().print(" invalid=");
    serialOut().println(static_cast<unsigned long>(report.invalid));
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
    _detection.setProfileName(detection::detectionProfileName(detectionProfile.kind));
    _detection.setVerdictQueueEnabled(true);
    _freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
    _freqBandStream.setTargetFrequencyHz(_chirpOutput.toneHz());
    _freqBandStream.setBand(0, 0);  // force applyBandParams() to re-apply after a profile switch
    applyBandParams();
}

void Node::applyActiveBehaviorGateConfig() {
    _behavior.configure(activeBehaviorProfile());
}
