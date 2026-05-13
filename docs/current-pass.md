# Codex Refactor Task — ResonantBehavior Idle / Response Timing

## Goal

Refactor behavior logic to prevent synchronized idle chirps and make idle trigger timing probabilistic, while preserving the current detection pipeline and leaving the transient response path alone.

Current working baseline:

```txt
5 nodes
~1 m circle
neighbor-specific pickup
frequency matching works
behaviorSuppressSelfChirp = 200 ms is enough
waitAfterTransient = 100 ms
```

## Do not change

- Audio input / MEMS code
- BTL output code
- frequency matching
- detector internals
- detector API
- signal pipeline

Keep:

```txt
AudioSignal → Detector → Behavior
```

Behavior consumes detector events only.

---

## Add / organize behavior params

Add these params if missing:

```cpp
bool idleEnabled = true;

uint8_t idleChirpCount = 3;
uint16_t idleChirpMs = 100;
uint16_t idleGapMs = 100;

uint32_t idleMinMs = 10000;
uint32_t idleMaxMs = 30000;

uint32_t idleBlockedAfterHeardMs = 3000;
uint32_t idleBlockedAfterOwnEmitMs = 5000;

uint8_t responseProbabilityPct = 75;
uint16_t responseRefractoryMs = 700;

uint16_t waitAfterTransientMs = 100;
uint16_t behaviorSuppressSelfChirpMs = 150;
```

---

## Add behavior state

```cpp
uint32_t nextIdleAtMs = 0;
uint32_t lastHeardAtMs = 0;
uint32_t lastOwnEmitAtMs = 0;
uint32_t lastResponseAtMs = 0;

bool pendingResponse = false;
uint32_t pendingResponseAtMs = 0;
```

---

## Random seed

Seed once during setup:

```cpp
randomSeed(esp_random() ^ millis() ^ (uint32_t(nodeId) * 7919UL));
```

---

## Idle scheduling rule

Add helper:

```cpp
void scheduleNextIdle(uint32_t now) {
  nextIdleAtMs = now + random(idleMinMs, idleMaxMs + 1);
}
```

Call it:
- once on behavior start
- after every own emit
- after every heard transient

Key rule:

```txt
Every heard transient and every own emit reschedules idle randomly.
```

This breaks synchronized idle chirps.

---

## On valid transient

When detector reports a valid event:

```cpp
lastHeardAtMs = now;
scheduleNextIdle(now);
```

Then decide probabilistic response:

```cpp
if (now - lastResponseAtMs < responseRefractoryMs) return;
if (now - lastOwnEmitAtMs < behaviorSuppressSelfChirpMs) return;

if (random(100) < responseProbabilityPct) {
  pendingResponse = true;
  pendingResponseAtMs = now + waitAfterTransientMs;
}
```

Do not emit immediately unless current architecture already requires it.

---

## In behavior update

Handle pending response:

```cpp
if (pendingResponse && now >= pendingResponseAtMs) {
  pendingResponse = false;

  if (now - lastResponseAtMs >= responseRefractoryMs &&
      now - lastOwnEmitAtMs >= behaviorSuppressSelfChirpMs) {
    emitResponseChirp();
  }
}
```

Then call idle check.

---

## Idle check

```cpp
bool canIdle(uint32_t now) {
  if (!idleEnabled) return false;
  if (now < nextIdleAtMs) return false;
  if (now - lastHeardAtMs < idleBlockedAfterHeardMs) return false;
  if (now - lastOwnEmitAtMs < idleBlockedAfterOwnEmitMs) return false;
  return true;
}
```

If `canIdle(now)`:

```cpp
emitIdleTriplet();
scheduleNextIdle(now);
```

---

## Emit response

```cpp
void emitResponseChirp() {
  soundOutput.chirp(100); // or existing response chirp param
  uint32_t now = millis();

  lastOwnEmitAtMs = now;
  lastResponseAtMs = now;
  scheduleNextIdle(now);
}
```

---

## Emit idle

Idle = 3 chirps:

```cpp
void emitIdleTriplet() {
  for (uint8_t i = 0; i < idleChirpCount; i++) {
    soundOutput.chirp(idleChirpMs);
    delay(idleGapMs); // acceptable for now
  }

  uint32_t now = millis();
  lastOwnEmitAtMs = now;
  scheduleNextIdle(now);
}
```

Non-blocking version can come later.

---

## Success criteria

- 5 nodes no longer idle almost in sync
- idle only appears when field is quiet
- response still happens reliably
- no increase in self-triggering
- current neighbor-specific pickup remains intact

---

## Do not over-refactor

This is a behavior-only change.

Avoid:
- detector changes
- signal changes
- output driver changes
- new architecture layers
- full activity model for now
