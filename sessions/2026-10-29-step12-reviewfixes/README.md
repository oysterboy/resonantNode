# 2026-10-29: step 12.0.1, the review fixes on the boards (ba34561, delivered over WiFi)

All five boards updated with one `NET * OTA ... token=<t>` from E1 plus the Analyzer's and the Emitter's own OTA
commands (the old images ignored the token). Each boot printed `OTA pending_verify confirm_after_ms=20000
reboot_deadline_ms=60000` and confirmed after 20 s. Script: the session's scratchpad ota12.py (deliver / rules); the
token is read from the git-ignored header and never printed.

- Token: `OTA` without token and with a wrong token -> `OTA result=bad_token`, nothing changed; a 170-character url ->
  `bad_command (url longer than 159 characters)`.
- H1: E5 sounding an idle call (`BEH emit start source=call`), then an OTA that fails (`download_failed`, connection
  refused): afterwards behavior_state=Idle, chirp_active=0; 3 / 3 hop trains valid and answered; NET * NET PING from E1
  answered by E4 and E5.
- Analyzer: `RAW` during a SEQ run -> `RAW_ERR busy reason=seq_active`; `SEQ start 0` -> `ERR SEQ tries out of range`.
- Reboot deadline (L-OTA2): E5 took a `-D OTA_TEST_HANG` image (loop blocked 5 s after boot, no crash): booted at
  8.9 s, `OTA test_hang blocking the loop before confirm` at 13.8 s, reset at 68.8 s (60 s after boot), the bootloader
  booted the previous image (ba34561 built 14:46:25) by itself.
