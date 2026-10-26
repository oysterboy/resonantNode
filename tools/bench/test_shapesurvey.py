"""Fixture test for shapesurvey.py's parser (critique D4.1): the replay's `shape` lines as `native-replay --shape
--dir test/fixtures` prints them (Replay.cpp printShapeWindow, copied 2026-10-26 at main e36ba57), read field by
field, attached to the capture line after them, and a shape line that lost a field stops the survey instead of
being dropped. Plain asserts: python tools/bench/test_shapesurvey.py"""
import importlib.util
import os

_spec = importlib.util.spec_from_file_location(
    "shapesurvey", os.path.join(os.path.dirname(os.path.abspath(__file__)), "shapesurvey.py"))
shapesurvey = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(shapesurvey)  # no CLI run at import: main() is behind __name__

# Real replay output: a one-bin chirp (empty dip lists), an accepted hop train, a capture-only window.
REPLAY = """\
replay profile=TonalPulseScalar band=2950-3950 target=3200 contrast_medium=30500 amp_medium=1500 max_duration_ms=300
    shape support_too_low start_ms=51 dur_ms=129 active_ms=130 runs=1 distinct=1 up_steps=0 other_steps=0 dips_pct= edips_pct= seq=3200x130
chirp3200_90cm_2026-10-16.csv        samples=  4256 accepted=0 rejected=1 first: support_too_low start_ms=51 dur_ms=129 strength=9711
    shape accepted start_ms=59 dur_ms=201 active_ms=193 runs=7 distinct=5 up_steps=5 other_steps=1 dips_pct=48,34,27,24,72,72 edips_pct=43,15,15,10,68,68 seq=2950x35+0,3200x37+0,3450x30+1,3700x56+3,3950x19+0,3700x6+3,3950x12
hop5_25x5g10_70cm_2026-10-16.csv     samples=  5157 accepted=1 rejected=0 first: accepted start_ms=59 dur_ms=201 strength=9083
    shape capture start_ms=58 dur_ms=186 active_ms=127 runs=5 distinct=5 up_steps=4 other_steps=0 dips_pct=0,0,0,1 edips_pct=0,0,0,0 seq=2950x23+18,3200x26+16,3450x25+12,3700x60+3,3950x4
hop5_25x5g10_70cm_2026-10-16_dry_g15.csv samples=  5477 accepted=0 rejected=0
""".splitlines()


def test_shape_line_fields():
    g = shapesurvey.shape_fields(REPLAY[3])
    assert g == ("accepted", "59", "201", "193", "7", "5", "5", "1",
                 "2950x35+0,3200x37+0,3450x30+1,3700x56+3,3950x19+0,3700x6+3,3950x12"), g
    # Empty dip lists (one run) do not shift the fields after them.
    g = shapesurvey.shape_fields(REPLAY[1])
    assert g == ("support_too_low", "51", "129", "130", "1", "1", "0", "0", "3200x130"), g


def test_other_lines_are_not_shape_lines():
    for i in (0, 2, 4, 6):
        assert shapesurvey.shape_fields(REPLAY[i]) is None, REPLAY[i]


def test_shapes_attach_to_the_capture_after_them():
    rows = shapesurvey.rows_from_output("test/fixtures", REPLAY)
    assert [(r[1], r[2], r[3]) for r in rows] == [
        ("chirp3200_90cm_2026-10-16.csv", "chirp / steady tone", "support_too_low"),
        ("hop5_25x5g10_70cm_2026-10-16.csv", "hop", "accepted"),
        ("hop5_25x5g10_70cm_2026-10-16_dry_g15.csv", "hop (variants: dry/wet gaps)", "capture"),
    ], rows
    # The summary reads distinct and up_steps at r[8], r[9] and seq at r[11].
    assert (rows[1][8], rows[1][9], rows[1][11][:9]) == ("5", "5", "2950x35+0"), rows[1]


def test_a_field_the_replay_adds_is_ignored():
    line = REPLAY[3].replace(" seq=", " new_field=7 seq=")
    assert shapesurvey.shape_fields(line) == shapesurvey.shape_fields(REPLAY[3])


def test_a_missing_field_stops_the_survey():
    line = REPLAY[3].replace(" other_steps=1", "")
    try:
        shapesurvey.shape_fields(line)
    except SystemExit as e:
        assert "other_steps" in str(e.code), e.code
    else:
        raise AssertionError("a shape line without other_steps was accepted")


if __name__ == "__main__":
    tests = [(n, f) for n, f in sorted(globals().items()) if n.startswith("test_") and callable(f)]
    for name, fn in tests:
        fn()
        print("ok", name)
    print("%d tests passed" % len(tests))
