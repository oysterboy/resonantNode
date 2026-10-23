# 2026-10-23: three silent Nodes listening while the owner drilled on the bench (60 min)

Setup: E1, E4, E5 on Node 3f3e69d code (f38cd69 tree on E1), `output.silent=1` saved, so none sounded.
`nodegroup.py --boards E1=COM8,E4=COM10,E5=COM11 --secs 3600 --status-every 60`, 13:29-14:29. The owner said
the bench would be used for drilling from about 30 min after 13:20; start and end of the drilling were not
noted. E2/E3 idle. summary.txt has nodegroup's summary.

Result: 0 verdicts (valid or not) on all three Nodes in 60 min, 0 emissions; 59 STATUS lines per Node, 0 resets.

Not shown by this log: whether or how loud the drilling was. RB STATUS had no level field (added afterwards,
audio.level / level_max / baseline). E5's startup rebase was skipped for `no_quiet` (smooth 425) at 13:29, the
only sign of sound in the log. So the result is "nothing looked like a node's signal in that hour", not yet
"drilling does not trigger a node". E1's heard.valid read 109 throughout: counted before this session (it heard
the reference cell's trains), the counter was not reset when the port opened.
