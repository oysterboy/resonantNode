# 2026-10-16 evening: bench_audio.wav through the PC speaker, shipped build 5ba805d, band 2950-3950

Regression of the 2026-10-16-audiofile-run1 pass on the shipped build (contrast medium 30,500, amp medium 1,500,
max duration 300, history 512 as defaults; E2 flashed to 5ba805d at 16:24) with the new network band. E3 idle,
boards at 70 cm, PC active speaker 50 cm from E2 at 90 degrees, same position and level setting as the midday runs
(owner); the level was not re-measured with ambientlevel.py. Playback via Python winsound (start time in
`console.log` / this file), `SEQ OBS start tries=222 period=2000 window=1800 report_settle=200 band=2950-3950`.
Sections: `tools/bench/bench_audio.sections.txt`.

Playback started unix ms 1792161003375 (435 s file), OBS started 16:30:09 local, about 6 s later.

Result: 222 windows, 221 miss, 1 rejected by the detector (window 56, speech female, duration_too_short 3 ms), 0 occurrences reached the inspectors,
0 valid false answers. Fewer detector candidates than the midday pass on the old build (11 of 222): the shipped
values and the new band do not open the detector to this material; whether the playback level was identical is
the caveat (not re-measured).
