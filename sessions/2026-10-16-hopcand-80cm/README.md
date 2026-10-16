# 2026-10-16 5b candidate hop shapes, 80 cm, combined build (tools/bench/hopcand.sh)

Same build, placement rule and cells as 2026-10-16-hopcand-70cm. Single-tone captures at this placement: 2026-10-16-corpus-80cm (3,300 Hz in a null: 9,700 against 27,000-38,000 for 2,800 / 3,200 / 3,800).

| Cell | expected | amp P75 | contrast P75 | failures |
|---|---|---|---|---|
| chirp 3,200 (comb reference) | 10/10 | 5,100-5,697 | 32,757 | - |
| hop5 25x5 gap 10 | 9/10 | 2,890-3,098 | 30,264-31,245 (median 30,973) | 1 evaluator: contrast weak, duration 140 ms, coverage 115 |
| hop4 30x4 gap 10 bin-aligned | 6/10 | 2,390-2,755 | 32,605-32,620 (6 inspected) | 4 detector: duration_too_short, 75 ms seen |

The 3,300 Hz step sits in a null. hop5 survives it in 9 of 10 (detector sees 102-142 ms of the 165 ms train) with the smallest contrast margin of the afternoon. hop4 loses its second step (3,300) and the train splits into 30 ms and 70 ms islands across a 50 ms hole, both below min_duration: with gap 10 and 30 ms steps, a missing middle step breaks a 4-step train. The position of the null step matters, not only the count.
