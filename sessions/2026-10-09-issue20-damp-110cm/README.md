# 2026-10-09 issue #20: D-AMP at 110 cm, COM6 emitting - INVALID

All runs here (probe_obs5, p1_*) are invalid as detection data: the COM6
amp/speaker produced no sound at any level after the boards were moved
(RAW captures at the COM10 mic: no 3200 Hz above the floor; COM6's own mic
did not hear its own beep under the wiring-check sketch, where it had
heard it at 250-380k earlier the same day). Most likely a loose
breadboard wire on the amp or speaker. The silent OBS controls (p1_OBS_*)
are valid as false-positive data for profile defaults: 0 detections in
100 windows. Continued with roles swapped: bench:sessions/
2026-10-09-issue20-damp-110cm-b.
