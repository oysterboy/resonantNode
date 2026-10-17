# 2026-10-17 hearing matrix: four Nodes, one pair at a time, the others held silent (tools/bench/pairmatrix.py)

All boards on the Node build with the serial TX-buffer fix (0ede378; banners on E1/E3/E4/E5 say 21a8529, same code),
band 2950-3950, hop 25 ms x 5 gap 10, rotations from the MAC (E1 1, E2 4, E3 3, E4 0). E5 (mic dead on its long
cable) only held silent. Per pair: the other boards opened, idle off and detection threshold 200,000, ports kept open
(a port close resets a D-AMP board), then `nodepair.py --kick none --status-every 30`, 180 s, idle voice on both.
E2-E3 ran first as a 600 s run (`bench/sessions/2026-10-17-pair-after-txfix`). Placement (owner, after the run), one row right to left: E2 -70 cm- E3 -70 cm- E1 -40 cm- E4 -49 cm- E5; E2 and E3
stand about 20 cm higher (raised on a block), E1/E4/E5 on the bench. Mic and speaker facing up.

| Pair | A -> B heard / emitted | B -> A heard / emitted | longest answer chain | note |
|---|---|---|---|---|
| E2-E3 (600 s) | E2 -> E3 19/33 | E3 -> E2 30/33 | 1 | no deaf phase, 0 dropped DMA buffers |
| E1-E2 | 4/11 | 4/10 | short | weak both ways |
| E1-E3 | 35/36 | 30/37 | **41** | long ping-pong chains |
| E1-E4 | 259/259 | 258/259 | **518 (the whole run)** | continuous ping-pong, one answer every 350 ms |
| E2-E4 | E2 -> E4 9/9 | E4 -> E2 0/11 | 1 | one-way |
| E3-E4 | 0/9 | 0/10 | 0 | no contact |

Findings:
1. **A two-node chain runs on indefinitely** when both hear: E1 and E4 answered each other 518 times in 180 s, a
   turn every 350 ms (165 ms train + ~120 ms verdict-to-emit + suppression), never stopping. The refractory of
   400 ms (from the end of the own train) does not stop it. So the open decision
   `docs/decisions/2026-10-17-two-node-exchange-chain-length.md` is now practical: with five nodes this rings forever.
2. Hearing follows distance and height. Pair distances from the placement: E1-E4 40 cm, same height: 259/259 both
   ways. E1-E3 70 cm, E3 raised: 30-35 of 36. E2-E3 70 cm, both raised: 19-30 of 33. E1-E2 140 cm: 4 of 10.
   E3-E4 110 cm, one raised: 0. E2-E4 180 cm: one-way (E2 -> E4 9/9, E4 -> E2 0/11). Beyond about 1 m, and across the
   20 cm step, contact gets unreliable; the 2026-10-16 battery reached 110 cm with both boards on the bench at the
   same height. Asymmetry (E4 -> E2 vs E2 -> E4) is per-board level or the per-direction comb.
3. No minutes-long deafness in any pair since the TX-buffer fix, and no dropped DMA buffers.

Next: put all five at the same height, and decide the chain length before the five-node run; then the full
five-node matrix at a spacing inside ~1 m.
