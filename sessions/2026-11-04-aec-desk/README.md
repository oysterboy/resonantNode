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

## The lockstep output (DuplexStream; the owner's go, decisions/2026-11-04-whispernet-duplex-lockstep.md)

- lockstep_cal21-22 (TX queue kept full, writes waiting): the loop waited on every write, 235 mic buffers dropped,
  the delay meaningless.
- lockstep_cal31-33 (half queue, no waiting, refilled after drops), the replayed synthetic whisper as the signal: D
  1091, 454, 1091 at corr 0.13-0.24 (the replay too quiet at the mic, 13-19 rms, for the correlation).
- lockstep_cal41-44 (WHISPER AEC CAL playing 0.4 s of its own noise): D = 1066 samples (44.4 ms) four times alike,
  corr 0.35: the speaker's delay behind the mic is fixed. aec_delay's default since: the queue (512) + 554.
- Training on WHISPER AEC NOISE 4000 (x4), whisper.aec 1, 128 taps, delay 1066: ERLE 10.9-11.1 dB, the filter's peak
  at tap 32 (the lead: aligned), no new dropped buffer, block_us_max 4.6 ms of the 5.3 ms block. (The double-talk
  freeze had held the filter at 0 dB before it learned: it now waits for ERLE above 10 dB.)
- aec3_0 / aec3_1 (the synthetic whispers, aec off / on): 10 phrases each, 4 / 2 checks passed (too few to compare),
  ERLE during the replays 9.2-9.5 dB.

whisper.aec stays off by default (deaf during its replay the node gains little); listen_while_playing (15 dB) and
whether 10 dB is enough are the next desk questions.
