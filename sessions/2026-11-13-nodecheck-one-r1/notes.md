# One-board Node check on R1 (2026-11-13, ~19:35)

`tools/bench/nodecheck_one.py --node R1=COM3 --gw E0=COM12 --source E2` (main, the commit that adds it). R1 =
stripboard board 1, `lochraster_v2`, image `esp32dev-lr2` at ba8bcac2, USB + buck. E0 gateway image. E2 in earshot
as the chirp source. Owner away from the bench.

Result: 24 / 24 PASS (`results.txt`; transcripts `R1.log`, `E0.log`). SELFTEST over USB: mic bit8 50.2 %, framing
ok; amp SNR 39.9-42.3 dB, spread 1.00. External: R1 heard E2's five chirps. By radio: PASS.

Two earlier runs of the script before this one (not kept), and what they changed in it:
- Run 1 (23/23): E3 restarted at the start of the check and did not confirm the radio mute, so the external check
  was not protected against its call. The script now pings every node and mutes each by name until it confirms.
- Run 2 (23/24): `own_chirp_suppressed` FAIL (`reason=own_verdict ... other_verdicts=1`): a neighbour's call landed
  in the 300 ms after one of R1's own chirps. The others were still unmuted; the script now mutes before every
  self-test.

Observation, not followed up: E3 (build 8003bf22, 2026-11-06, older than E1 and E2) restarted twice within about
10 minutes (around 19:2x and 19:31), then ran 4 minutes without a restart (uptime 23 -> 234 s by NET PING). Cause
unknown; E3 logs over the radio (`NET log from=E3`).
