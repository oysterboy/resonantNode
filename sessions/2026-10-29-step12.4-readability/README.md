# 2026-10-29: step 12.4, the readability pass, board checks

Analyzer E2 (COM3), remote. Before: a9b7e99 (as delivered for 13.0). After: 3e5c757 (12.4.4 report builder split,
12.4.5 runRawTrigger in stages), delivered by OTA (confirmed, app1).

| part | tool | result |
|---|---|---|
| SEQ set: hop10, chirp 3200 / 100, chirp 3450 / 150, the edge cases | tools/bench/analyzerab.py, seqcmp.py | ab_before_a9b7e99 vs ab_after_3e5c757: the same trial results (hop 10 / 10 expected, chirps 10 / 10 rejected), the same line counts, the same key set per SEQ line type; edge cases: the same line types and counts |
| RAW: feat, pcm, i2s decim=4, both + pulses / hop / band, feat sweep, both pre=0 post clamp | rawset.py (this folder) | raw_before_a9b7e99 vs raw_after_3e5c757: the same RAW_ line keys, the same CSV headers, the same row counts (4656 / 3456 / 1200 / 0 / 4800 / 6000) |

both_hop fails the allocation on both builds (`RAW_ERR memory=raw_buffer_alloc_failed`, largest block 110,580 bytes
for 6,000 + 1,600 rows of 16 bytes): mode=both with a pre window cannot get the 6,000-row maximum. Not from this pass;
the failure path itself prints the same lines before and after (free_heap 4 bytes apart).

## 20f2304 (12.4.6 the reporters' printKv) and 5f1aa75 (12.4.7-12.4.9), all four boards by OTA

| part | result |
|---|---|
| printset.py (this folder): Node E5 boot, RB STATUS, RB summary, hop_check / plan_check; Analyzer SEQ mode=system / inspect / source | print_before (053e00b / 3e5c757) vs print_after_20f2304: the ordered key list identical for all 20 Analyzer line types and 42 of 44 Node line types; the other two are the rebase lines, where the room was not quiet on the before boot (FAILED_NO_QUIET) and was on the after boot |
| analyzerab.py | ab_after_20f2304 and ab_after_5f1aa75 vs ab_before_a9b7e99: ordered keys identical per SEQ line type, the same trial results |
| rawset.py | raw_after_20f2304: the same keys and row counts, except pcm 3502 rows (before 3456): pre_samples 302 vs 256, samples read before the Emitter's start line, i.e. timing; post_samples 3200 in every run |
| tools/bench/nodecheck.py | nodecheck_20f2304 and nodecheck_5f1aa75: 26 / 26 PASS each (no --ref-log; the RB STATUS key order was compared by printset instead) |
