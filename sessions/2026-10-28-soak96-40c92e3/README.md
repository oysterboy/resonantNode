# 2026-10-28 to 10-29: soak on 40c92e3 (11g's changes + the OTA code), stopped at 27 h for 11d

    python tools/bench/soakcycle.py --nodes E1=COM8,E4=COM10,E5=COM11 --analyzer COM3 --hours 96 --at-min 7,27,47 \
        --out bench/sessions/2026-10-28-soak96-40c92e3 --name soak

Layout as 2026-10-26-soak24-shape-b (E5 -20 cm- E4 -30 cm- E1; E2 / E3 at 79 cm). Planned 96 h while the owner was
away; stopped 10-29 ~13:10 at 27.0 h because the lab PC's firewall rule went in (via remote desktop) and 11d's OTA
could run.

- Room checks (82, at :07 / :27 / :47): hop10 820 / 820 (strength 6,690-14,790); chirp 3200 / 100 ms 0 / 820 (shape
  74 checks, duration_too_short 8, the 3200 null on 10-28 afternoon); chirp 3450 / 150 ms 0 / 810 (shape).
- Network (nodegroup_interim.py): 17,859 emissions, 1,484 chains all ended within 30 s (max 4.9 s), 0 noise-started
  of 16,316 answers, 0 resets, 0 verdict-queue overflows; E4 / E5 0 dropped buffers, E1 125 all from the blocking
  OTA attempt before the soak started (unchanged in all 810 STATUS lines).
