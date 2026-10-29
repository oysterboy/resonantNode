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
