# 2026-11-05 night: how complete is the radio log at node.radio_log 20 (E0 gateway)

Five boards on the Node image 0f8ac9c9 (E3 silent, E1 / E2 / E4 / E5 playing, start chance 85, level 200), all on
chargers; E0 (COM12) the bare radio gateway. boardview.py --boards E0=COM12 --radio E1,E2,E3,E4,E5 --poll "RB STATUS".
log20.log: the first 212 s, RB log still minimal after the reflash (no verdict lines: nothing to compare).
log20_full.log: 660 s with `NET * RB log full`, node.radio_log 20 (live).

Truth = each board's own counter (heard.valid from RB STATUS, replies are unicast with retries); compared with the
`RB verdict= ... valid=1` lines that reached the log:

| board | counter delta | logged valid verdicts | ratio |
|---|---:|---:|---:|
| E1 | 163 | 188 | 1.15 |
| E2 | 122 | 142 | 1.16 |
| E3 | 272 | 296 | 1.09 |
| E4 | 196 | 217 | 1.11 |
| E5 | 198 | 215 | 1.09 |
| all | 951 | 1058 | 1.11 |

The ratio is over 1 because the counter delta only spans the first to the last status reply (~630 s of the 660 s
the log covers); within that, nothing is missing. At node.radio_log 5 the same measure read 0.59 (first ten minutes of
the overnight run, tools/bench/logcomplete.py eval with E3 on USB). Dropped DMA buffers: 0 on all five during the run
(i2s.dropped_dma_buffers), no overflows. So 20 lines per second is complete and costs the audio nothing, with five
boards in one field. radio_log 20 saved on all five after this run.
