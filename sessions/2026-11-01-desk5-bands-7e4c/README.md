# 2026-11-01 evening: five boards free, Nodes with a band per node, then the echo room on all five

## Nodes, a band per node (16:25-16:53, 7e4c2cf8; E2 / E3 by OTA from the Analyzer / Emitter images)

Live (nodegroup --setup), nothing saved: output.silent 0, behavior.band_per_node 1, base_scatter_bins 1. Runs, each
cut short by an owner's change: free_60min (16:25-16:29, defaults), free_60min_lively (16:32-16:34, level_max 4,
level_decay_ms 5000), free_60min_quick (16:36-16:41, answer_jitter_ms 800; ended by a serial "access denied"),
free_60min_low / low2 (16:44, 16:46-16:53; band range 500-2000, idle announce 120 Hz 80 ms; low ended by the same
error). In the first 4 min: 43 valid verdicts, 13 answered, 25 declined by the level draw; positions at 16:40 E1, E3,
E5 1200-2200, E2 1700-2700, E4 2700-3700.

USB: the ports dropped twice ("ClearCommError ... access denied") once the boards played more and faster; no board
reset, nothing in the Windows system log. Owner's reading: power, the amps drawing from the hub. nodegroup now
reopens a lost port and sends the setup again (b141a676).

## Echo room on all five (17:00-18:15, b141a676 then 49190b69)

Owner's settings, live in order: gain 32 then 128 ("sounds good"; 128 and attack 20 ms became the defaults in
49190b69, which also made the echo room's Behavior live: levelmax / offset / decay / rest / wait / jitter);
levelmax 12 offset 8 rest 2000 ("ends too soon"); levelmax 20 offset 20 (every clap answered), wait 600 jitter 400;
onset 800 release 560 peak 800 attack 40, wait 900 jitter 500 ("ends too soon, answer a bit later"); wait 700 jitter
1500; pre 100 post 200 (replays were playing only the clap's tail; not checked). Stopped with on=0 (listening, silent).

echo5_first10min (17:02-17:12, gain 1, defaults): 20 replays in 10 chains, mostly 1-2 replays, the longest 7 over
5.7 s (5-1-2-3-3-2-1); two boards answering one clap played 0.1-0.2 s apart (simultaneous: each deaf while it plays,
so nobody hears the other). Accepted claps rose in 3-10 ms. The later runs' logs were lost (the quick logger wrote at
its end and was killed for each change; it now writes each line at once).

Counters at the end (since the 17:43 OTA, all settings above): claps 192-409, replays 117-200 (about half), "slow"
(attack) 161-197, clip_missing 0, block_us_max ~7 ms (the 48 kHz block is 2.7 ms), dropped_bufs 25-26 each.

Open: the chain's length with the answers spread (jitter 1500) and the longer pre-roll, untested; power for the amps.
