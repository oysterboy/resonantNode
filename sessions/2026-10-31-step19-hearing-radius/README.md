# 2026-10-31: step 19.1, the frequency hearing radius (Analyzer)

Analyzer E2 and Emitter E3 on 1f0927a7, desk layout of 2026-10-26 (79 cm), 0.5 FS, remote. The Analyzer listens on a
5-bin window, band=3450-4450 (the 250 Hz grid); the Emitter plays the hop train (25 ms x 5, gap 10, rising
consecutive bins) shifted by o = -5..+5 bins: start 3450 + 250 * o. 10 trials per offset; offset_<o>.log, summary.txt.

| offset | bins inside the window | result | reason | shape |
|---|---|---|---|---|
| 0, +-1, +-2 | 5, 4, 3 | 10 / 10 expected each | | 5 / 4 / 3 distinct bins, strong / strong / medium |
| -3 | 2 (the train's last two pulses) | 10 / 10 rejected | pattern_rejected (shape) | 2 bins, weak |
| +3 | 2 (its first two pulses) | 10 / 10 rejected | duration_too_short | none |
| +-4 | 1 | 10 / 10 rejected | duration_too_short | none |
| +-5 | 0 | 10 / 10 missed | nothing heard | |

The radius is +-2 bins (three of five bins in the window, as the shape rule's medium class asks). One step further,
the train is seen but rejected, and only on one side does it leave a usable trace: with the train's end inside the
window (-3) the occurrence forms and the bin track carries its two bins (a "weak" shape: a lenient listener knows the
sender sits below); with its start inside (+3) the occurrence closes too short and leaves nothing. A wanderer finds
a cluster within two bins either way, and three bins below it only by the weak grade.
