// native-fieldsim (BEH-008, step 16.2): N real ResonantBehaviors (the firmware's, not a model) through a fake room.
//
// Each node ticks at 1 ms. When its Behavior asks for a chirp the node sounds for the train's span (an idle call:
// its announcement, a gap, then the train); every node that hears it (the hearing matrix) gets a verdict after the
// train ends, unless it was sounding or inside its own-emit suppression while the train played. Several trains
// heard by one node are merged by step 14's rule (bench/sessions/2026-10-29-step14-rotations): starts less than
// 230 ms apart are one occurrence, counted by its duration (ResonantBehavior::trainsInVerdict); 230 ms or more apart
// are separate verdicts. The verdict comes 20 ms after the occurrence ends (release debounce plus inspection).
//
// Reports the decision's gate (decisions/2026-10-29-answers-follow-the-field-level-automaton.md, BEH-008):
// chains end (3 s silence within 30 s in 80 % of starts), no minute over 100 emissions, chords under 25 % of rounds
// in a chain's first half and above in its last, the first answer to a call later than 1 s in most chains.
//
//   program --matrix all|line|line2 --nodes 5 --hours 4 --seed 1 [--offset 0|1] [--jitter MS] [--levels L] [--old]
//           [--emissions out.csv]
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "../../behavior/ResonantBehavior.h"

