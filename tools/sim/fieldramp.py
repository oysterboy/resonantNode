#!/usr/bin/env python3
"""
fieldramp.py: a desk model of N nodes answering each other, to compare the shipped
behavior (fixed 100 ms answer, hard rest at 4 field patterns) with an activity ramp
(tentative first answers, quicker as the field fills, silence at saturation).

Model only: no detection, no audio. Every emission is the 165 ms hop train; a listener
that is not emitting or in its own-emit tail hears it with probability --hear and gets
the verdict 20 ms after the train ends (bench 2026-10-16: verdict to answer start
117-127 ms with the 100 ms wait). Idle calls every 60 s +- 20 s as shipped. The ramp is
the options in docs/research/field-activity-response.md; nothing here is measured.

    python tools/sim/fieldramp.py                 # shipped, ramp and level, 5 nodes, 20 min
    python tools/sim/fieldramp.py --variant ramp --nodes 5 --minutes 30 --trace

Prints per variant: chains (a 3 s silence ends one, as nodegroup.py), chain length and
emissions per chain, emissions per minute, the chord fraction (rounds with 2+ trains
within 165 ms, as overlapstats.py), and the answer interval over the course of a chain
(first third / last third), which is the ramp's signature.
"""
import argparse
import math
import random
import statistics

STEP_MS = 10
TRAIN_MS = 165
TAIL_MS = 60          # own-emit detection tail after the train
VERDICT_LAG_MS = 20   # train end to verdict at the listener


class Node:
    def __init__(self, idx, variant, cfg, rng):
        self.idx = idx
        self.variant = variant
        self.cfg = cfg
        self.rng = rng
        self.emit_until = -1
        self.deaf_until = -1
        self.answer_at = None          # scheduled answer (ms), None when nothing pending
        self.rest_until = -1
        self.next_idle = self.idle_delay(0)
        self.last_heard = -10**9
        self.last_emit = -10**9
        # shipped rest rule: valid patterns heard in the 4 s window, own emits in 10 s
        self.heard_times = []
        self.emit_times = []
        # ramp: leaky activity estimate
        self.activity = 0.0
        self.activity_at = 0
        self.emissions = []
        # level automaton: an integer level, decayed one step per decay_ms without hearing
        self.level = 0
        self.level_at = 0
        self.yields = 0

    def idle_delay(self, now):
        return now + self.rng.uniform(40_000, 80_000)

    # --- activity (ramp variant) ---
    def decay(self, now):
        dt = now - self.activity_at
        if dt > 0:
            self.activity *= math.exp(-dt / self.cfg.tau_ms)
            self.activity_at = now

    def bump(self, now, w):
        self.decay(now)
        self.activity += w

    # --- events ---
    def hear(self, now):
        """A valid network pattern arrives at `now`."""
        if now < self.deaf_until or self.emit_until > now:
            return
        if self.rng.random() > self.cfg.hear:
            return
        self.last_heard = now
        if self.variant == "level":
            self.hear_level(now)
            return
        if now < self.rest_until:
            return
        if self.variant == "shipped":
            self.heard_times = [t for t in self.heard_times if now - t < 4000] + [now]
            if len(self.heard_times) >= 4:
                self.rest_until = now + 5000
                self.answer_at = None
                return
            if self.answer_at is None and self.emit_until <= now and now >= self.refractory_until():
                self.answer_at = now + 100
        else:
            self.bump(now, 1.0)
            if self.activity >= self.cfg.sat:
                self.rest_until = now + self.cfg.rest_ms
                self.answer_at = None
                self.activity = 0.0
                return
            x = min(self.activity / self.cfg.sat, 1.0)
            p = self.cfg.p_min + (1.0 - self.cfg.p_min) * x
            if self.rng.random() > p:
                return
            delay = self.cfg.d_max - (self.cfg.d_max - self.cfg.d_min) * x
            delay *= self.rng.uniform(0.7, 1.3)   # per-node spread breaks the chord
            if self.answer_at is None and self.emit_until <= now:
                self.answer_at = now + delay

    def hear_level(self, now):
        """Level automaton: one integer, one probability from it (p = level / L) for both answering and not
        yielding; a fixed jitter window; saturation at L."""
        if now < self.rest_until:
            return
        self.level += 1
        self.level_at = now
        if self.level >= self.cfg.levels:
            self.rest_until = now + self.cfg.rest_ms
            self.level = 0
            self.answer_at = None
            return
        p = min(1.0, (self.level + self.cfg.p_offset) / self.cfg.levels)
        if self.answer_at is not None:
            if self.rng.random() > p:      # yield: someone else answered first
                self.answer_at = None
                self.yields += 1
            return
        if self.rng.random() <= p and self.emit_until <= now:
            self.answer_at = now + 100 + self.rng.uniform(0, self.cfg.jitter_ms)

    def refractory_until(self):
        return self.last_emit + TRAIN_MS + self.cfg.refractory_ms

    def start_emit(self, now, field):
        self.emit_until = now + TRAIN_MS
        self.deaf_until = now + TRAIN_MS + TAIL_MS
        self.last_emit = now
        self.emissions.append(now)
        self.answer_at = None
        self.next_idle = self.idle_delay(now)
        if self.variant == "shipped":
            self.emit_times = [t for t in self.emit_times if now - t < 10_000] + [now]
            if len(self.emit_times) >= 6:
                self.rest_until = now + TRAIN_MS + 5000
        elif self.variant == "level":
            self.level += 1
            self.level_at = now
            if self.level >= self.cfg.levels:
                self.rest_until = now + TRAIN_MS + self.cfg.rest_ms
                self.level = 0
        else:
            self.bump(now, self.cfg.w_own)
            if self.activity >= self.cfg.sat:
                self.rest_until = now + TRAIN_MS + self.cfg.rest_ms
                self.activity = 0.0
        field.schedule(now + TRAIN_MS + VERDICT_LAG_MS, self.idx)

    def tick(self, now, field):
        if self.variant == "level" and self.level > 0 and now - self.level_at >= self.cfg.decay_ms:
            self.level -= 1
            self.level_at = now
        if self.answer_at is not None and now >= self.answer_at and self.emit_until <= now:
            if now >= self.rest_until:
                self.start_emit(now, field)
            else:
                self.answer_at = None
        elif self.emit_until <= now and now >= self.next_idle and now >= self.rest_until \
                and now - self.last_heard > 1000 and now - self.last_emit > 500:
            self.start_emit(now, field)


