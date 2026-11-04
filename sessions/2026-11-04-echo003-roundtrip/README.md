# 2026-11-04: ECHO-003, the round trip Node -> echo -> whisper -> Node on E5 with stored params (784f8dea)

OTA on E5's own port (otaserve, the three images of 784f8dea). At each stop a value SET, PARAM SAVE, a reset
over USB (RTS), the value read back:

- echo: boot "PARAM load stored=0"; echo.gain 128 -> 100, SAVE written=17; after the reset "PARAM load
  path=echo.gain value=100.000 default=128.000 / stored=17 applied=17"; PARAM DUMP shows no Node or whisper path.
- whisper: boot "PARAM load stored=0" (the echo saves are not read); whisper.gain 64 -> 50, whisper.delay_ms 1717
  (the chip's); SAVE written=22; after the reset stored=22 applied=22, whisper.gain 50.
- Node: boot "PARAM load stored=50 applied=50", output.silent 1 as before: the Node's saves untouched by the trip.

Left on E5's NVS from the test: echo.gain 100 and whisper.gain 50 saved (read only in those modes). The script:
rt.py.
