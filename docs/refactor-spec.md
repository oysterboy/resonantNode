# Refactor Spec
## Align to Current Architecture + Prepare Onset/Transient Split + Analyzer

## Goal

Refactor the current codebase to:

1. Align explicitly with current architecture:
   AudioSource -> AudioSignal -> AudioOnsetDetector -> ResonantBehavior -> ChirpOutput

2. Prepare clean future split:
   AudioOnset + AudioTransient (without breaking current behavior)

3. Prepare codebase for Analyzer / Test-Emitter integration

---

## STRICT vs VOLATILE

### STRICT
Must reflect current working system and not break behavior.

### VOLATILE
Prepares next steps (onset/transient split, analyzer).

---

## 1. Keep current pipeline intact [STRICT]

Do NOT break:

AudioSource
-> AudioSignal
-> AudioOnsetDetector
-> ResonantBehavior
-> ChirpOutput
-> Node

System must:
- compile
- behave as before

---

## 2. Refactor AudioOnsetDetector internally [STRICT -> VOLATILE boundary]

Current state:
AudioOnsetDetector handles:
- onset detection
- transient qualification

### Task

Refactor internally into two conceptual sections:

#### Onset stage (internal)
- threshold
- cooldown

#### Transient stage (internal)
- duration tracking
- release threshold
- debounce
- peak strength validation

Do NOT yet split into two files unless trivial.

Add comments marking:

// ONSET STAGE
// TRANSIENT STAGE

---

## 3. Prepare future class split [VOLATILE]

Structure code so it can later become:

AudioOnset
AudioTransient

WITHOUT changing interfaces elsewhere.

Requirements:
- isolate onset variables
- isolate transient variables
- avoid cross-dependencies

---

## 4. Keep AudioSignal pure [STRICT]

Ensure AudioSignal only:
- processes continuous signal
- no event detection logic

---

## 5. Keep Behavior semantic [STRICT]

ResonantBehavior should only consume:
- transientDetected
- transientStrength

Do NOT move signal math into behavior.

---

## 6. Keep Node mostly glue [STRICT]

Node responsibilities:
- update chain
- lifecycle forwarding
- debug coordination

Allowed temporary:
- param presets
- LED debug
- self-chirp suppression

Do NOT add:
- detection logic
- waveform logic

---

## 7. AudioSource contract [STRICT]

Must remain:

begin()
readSample()

Do NOT add:
- window methods
- streaming APIs

---

## 8. Debug stabilization [STRICT]

Ensure debug still outputs:

Value mode:
- raw
- magnitude
- smoothed
- onset strength
- transient strength

Event mode:
- transient pulses
- chirp start/stop

---

## 9. Analyzer preparation [VOLATILE]

Prepare codebase for:

Analyzer app using:

AudioSource
-> AudioSignal
-> AudioOnsetDetector

Requirements:
- no hidden coupling to Behavior required
- easy to reuse detector chain standalone
- support observe-only analysis first
- allow later compare-with-emitter analysis without changing the detector chain

No full implementation required yet.

---

## 10. Test-Emitter compatibility [VOLATILE]

Ensure future compatibility:

- external chirp trigger (Serial2)
- no dependency from detector to emitter
- emitter can live as its own mode on separate hardware
- analyzer may later control emitter and compare measured vs expected response
- current emitter behavior is a single-beep placeholder
- richer chirp profiles are future work

No implementation required.

---

## 11. Parameter grouping [STRICT]

Group constants clearly inside classes:

AudioSignal
AudioOnsetDetector
ResonantBehavior
ChirpOutput

No global config system.

---

## 12. Implementation steps

1. review AudioOnsetDetector
2. mark onset vs transient logic
3. clean variable grouping
4. ensure behavior inputs unchanged
5. verify debug outputs
6. confirm build + runtime unchanged

---

## 13. Success criteria

- compiles
- behavior unchanged
- detector logic clearer internally
- future split possible
- Analyzer path unblocked

---

## Summary

This refactor:

- does NOT redesign behavior
- does NOT introduce new features
- makes detector architecture explicit internally
- prepares clean next step:

AudioSignal -> AudioOnset -> AudioTransient -> Behavior
