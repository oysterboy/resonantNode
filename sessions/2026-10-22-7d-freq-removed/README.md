# 2026-10-22: step 7d (FrequencyMatch removed), E2 Analyzer / E3 Emitter at 110 cm

Setup: as 2026-10-22-7a-gate (owner: layout unchanged). E2 (COM3) Analyzer on main 3a11d80, E3 (COM4) Emitter on
23d034a (7a used bc9a89e), 110 cm apart. E1, E4 and E5 were held silent by another session's open ports.

## Node boot check (node_boot_check.3a11d80.log, E2 flashed with the Node build first)

- `BUILD role=node git=3a11d80`, `RB STATUS profile=TonalPulseScalar emitters=ScalarTransient`.
- `RB PARAM freqScore=1` -> `RB PARAM ERR removed (FrequencyMatch removed); use PARAM SET ...`.
- `RB PROFILE name=TonalPulseFreq` -> usage `TonalPulseScalar|AmpExperimental`.
- `PARAM GET detection.freq_attack_score_min` -> `status=unknown_param`; PARAM DUMP lists the scalar and band params.

## Analyzer T3 vs 2026-10-22-7a-gate (seqcmp)

| run | 7a-gate (2a99951) | 7d (3a11d80) |
|---|---|---|
| final_chirp_diag_detail, 20 | 20 expected, dt 51 ms, strength 5,415 | 20 expected, dt 52 ms, strength 5,495 |
| final_hop5_diag_detail, 10 | 10 expected, dt 48 ms, strength 9,935 | 10 expected, dt 51 ms, strength 9,789 |
| final_dur40_diag_detail, 10 | 10 rejected: 9 at the detector (61-62 ms, too short), 1 at the pattern stage (87 ms) | 10 rejected, all at the pattern stage (85-87 ms) |

0 dropped DMA buffers in all 41 trial lines.

The 40 ms row differs at the detector. A/B on the same setup: the Analyzer from a9a47c3 (the commit before the
removal) gives the same 85-86 ms and 10 pattern-stage rejects (ab_pre7d_dur40_diag_detail.a9a47c3.log). So the
removal did not cause it; something in the setup since 7a did (E3's firmware changed from bc9a89e to 23d034a, whose
only output-side change is 98b2fd0's IRAM move; the room is unrecorded). Follow-up RAW capture
(2026-10-22-7d-dur40-raw): the Emitter plays 40 ms. A late reflection 55-80 ms after onset sits at 2,000-2,500,
just above the 1,500 release threshold, and keeps the candidate open. A 2.6 % level rise since 7a is enough to
flip it.

What it means: a 40 ms tone read +45 ms long sits 0-2 ms over the 85 ms minimum, so the duration gate held it only by
chance in 7a. Correction 2026-10-22, later: the inspectors never measured these tones. Each 85-87 ms occurrence was inspected 3 ms before its 0..+100 ms contrast window ended, so it was rejected future_window_unavailable (MissingSupport): audit 3.1's bug, by construction, not the contrast gate. Step 8b.

Afterwards E2 was flashed back to the Analyzer (3a11d80).