namespace {

constexpr unsigned long kTrainMs = 165;        // the hop train (25 ms x 5, gap 10)
constexpr unsigned long kAnnounceMs = 500;     // output.idle_announce_ms
constexpr unsigned long kAnnounceGapMs = 200;  // the idle call's gap before its train (approximate)
constexpr unsigned long kTailMs = 50;          // a train reads ~215 ms as a verdict (step 14)
constexpr unsigned long kVerdictLagMs = 20;    // release debounce + inspection
constexpr unsigned long kMergeMs = 230;        // step 14: starts closer than this merge into one occurrence

uint64_t gRng = 88172645463325252ULL;
uint32_t simRandom(uint32_t bound) {
    gRng ^= gRng << 13;
    gRng ^= gRng >> 7;
    gRng ^= gRng << 17;
    return bound == 0 ? 0U : static_cast<uint32_t>(gRng % bound);
}

// The 8-bin band with a 5-step train (BEH-009): bases 0..3 at 2950 + 250 * base.
constexpr uint32_t kBandLowHz = 2950;
constexpr uint32_t kBinHz = 250;
constexpr uint32_t kBases = 4;

struct Emission {
    unsigned long startMs;     // the train's start (an idle call's train starts after its announcement)
    int node;
    bool idle;
    uint32_t base;  // the base bin the train started on
};

struct Occurrence {  // one listener's open occurrence: merged trains
    bool open = false;
    unsigned long startMs = 0;
    unsigned long endMs = 0;
    uint32_t lowestBase = 0;  // a chord's lowest train decides the heard base (the bin track's lowest bin)
};

struct HeardTrain {
    unsigned long startMs;
    unsigned long endMs;
    uint32_t base;
};

struct Node {
    ResonantBehavior behavior;
    unsigned long soundingUntil = 0;  // the node's own output (announcement + train)
    bool sounding = false;
    Occurrence occ;
    std::vector<HeardTrain> heardTrains;  // pending merge per listener
};

bool heard(const std::string& matrix, int listener, int speaker) {
    if (listener == speaker) {
        return false;
    }
    const int d = abs(listener - speaker);
    if (matrix == "all") {
        return true;
    }
    if (matrix == "line") {
        return d == 1;
    }
    if (matrix == "line2") {
        return d <= 2;
    }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    std::string matrix = "all";
    int n = 5;
    double hours = 4.0;
    unsigned long seed = 1;
    int offset = -1;
    bool old = false;
    long jitter = -1;
    long levelMax = -1;
    int shrink = -1;  // -1: the firmware's default
    bool glide = true;
    const char* emissionsPath = nullptr;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--matrix" && i + 1 < argc) {
            matrix = argv[++i];
        } else if (a == "--nodes" && i + 1 < argc) {
            n = atoi(argv[++i]);
        } else if (a == "--hours" && i + 1 < argc) {
            hours = atof(argv[++i]);
        } else if (a == "--seed" && i + 1 < argc) {
            seed = strtoul(argv[++i], nullptr, 10);
        } else if (a == "--offset" && i + 1 < argc) {
            offset = atoi(argv[++i]);
        } else if (a == "--jitter" && i + 1 < argc) {
            jitter = atol(argv[++i]);
        } else if (a == "--levels" && i + 1 < argc) {
            levelMax = atol(argv[++i]);
        } else if (a == "--shrink") {
            shrink = 1;
        } else if (a == "--no-shrink") {
            shrink = 0;
        } else if (a == "--no-glide") {
            glide = false;
        } else if (a == "--old") {
            old = true;
        } else if (a == "--emissions" && i + 1 < argc) {
            emissionsPath = argv[++i];
        } else {
            fprintf(stderr,
                    "usage: %s --matrix all|line|line2 --nodes N --hours H --seed S [--offset 0|1] [--jitter MS] "
                    "[--levels L] [--no-glide] [--old] "
                    "[--emissions out.csv]\n",
                    argv[0]);
            return 2;
        }
    }
    gRng ^= 0x9E3779B97F4A7C15ULL * (seed + 1);

    BehaviorGateConfig cfg;  // shipped: the level automaton
    if (offset >= 0) {  // otherwise the firmware's default
        cfg.levelOffset = static_cast<uint32_t>(offset);
    }
    if (jitter >= 0) {
        cfg.answerJitterMs = static_cast<uint32_t>(jitter);
    }
    if (levelMax >= 0) {
        cfg.levelMax = static_cast<uint32_t>(levelMax);
    }
    if (shrink >= 0) {
        cfg.answerJitterShrinks = shrink == 1;
    }
    cfg.baseGlide = glide;
    if (old) {  // the behaviour until step 16: fixed answer, the two rest triggers
        cfg.levelMax = 0;
        cfg.restAfterFieldPatterns = 4;
        cfg.ownEmitBudget = 6;
        cfg.restMs = 5000;
    }
    std::vector<Node> nodes(static_cast<size_t>(n));
    for (Node& node : nodes) {
        node.behavior.configure(cfg);
        node.behavior.resetState();
        node.behavior.setRandomSource(&simRandom);
        node.behavior.seedIdleSchedule(0);
        node.behavior.setBaseGrid(kBandLowHz, kBinHz, kBases);
        node.behavior.seedBaseBin(
            simRandom(kBases));  // random at boot (the decision; the roster overrides on the desk)
    }

    std::vector<Emission> emissions;
    std::vector<unsigned long> restsAt;
    const unsigned long endMs = static_cast<unsigned long>(hours * 3600000.0);
    // Pending deliveries: (time, listener, start, end)
    struct Delivery {
        unsigned long atMs;
        int listener;
        unsigned long startMs;
        unsigned long durationMs;
        uint32_t lowestBase;
    };
    std::vector<Delivery> deliveries;
    unsigned long restsBefore = 0;

    for (unsigned long t = 1; t < endMs; ++t) {
        // 1. verdicts due now
        for (size_t k = 0; k < deliveries.size();) {
            if (deliveries[k].atMs <= t) {
                const Delivery d = deliveries[k];
                deliveries.erase(deliveries.begin() + static_cast<long>(k));
                Node& node = nodes[static_cast<size_t>(d.listener)];
                detection::OccurrenceVerdict v;
                v.type = detection::VerdictType::SinglePulse;
                v.accepted = v.proposalMatched = v.supportMatched = v.valid = true;
                v.primaryStartMs = d.startMs;
                v.primaryHeardAtMs = t;
                v.primaryAcceptedMs = t;
                v.primaryDurationMs = d.durationMs;
                v.primaryStrength = 9000.0f;
                v.primaryLowestBinHz = static_cast<uint16_t>(kBandLowHz + d.lowestBase * kBinHz);
                node.behavior.handleOccurrenceVerdict(v, detection::FieldState{}, t);
            } else {
                ++k;
            }
        }
        // 2. close occurrences that no further train can join
        for (int i = 0; i < n; ++i) {
            Occurrence& o = nodes[static_cast<size_t>(i)].occ;
            if (o.open && t >= o.endMs + kVerdictLagMs && t - o.startMs >= kMergeMs) {
                deliveries.push_back({t, i, o.startMs, (o.endMs - o.startMs) + kTailMs, o.lowestBase});
                o.open = false;
            }
        }
        // 3. Behavior, and chirps
        for (int i = 0; i < n; ++i) {
            Node& node = nodes[static_cast<size_t>(i)];
            if (node.sounding && t >= node.soundingUntil) {
                node.sounding = false;
                node.behavior.notifyChirpFinished(t);
            }
            node.behavior.update(t);
            if (!node.sounding && node.behavior.shouldStartChirp()) {
                const bool idle = strcmp(node.behavior.chirpRequestSourceName(), "idle") == 0;
                const unsigned long span = idle ? kAnnounceMs + kAnnounceGapMs + kTrainMs : kTrainMs;
                node.behavior.notifyChirpStarted(t, span);
                node.sounding = true;
                node.soundingUntil = t + span;
                const unsigned long trainStart = t + span - kTrainMs;
                const uint32_t base = node.behavior.baseBin();  // after update(): an answer's glide is in it
                emissions.push_back({trainStart, i, idle, base});
                // every listener that hears it, unless sounding or suppressed while the train plays
                for (int j = 0; j < n; ++j) {
                    if (!heard(matrix, j, i)) {
                        continue;
                    }
                    Node& l = nodes[static_cast<size_t>(j)];
                    l.heardTrains.push_back({trainStart, trainStart + kTrainMs, base});
                }
            }
        }
        // 4. trains whose end has come: deaf or into the listener's occurrence
        for (int j = 0; j < n; ++j) {
            Node& l = nodes[static_cast<size_t>(j)];
            for (size_t k = 0; k < l.heardTrains.size();) {
                const HeardTrain tr = l.heardTrains[k];
                if (t < tr.endMs) {
                    ++k;
                    continue;
                }
                l.heardTrains.erase(l.heardTrains.begin() + static_cast<long>(k));
                const bool deaf = (l.sounding && l.soundingUntil > tr.startMs) ||
                    l.behavior.ownEmitDetectionSuppressed(tr.startMs) ||
                    l.behavior.ownEmitDetectionSuppressed(tr.endMs);
                if (deaf) {
                    continue;
                }
                Occurrence& o = l.occ;
                if (o.open && tr.startMs - o.startMs < kMergeMs) {
                    o.endMs = std::max(o.endMs, tr.endMs);
                    o.lowestBase = std::min(o.lowestBase, tr.base);
                } else {
                    if (o.open) {
                        deliveries.push_back({t, j, o.startMs, (o.endMs - o.startMs) + kTailMs, o.lowestBase});
                    }
                    o.open = true;
                    o.startMs = tr.startMs;
                    o.endMs = tr.endMs;
                    o.lowestBase = tr.base;
                }
            }
        }
    }
    unsigned long rests = 0;
    for (Node& node : nodes) {
        rests += node.behavior.restsEntered();
    }
    (void)restsBefore;

    if (emissionsPath != nullptr) {
        FILE* f = fopen(emissionsPath, "w");
        if (f != nullptr) {
            fprintf(f, "ms,node,idle,base\n");
            for (const Emission& e : emissions) {
                fprintf(f, "%lu,%d,%d,%lu\n", e.startMs, e.node, e.idle ? 1 : 0, static_cast<unsigned long>(e.base));
            }
            fclose(f);
        }
    }

    // --- the gate ---
    std::sort(emissions.begin(), emissions.end(),
              [](const Emission& a, const Emission& b) { return a.startMs < b.startMs; });
    struct Chain {
        size_t first, last;
    };
    std::vector<Chain> chains;
    for (size_t k = 0; k < emissions.size(); ++k) {
        if (chains.empty() || emissions[k].startMs - emissions[chains.back().last].startMs >= 3000) {
            chains.push_back({k, k});
        } else {
            chains.back().last = k;
        }
    }
    size_t ended30 = 0, ignited = 0, idleStarts = 0, lateFirst = 0, firstAnswers = 0;
    size_t roundsFirst = 0, chordsFirst = 0, roundsLast = 0, chordsLast = 0;
    double sumLen = 0.0;
    size_t maxLen = 0;
    for (const Chain& c : chains) {
        const size_t len = c.last - c.first + 1;
        sumLen += static_cast<double>(len);
        maxLen = std::max(maxLen, len);
        const unsigned long span = emissions[c.last].startMs - emissions[c.first].startMs;
        if (span <= 30000) {
            ++ended30;
        }
        if (emissions[c.first].idle) {
            ++idleStarts;
            if (len >= 2) {
                ++ignited;
                ++firstAnswers;
                if (emissions[c.first + 1].startMs - (emissions[c.first].startMs + kTrainMs) > 1000) {
                    ++lateFirst;
                }
            }
        }
        if (len >= 4) {  // chords along the chain: rounds = trains within 100 ms of each other
            const unsigned long mid = emissions[c.first].startMs + span / 2;
            size_t k = c.first;
            while (k <= c.last) {
                size_t r = k;
                while (r + 1 <= c.last && emissions[r + 1].startMs - emissions[k].startMs <= 100) {
                    ++r;
                }
                const bool chord = r > k;
                if (emissions[k].startMs < mid) {
                    ++roundsFirst;
                    chordsFirst += chord ? 1 : 0;
                } else {
                    ++roundsLast;
                    chordsLast += chord ? 1 : 0;
                }
                k = r + 1;
            }
        }
    }
    // The glide (BEH-009): distinct bases among a chain's first three trains and its last three, for chains of six
    // or more; "settled" when the last three share one base.
    size_t longChains = 0, settled = 0, distinctFirst = 0, distinctLast = 0;
    auto distinct = [&](size_t from) {
        uint32_t seen = 0;
        for (size_t k = from; k < from + 3; ++k) {
            seen |= 1U << emissions[k].base;
        }
        return static_cast<size_t>(__builtin_popcount(seen));
    };
    for (const Chain& c : chains) {
        if (c.last - c.first + 1 < 6) {
            continue;
        }
        ++longChains;
        distinctFirst += distinct(c.first);
        const size_t last = distinct(c.last - 2);
        distinctLast += last;
        settled += last == 1 ? 1 : 0;
    }
    size_t maxMinute = 0;
    for (size_t a = 0, b = 0; b < emissions.size(); ++b) {
        while (emissions[b].startMs - emissions[a].startMs >= 60000) {
            ++a;
        }
        maxMinute = std::max(maxMinute, b - a + 1);
    }
    auto pct = [](size_t num, size_t den) { return den ? 100.0 * static_cast<double>(num) / den : 0.0; };
    printf("fieldsim matrix=%s nodes=%d hours=%.1f seed=%lu %s offset=%d L=%lu jitter=%lu%s\n", matrix.c_str(), n,
           hours, seed, old ? "old(fixed answer, rest triggers 4/6)" : "level", static_cast<int>(cfg.levelOffset),
           static_cast<unsigned long>(cfg.levelMax), static_cast<unsigned long>(cfg.answerJitterMs),
           cfg.answerJitterShrinks ? " shrinking" : "");
    printf("  emissions %zu  chains %zu  mean %.1f  max %zu  rests %lu\n", emissions.size(), chains.size(),
           chains.empty() ? 0.0 : sumLen / chains.size(), maxLen, rests);
    printf("  idle calls igniting %.0f %% (%zu / %zu)\n", pct(ignited, idleStarts), ignited, idleStarts);
    printf("  gate: chains ending within 30 s %.0f %% (>= 80)  max per minute %zu (<= 100)\n",
           pct(ended30, chains.size()), maxMinute);
    printf("  gate: chord rounds first half %.0f %% (< 25), last half %.0f %% (> first)\n",
           pct(chordsFirst, roundsFirst), pct(chordsLast, roundsLast));
    printf("  gate: first answer later than 1 s in %.0f %% of ignited calls (most)\n", pct(lateFirst, firstAnswers));
    printf("  base (glide %s): chains >= 6 trains %zu, distinct bases first 3 %.2f -> last 3 %.2f, settled %.0f %%\n",
           cfg.baseGlide ? "on" : "off", longChains, longChains ? static_cast<double>(distinctFirst) / longChains : 0.0,
           longChains ? static_cast<double>(distinctLast) / longChains : 0.0, pct(settled, longChains));
    return 0;
}
