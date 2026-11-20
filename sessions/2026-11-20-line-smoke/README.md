# 2026-11-20 smoke test: hearingtest with four nodes

The line R1 - E2 - E4 - E3, 1 m apart (setup: ../2026-11-20-line-selftest). dd22ab0a, hearingtest.py at f763e796:
each node plays in turn, the other three listen. trill-taper-glide-grain-rise, --listen band, window 2500-3500,
--pitches (2500 and 2750 Hz), --block 60, one round. 11:27-11:40.

- The radio carried three listeners in full: almost every call has an outcome at every listener (few "detected",
  no gaps).
- E4 played into silence: its amp had gone silent (see the self-test session), so every E4 row is "missed quiet";
  not a hearing result.
- 1 m: 96-100 % heard. 2 m: E2<->E3 96-100 %; R1->E4 100 % at 2500, 56 % at 2750 (invalid, amp too low).
- 3 m (R1<->E3): mostly dismissed (duration too short, strength too low); R1->E3 77 % at 2750.
- A block takes ~92 s for 60 s of listening (the preset and pin sent to four nodes, the settle).
