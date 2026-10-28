# 2026-10-27 to 10-28: step 11h, frequency sweep 500-5000 Hz (42f1eef)

Boards as in 2026-10-27-step11h-checks (42f1eef on all five): E2 Analyzer / E3 Emitter at 79 cm, one height; E1 / E4
/ E5 Nodes silent (stored), their band set per position (unsaved). Noise: `C:/Users/admin/Downloads/bench_audio.wav`
(435 s: drum and bass, female speech, male speech, flute) through the lab PC's default output (the Studio One box,
its standing level), the whole file per position while the Nodes listen.

    python tools/bench/bandsweep.py bench --analyzer COM3 --nodes E1=COM8,E4=COM10,E5=COM11 \
        --out bench/sessions/2026-10-27-step11h-sweep24 --positions 500:4000:250 \
        --noise-wav C:/Users/admin/Downloads/bench_audio.wav --hours 24

Ran 10-27 13:21 to 10-28 08:30 (19.1 h), accepted by the owner then ("Accept the sweep now"): rounds 1-8 complete,
round 9 in part (8 or 9 visits per position). Each round in a shuffled order, the shipped band (2950) first. The run
was stopped from outside, so the Nodes kept the last position's band (unsaved; a reboot or reflash restores it).
Files: positions.csv (one row per SEQ step), noise.csv (one row per Node and noise play), pos<low>_r<NN>_*.log
(Analyzer transcripts), pos<low>_r<NN>_hop5_0.csv (RAW hop captures, hopbins.py), nodes.log.

`bandsweep.py report` (hop10 accepted / tried, its avg strength range over the rounds, chirp150 accepted; no Node
accepted any noise at any position, so the noise column is 0.0 throughout and left out here):

```
band       hop acc   strength        chirp acc
 500-1500   80/80    32767-32767     0/80
 750-1750   80/80    32767-32767     0/80
1000-2000   80/80    14891-17827     0/80
1250-2250   80/80     9920-11169     0/80
1500-2500   70/80     4462-7127      0/80
1750-2750   80/90     4388-6920      0/90
2000-3000   65/90     4562-5868      0/90
2250-3250   80/80     4746-6151      0/80
2500-3500   80/80     6668-8346      0/80
2750-3750   90/90     8126-11510     0/90
2950-3950   90/90     9628-14618     0/90
3000-4000   80/80     8861-10210     0/80
3250-4250   80/80     8042-10606     0/80
3500-4500   80/80     7763-9658      0/80
3750-4750   80/80     8189-11080     0/80
4000-5000   80/80     7780-9531      0/80
```

Noise detections the Nodes rejected, summed over E1 / E4 / E5 and all plays (8-9 per position): 500-1500: 1,317,
750-1750: 887, 1000-2000: 183, 1250 and up: 0. Accepted: 0 everywhere. By file section (rounds 1-3): mostly the
flute, then the male voice at 500; the drum and bass none.

Reading:
- Noise acceptance: 0 accepted at every position over 8-9 full plays each (about an hour of music and speech per
  position); plain chirps 0 / 1,330. Below 1250 Hz the noise reaches the detector hundreds of times per play and only
  the later gates (the shape rule) stop it; from 1250 Hz up it does not trigger the detector at all.
- Detection: the hop train is accepted every time at 500-1250 (strength at or near the cap) and at 2250-5000. The
  weak zone 1500-3000 (strength 4,400-7,100) loses trains: 1500-2500 70/80, 1750-2750 80/90, 2000-3000 65/90; the
  losses move between rounds (the room, not the speaker).
- The shipped band 2950-3950: 90 / 90, strength 9,600-14,600, no noise detections.
