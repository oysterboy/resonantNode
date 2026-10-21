# ResonantNode, week by week

A loose diary of how the project grew, from the first commit to now. One entry per week, no deep detail. Weeks run Monday to Sunday.

---

### Week 15 · 6–12 April: Hello, world (sort of)

The very first commit. By the end of day one the node could listen through a mic, spot loud peaks, blink an LED, and chirp back through a piezo. Two days later there was already a spec, a cleanup pass and a debug mode that plots nicely. Off to a fast start.

### Week 16 · 13–19 April: Ears and mouth

Lots of fiddling with how the node hears. Transient detection got tuned, the node learned to ignore its own chirps, and a MEMS mic joined the analog one. The MEMS mic didn't win yet, so things rolled back to analog, but tuning and chirp patterns now split per mic source.

### Week 17 · 20–26 April: A quieter week

A small tidy-up of who owns what inside the node. The first sketch of a separate "emitter" mode appeared, a device that only plays test signals. Chirp output moved into its own tone layer.

### Week 18 · 27 April – 3 May: Calibration and frequency

The emitter got a first real implementation. A baseline calibration step arrived, plus the first attempt at scoring how well a sound matches the target frequency. Audio moved to buffered samples.

### Week 19 · 4–10 May: Getting serious about sampling

Timing problems in the audio path got fixed, and the Analyzer mode started producing stable test sequences. A big refactor ran in many small "passes" with safety backups between each one. Raw mic dumps, frequency logging and spec v0.2 rounded out the week.

### Week 20 · 11–17 May: Refactor marathon

The busiest week so far. The first five-node setup ran on the bench. Then came a full rebuild of the detection architecture, done in dozens of numbered steps over two days. The Analyzer got the same treatment right after, because, in the commit's own words, "analyser is messy."

### Week 21 · 18–24 May: Out with the old

Eleven commits titled "Legacy Remove" deleted old paths, old modes and old aliases one by one. Detection and behavior settings got their own separate profiles. The rest of the week went into bringing the spec and the code back in line with each other.

### Week 22 · 25–31 May: Five nodes in a circle

Five nodes ran a slow "circle" of call and response on the bench. Detection had some wobbly days, so a lot of the week was fixing and diagnosing. A new detection profile, faster frequency math, and a memory error near the end that took until Monday to squash.

### Week 23 · 1–7 June: Detection cleanup, the itemized version

The memory bug got fixed. Then a long, numbered detection cleanup: clearer reports, clearer names, a naming glossary, raw signal health checks, and a memory diet for the firmware.

### Week 24 · 8–14 June: Who owns the truth?

Still cleaning, but now about boundaries. Each detector became responsible for its own reports, and the Analyzer stopped reconstructing things it should just read. Lots of legacy got deleted along the way. The week ended with the first automated tuning-and-logging runs.

### Week 25 · 15–21 June: Raw audio deep dive

The logging tooling grew up, and a new three-bin frequency detector appeared. Most of the week was spent on raw audio capture to see what the mic really delivers. That led to a fix in how mic values are decoded and a switch to the ESP32's own I2S driver.

### Week 26 · 22–28 June: Numbers that mean the same thing everywhere

Detector values got normalized so every stage speaks the same units. The Analyzer got more honest about which trial a detection belongs to. Profiles got another round of tuning. Then the project went quiet for the summer.

### Weeks 27–35 · 29 June – 30 August: Summer break

No commits. The nodes rested.

### Week 36 · 31 August – 6 September: Back, gently

Docs got tidied, the firmware now prints its version at boot, and a stack-size bump fixed an Analyzer crash. The first lab experiment, a distance ladder, came back inconclusive.

### Week 37 · 7–13 September: Quiet

Nothing committed.

### Week 38 · 14–20 September: Restart with a plan

Detectors got consolidated and dead code removed. A lightweight parameter registry landed, so settings can be changed at runtime. The VEKTOR spec, a future way to expose the nodes to the outside world, was imported for later.

### Week 39 · 21–27 September: Many helpers, one main branch

A big day of merging work from several parallel AI sessions. That brought in an outside critique of the firmware, a handful of real bug fixes, and memory savings of about 14 KB. The evaluation step got its new name, OccurrenceEvaluator. The project switched from Codex to Claude Code, and a CLAUDE.md file now briefs every new session. Roadmaps got reordered into numbered steps, each with a clear gate.

### Week 40 · 28 September – 4 October: Quiet

Nothing committed.

### Week 41 · 5–11 October: New hardware day

The biggest week of the whole project. Decisions got their own folder, and GitHub issues became the bench log. A separate branch now holds the bench data. Strange audio drift got hunted down and turned out to be noise from the mic, not a bug. Then the nodes moved to new hardware, the D-AMP board, which has a real amplifier and an I2S mic. It beat the old piezo nodes clearly: 50 out of 50 detections at every test distance, with no false alarms. A self-test command, a 24-hour soak runner and a status LED followed. Near the end, the soak test exposed a bug where detection went deaf after about 72 minutes, when the microsecond clock wraps around. The fix was one shared 64-bit clock.

### Week 42 · 12–14 October (so far): Review, fix, rethink the mic

The clock fix held on real hardware, and the soak ran for over seven hours with 299 out of 300 passes. The status LEDs are fitted and confirmed working. Two full code reviews produced a long list of fixes, each checked on the bench where possible. Finally, a rethink of physical setup. With the mic and speaker facing up, a single tone can land in dead spots. The plan is to make the signal and detector more robust rather than fiddle with mounting.
