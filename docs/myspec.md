# ResonantNode Firmware Architecture
Version: draft v0.2  
Status: working architecture for current ESP32 proto, aligned with later VEKTOR integration

---

## 1. Purpose

This document defines the firmware architecture for a **ResonantNode**.

A ResonantNode is a local sound-reactive node with:

- sound input
- sound output
- local signal processing
- local behavior
- optional later connection to VEKTOR

This architecture separates:

- hardware access
- semantic I/O wrappers
- signal/features
- behavior logic
- node orchestration

---

## 2. Layer Overview

1. HAL  
2. IO  
3. Signal / Detection  
4. Behavior  
5. Node  

---

## 3. Data Flow

HAL -> IO -> Signal/Detection -> Behavior -> IO -> HAL

---

## 4. Execution Loop

1. update input  
2. update signal  
3. update detectors  
4. update behavior  
5. update output  

---

## 5. Core Principle

- Features first (level, transient, etc.)
- Meaning later (behavior decides)

---

## 6. Structure

src/
  hal/
  io/
  signal/
  behaviors/
  nodes/

---

## 7. Summary

Keep HAL dumb, IO semantic, Signal technical, Behavior meaningful, Node orchestrating.

---

## 8. Behavior Routing

- Behavior is a feature of the node.
- A node may contain a behavior or operate without one.
- Behavior shall produce intent, not hardware access.
- Node shall route behavior intent to IO or hardware execution.
- The node orchestration code should remain stable across different behavior implementations.
- Hardware-specific actions belong in IO or HAL, not in behavior logic.
