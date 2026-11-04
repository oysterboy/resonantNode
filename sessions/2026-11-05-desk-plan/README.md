# Plan for the desk, 2026-11-05 (prepared 2026-11-04, nothing run yet)

Boards now: E1-E4 Nodes on 7e4c2cf8 (stored output.silent 1), E5 on esp32dev-whisper (whisper.enabled off live
until the session; it stays a whisper board). Images to serve (otaserve): node / echo / whisper of the day's HEAD.

0. Power: a powered USB hub or the amps on their own supply before anything loud (ports dropped at echo gain 128).

1. Whispernet with the echo cancellation (E5), ~9 min, the owner whispering:
     python tools/bench/deskrun.py bench/sessions/2026-11-05-desk-plan/whisper_listen.json --out bench/sessions/<folder>
     python tools/bench/deskeval.py bench/sessions/<folder>
   Short and long sentences, each deaf (as now) and listening while playing (whisper.listen_while_playing 1 at
   ERLE >= 8 dB); a rustle section at the end. Per section: phrases, passes and Behavior's decisions, deaf time
   (deafs / deaf_ms), phrases cut by the own playback (cuts), ERLE. The question: does listening while playing make
   it less sluggish and keep long sentences whole, without answering itself?

2. Labelled recordings from the PC mic (input "Microphone (HD Audio)", device 2 or 35; check it is a mic), ~6 min:
     python tools/bench/labelrec.py bench/sessions/2026-11-05-desk-plan/labels.json --out bench/sessions/<folder> --device 2
   One WAV per label (whisper at three distances, speech, typing, chair, paper, clothes) and the features per label
   (the board's flatness, HF share, bumps, rise): which separates whispers from the room's noises.

3. Echo room on all five with the hub's power: chain length with spread answers (ECHO GATE wait=700 jitter=1500,
   pre=100 post=200; now params echo.*): boardview.py open, claps, the owner's ear.

4. Node listening session (all five back to the Node image): presets one after another, ~5 min each, boardview open:
     python tools/bench/nodepreset.py --boards E1=COM8,E2=COM3,E3=COM4,E4=COM10,E5=COM11 low|deep|calm|wide|quiet
     python tools/bench/boardview.py --boards E1=COM8,E2=COM3,E3=COM4,E4=COM10,E5=COM11 --html boards.html
   The owner picks what sounds right; the field splitting into two groups (the 41 h run) is the open question
   (a wander also when only the own group is heard: a Behavior change, the owner's call).
