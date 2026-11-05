# 2026-11-05 night: who hears whom with the glide (reach.py through the E0 gateway)

Five boards on Node 0f8ac9c9, all in one window (band_range 2500-3500, saved), on chargers; E0 the bare gateway. Each
board in turn plays 10 x RB CALL (3.5 s apart, the others silent, idle calls off), every board's own counters
(heard.valid, heard.strength_sum) before and after give hits and the mean strength per heard train. reach.txt:

| caller \ listener | E1 | E2 | E3 | E4 | E5 |
|---|---|---|---|---|---|
| E1 | - | 0/10 | 0/10 | 0/10 | 0/10 |
| E2 | 10/10 (8133) | - | 10/10 (10402) | 10/10 (8191) | 10/10 (5720) |
| E3 | 8/10 (4442) | 10/10 (6448) | - | 10/10 (13088) | 10/10 (7529) |
| E4 | 7/10 (5872) | 10/10 (3830) | 10/10 (11638) | - | 10/10 (8419) |
| E5 | 0/10 | 0/10 | 0/10 | 10/10 (8893) | - |

(mean strength in brackets, Strength16 units, the detection's own scale)

Reading: E1's call is heard by nobody and E5's only by E4, although E1 and E5 hear the others (E1 hears E2 10/10 at
8133). A re-check (E1 and E5 alone, RB CALL answers `queued`, output.started advances) says they do play. So E1's and
E5's output reaches little: a weak speaker or amp, or a board turned away; the placement on the bench was not noted.
E3 / E4 / E2 hear each other at 4000-13000; E1 hears E3 / E4 only 7-8 of 10 (the weakest links, 4400-5900).
To check by hand: E1's and E5's speaker (a tone test, SELFTEST amp) and the boards' placement.