class Field:
    def __init__(self):
        self.pending = []   # (time, source)

    def schedule(self, t, src):
        self.pending.append((t, src))

    def deliver(self, now, nodes, reach):
        due = [p for p in self.pending if p[0] <= now]
        self.pending = [p for p in self.pending if p[0] > now]
        # Trains that end together are one verdict at a listener (E003: a chord reads as one valid pattern).
        # With --reach the nodes stand in a line and hear only sources within that many positions.
        for n in nodes:
            if any(src != n.idx and (reach == 0 or abs(src - n.idx) <= reach) for _, src in due):
                n.hear(now)


def run(variant, cfg, seed):
    rng = random.Random(seed)
    nodes = [Node(i, variant, cfg, rng) for i in range(cfg.nodes)]
    field = Field()
    end = int(cfg.minutes * 60_000)
    for now in range(0, end, STEP_MS):
        field.deliver(now, nodes, cfg.reach)
        for n in nodes:
            n.tick(now, field)
    return sorted((t, n.idx) for n in nodes for t in n.emissions)


def analyse(events, cfg, trace):
    times = [t for t, _ in events]
    if not times:
        print("  no emissions")
        return
    # chains: a 3 s silence ends one (nodegroup.py --chain-gap 3)
    chains, cur = [], [times[0]]
    for t in times[1:]:
        if t - cur[-1] >= 3000:
            chains.append(cur)
            cur = [t]
        else:
            cur.append(t)
    chains.append(cur)
    lengths = [(c[-1] - c[0]) / 1000 for c in chains]
    counts = [len(c) for c in chains]
    per_min = {}
    for t in times:
        per_min[t // 60_000] = per_min.get(t // 60_000, 0) + 1
    # chords: rounds of starts within a train length (overlapstats.py)
    rounds, i = [], 0
    while i < len(times):
        j = i
        while j + 1 < len(times) and times[j + 1] - times[i] < TRAIN_MS:
            j += 1
        rounds.append(j - i + 1)
        i = j + 1
    chords = sum(1 for r in rounds if r >= 2)
    # the ramp's signature: answer intervals early vs late in a chain (chains with >= 6 emissions)
    early, late = [], []
    for c in chains:
        if len(c) < 6:
            continue
        gaps = [(b - a) / 1000 for a, b in zip(c, c[1:])]
        k = max(1, len(gaps) // 3)
        early += gaps[:k]
        late += gaps[-k:]
    print("  emissions %d in %.0f min, %d chains (ended within 30 s: %d), length median %.1f s max %.1f s, "
          "emissions per chain median %d max %d" % (
              len(times), cfg.minutes, len(chains), sum(1 for l in lengths if l <= 30),
              statistics.median(lengths), max(lengths), statistics.median(counts), max(counts)))
    print("  per minute max %d, median %d; chord rounds %d of %d (%.0f %%); chains of 4+ emissions: %d of %d" % (
        max(per_min.values()), statistics.median(per_min.values()), chords, len(rounds),
        100.0 * chords / len(rounds), sum(1 for c in counts if c >= 4), len(chains)))
    if early:
        print("  answer interval, chains of 6+: first third median %.2f s, last third median %.2f s" % (
            statistics.median(early), statistics.median(late)))
    per_node = {}
    for _, i in events:
        per_node[i] = per_node.get(i, 0) + 1
    print("  emissions per node: %s" % " ".join("%d" % per_node.get(i, 0) for i in range(cfg.nodes)))
    if trace:
        by_time = dict(events)
        for c in [c for c in chains if len(c) >= 4][:6]:
            print("  chain at %7.1f s: %s" % (c[0] / 1000, " ".join(
                "%.2f/%d" % ((t - c[0]) / 1000, by_time[t]) for t in c)))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--variant", choices=["shipped", "ramp", "level", "all"], default="all")
    ap.add_argument("--nodes", type=int, default=5)
    ap.add_argument("--minutes", type=float, default=20)
    ap.add_argument("--hear", type=float, default=0.95, help="probability a listener hears a train")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--reach", type=int, default=0,
                    help="nodes in a line hear only sources within this many positions (0: everyone hears everyone)")
    ap.add_argument("--trace", action="store_true", help="print the first chains")
    ap.add_argument("--refractory-ms", type=float, default=250,
                    help="shipped variant: holdoff after the train. 400 as shipped drops the reply in this model; "
                         "on the bench a pair chained 7 turns (2026-10-21-pair-state-header), so 250 is the stand-in")
    # ramp parameters (the proposal's names: docs/research/field-activity-response.md)
    ap.add_argument("--tau-ms", type=float, default=6000, help="activity decay time constant")
    ap.add_argument("--sat", type=float, default=6.0, help="activity at which the node falls silent")
    ap.add_argument("--w-own", type=float, default=1.0, help="weight of an own emission in the activity")
    ap.add_argument("--d-max", type=float, default=2500, help="answer delay ms at zero activity")
    ap.add_argument("--d-min", type=float, default=150, help="answer delay ms at saturation")
    ap.add_argument("--p-min", type=float, default=0.4, help="answer probability at zero activity")
    ap.add_argument("--rest-ms", type=float, default=8000)
    # level automaton parameters (the owner's lean 2026-10-29: probability only, yielding, few values)
    ap.add_argument("--levels", type=int, default=8, help="L: saturation level; p = level / L")
    ap.add_argument("--decay-ms", type=float, default=2000, help="one level step down per this much silence")
    ap.add_argument("--jitter-ms", type=float, default=2000, help="fixed random window on the answer delay")
    ap.add_argument("--p-offset", type=float, default=0, help="p = (level + offset) / L; 1 lets a first call ignite more often")
    cfg = ap.parse_args()
    for variant in (["shipped", "ramp", "level"] if cfg.variant == "all" else [cfg.variant]):
        print("%s (nodes %d, hear %.2f, reach %d, seed %d)" % (variant, cfg.nodes, cfg.hear, cfg.reach, cfg.seed))
        analyse(run(variant, cfg, cfg.seed), cfg, cfg.trace)


if __name__ == "__main__":
    main()
