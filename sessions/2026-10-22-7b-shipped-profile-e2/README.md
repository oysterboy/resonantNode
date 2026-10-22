# 2026-10-22-7b-shipped-profile-e2

Step 7b, gate item "one on-device test with the shipped scalar profile".

- Board: E2 (COM3), D-AMP, no audio used (the test replays PCM from flash).
- Firmware: the Unity test test/test_shipped_profile on main d847b37 (the test file of that commit; src at 23d034a), env
  esp32dev-analyzer.
- Run: `pio test -e esp32dev-analyzer -f test_shipped_profile --upload-port COM3 --without-testing`, then an EN
  reset and a read of the port (test_shipped_profile.d847b37.log).

Result: 6 Tests 0 Failures. makeTonalPulseScalarProfile() unchanged, every shipped gate on, float Goertzel on the
ESP32: the chirp is accepted on the single target and in band mode, the off-bin 3300 Hz tone is rejected at the
contrast gate, the hop train is accepted once in band mode and not heard on the single target, and the radio speech
is not accepted. These are the verdicts the host suite gives on the same fixtures; the same file passes 6/6 on the
host (native-test-behavior).

The first upload (6899683) overflowed the 16 KB loop stack: the stack canary triggered in FeatureHistory::reset(),
whose `buffer = {}` builds a StreamBuffer temporary, under the test's stack-held AudioSignal. Fixed in the test
(d847b37); the Node is not affected by this. Afterwards E2 was flashed back to the Analyzer (BUILD git=23d034a).
