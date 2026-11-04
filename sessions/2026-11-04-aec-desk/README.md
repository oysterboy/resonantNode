# 2026-11-04: Whispernet's echo cancellation on E5, first board tries (not working yet)

E5 on esp32dev-whisper (the canceller of faa580f7 with the played-sample tap, then the lock-and-count alignment, then
WHISPER AEC CAL). The source: the synthetic whispers (-35 dBFS) from the lab PC's speaker near E3.

- aec1 (timestamp alignment per block, 192 taps): ERLE 9.6 dB, the filter's peak at its last tap (191 of 192: the
  echo later than the window), the alignment jumping by up to 10 ms (7 relocks), block_us_max 4.2 ms of 5.3, 44
  dropped DMA buffers.
- aec2_0 / aec2_1 (lock once from 32 blocks, count; auto-centring; 128 taps): aec off 12 phrases heard, block 2.1 ms;
  aec on 8 heard, block 4.2 ms, no dropped buffer, the filter never converged (10 centring shifts, ERLE 0).
- cal1-6, cal10-12 (WHISPER AEC CAL: the raw mic and the played tap printed, tools/bench/aeccal.py cross-correlates):
  the TX-to-RX index offset came out different in every dump (-281, -25600, -50603; with the TX index kept counting
  while frozen -936, 1215, 4947; with a padded burst and silence after it 10444, 12569, 17851), the correlation low
  (0.10-0.50), and the output latency read from the time stamps -19 to +40 ms (a negative one is impossible). So
  neither the time stamps nor the two sample counters give a fixed speaker -> mic relation as they are kept now;
  the low correlation also says the mic hears the board's own replay only weakly in these takes.

Left: whisper.aec defaults to off (the canceller and the tap stay in, PARAM SET whisper.aec 1 turns it on).
What would hold: TX and RX in lockstep in one place (each RX block read, the same number of frames written) so the
delay is fixed by construction, as embedded AEC usually does it; then one cross-correlation per boot.
