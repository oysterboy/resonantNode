/*
Command line for the host replay (NODE-006), see Replay.h.

    program [options] <capture>...        capture = .wav (tools/bench/rawcorpus.py) or a RAW .csv transcript
    program [options] --dir <folder>      every .csv in the folder (falls back to .wav)

Options
    --band LO-HI          band bank (default 2950-3950, the network band; "off" = single target at --target)
    --target HZ           single-target frequency (default 3200)
    --set path=value      profile override by param path, as PARAM SET on the board (app/ParamBindings.h):
                          detection.scalar_onset_threshold, detection.scalar_release_threshold,
                          detection.scalar_cooldown_ms, detection.scalar_min_duration_ms,
                          detection.scalar_max_duration_ms, detection.scalar_min_peak_strength,
                          detection.scalar_release_debounce_ms, detection.scalar_require_carrier_quality,
                          detection.scalar_require_min_strength, detection.scalar_min_strength,
                          detection.scalar_min_release_coverage_ms, detection.scalar_min_longest_island_ms,
                          detection.scalar_max_gap_ms, detection.contrast_medium, detection.amp_medium;
                          values are range-checked like on the board
    --preprocess none|dc|fd   force the PCM preprocessor (default: none for mode=both/feat transcripts and
                          .wav files, the build's preprocessor for mode=i2s/pcm transcripts)
    --tail-ms N           silence appended after the capture so the last occurrence can close (default 300)
    --skip-ms N           ignore verdicts whose occurrence starts before N ms (noise captures: the marker tone)
    --compare             with a mode=both transcript: the host amp feature against the board's column
    --verbose             one line per verdict
    --quiet               only the final summary

Exit code 0; output is plain text, one capture per line.
*/
#ifndef UNIT_TEST

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "Replay.h"
#include "../detection/evaluation/VerdictNames.h"

namespace {

bool endsWith(const std::string& s, const char* suffix) {
    const size_t n = strlen(suffix);
    return s.size() >= n && strcmp(s.c_str() + s.size() - n, suffix) == 0;
}

void usage() {
    printf("usage: program [--band LO-HI|off] [--target HZ] [--set path=value]... [--preprocess none|dc|fd]\n"
           "               [--tail-ms N] [--skip-ms N] [--compare] [--verbose] [--quiet] <capture>... | --dir <folder>\n");
}

}  // namespace

int main(int argc, char** argv) {
    using namespace hostreplay;
    Options opt;
    bool bandGiven = false;  // --band on the command line wins over a transcript's RAW_BEGIN band=
    bool quiet = false;
    std::vector<std::pair<std::string, std::string>> sets;
    std::vector<std::string> files;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&](const char* what) -> const char* {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s needs a value\n", what);
                exit(2);
            }
            return argv[++i];
        };
        if (a == "--band") {
            bandGiven = true;
            const std::string v = next("--band");
            if (v == "off") {
                opt.bandLow = opt.bandHigh = 0;
            } else {
                const size_t dash = v.find('-');
                opt.bandLow = strtoul(v.c_str(), nullptr, 10);
                opt.bandHigh = dash != std::string::npos ? strtoul(v.c_str() + dash + 1, nullptr, 10) : 0;
            }
        } else if (a == "--target") {
            opt.targetHz = strtoul(next("--target"), nullptr, 10);
        } else if (a == "--set") {
            const std::string v = next("--set");
            const size_t eq = v.find('=');
            if (eq == std::string::npos) {
                fprintf(stderr, "--set wants path=value\n");
                return 2;
            }
            sets.emplace_back(v.substr(0, eq), v.substr(eq + 1));
        } else if (a == "--preprocess") {
            const std::string v = next("--preprocess");
            opt.preprocessForced = true;
            opt.preprocess = v == "dc"   ? runtime::PcmPreprocessMode::DcBlocker
                             : v == "fd" ? runtime::PcmPreprocessMode::FirstDifference
                                         : runtime::PcmPreprocessMode::None;
        } else if (a == "--tail-ms") {
            opt.tailMs = strtoul(next("--tail-ms"), nullptr, 10);
        } else if (a == "--skip-ms") {
            opt.skipMs = strtoul(next("--skip-ms"), nullptr, 10);
        } else if (a == "--compare") {
            opt.compare = true;
        } else if (a == "--dump-freq" && i + 1 < argc) {
            opt.dumpFreqPath = argv[++i];
        } else if (a == "--verbose") {
            opt.verbose = true;
        } else if (a == "--quiet") {
            quiet = true;
        } else if (a == "--dir") {
            const std::string dir = next("--dir");
            std::vector<std::string> csv;
            std::vector<std::string> wav;
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                const std::string p = entry.path().string();
                if (endsWith(p, ".csv")) {
                    csv.push_back(p);
                } else if (endsWith(p, ".wav")) {
                    wav.push_back(p);
                }
            }
            std::vector<std::string>& pick = csv.empty() ? wav : csv;
            std::sort(pick.begin(), pick.end());
            files.insert(files.end(), pick.begin(), pick.end());
        } else if (a == "--help" || a == "-h") {
            usage();
            return 0;
        } else {
            files.push_back(a);
        }
    }
    if (files.empty()) {
        usage();
        return 2;
    }

    detection::DetectionProfile profile = detection::makeTonalPulseScalarProfile();
    for (const auto& kv : sets) {
        const char* status = applyProfileSet(profile, kv.first, kv.second);
        if (strcmp(status, "ok") != 0) {
            fprintf(stderr, "--set %s=%s: %s\n", kv.first.c_str(), kv.second.c_str(), status);
            return 2;
        }
    }
    if (!quiet) {
        printf("replay profile=%s band=%lu-%lu target=%lu contrast_medium=%.0f amp_medium=%.0f max_duration_ms=%lu\n",
               detection::detectionProfileName(profile.kind), opt.bandLow, opt.bandHigh, opt.targetHz,
               static_cast<double>(profile.inspectionPlan.modules[0].magnitude.supportStrength.mediumPeakThreshold),
               static_cast<double>(profile.inspectionPlan.modules[1].magnitude.supportStrength.mediumPeakThreshold),
               static_cast<unsigned long>(profile.scalarTransient.maxTransientDurationMs));
    }

    unsigned totalAccepted = 0;
    unsigned totalRejected = 0;
    unsigned loaded = 0;
    for (const std::string& path : files) {
        Capture cap;
        if (!loadCapture(path, cap)) {
            printf("%s: could not load\n", path.c_str());
            continue;
        }
        ++loaded;
        if (!quiet && opt.verbose) {
            printf("%s samples=%zu sr=%u mode=%s %s\n", cap.name.c_str(), cap.pcm.size(), cap.sampleRateHz,
                   cap.mode.c_str(), cap.shape.c_str());
        }
        // A transcript that records the board's band (RAW band=, step 7b) is
        // replayed on that band unless --band says otherwise, so --compare
        // compares like with like.
        Options capOpt = opt;
        if (!bandGiven && cap.hasBoardBand) {
            capOpt.bandLow = cap.boardBandLow;
            capOpt.bandHigh = cap.boardBandHigh;
        }
        const Result r = replay(cap, capOpt, profile);
        totalAccepted += r.accepted;
        totalRejected += r.rejected;
        if (!quiet) {
            printf("%-36s samples=%6zu accepted=%u rejected=%u", cap.name.c_str(), r.samples, r.accepted, r.rejected);
            if (r.skipped > 0) {
                printf(" skipped=%u", r.skipped);
            }
            if (!r.verdicts.empty()) {
                const detection::OccurrenceVerdict& v = r.verdicts.front();
                printf(" first: %s start_ms=%lu dur_ms=%lu strength=%.0f", v.accepted ? "accepted" : detection::verdictRejectReasonName(v.rejectReason),
                       v.primaryStartMs, v.primaryDurationMs, static_cast<double>(v.primaryStrength));
            }
            if (r.ampCompared > 0) {
                printf(" amp_vs_board: n=%zu mean_abs=%.1f max_abs=%.0f", r.ampCompared, r.ampMeanAbsDiff, r.ampMaxAbsDiff);
            }
            if (r.freqCompared > 0) {
                printf(" freq_vs_board: n=%zu mean_abs=%.1f max_abs=%.0f contrast_rel mean=%.4f max=%.4f", r.freqCompared,
                       r.freqMeanAbsDiff, r.freqMaxAbsDiff, r.contrastMeanRelDiff, r.contrastMaxRelDiff);
            }
            printf("\n");
        }
    }
    printf("summary captures=%u accepted=%u rejected=%u\n", loaded, totalAccepted, totalRejected);
    return 0;
}

#endif  // UNIT_TEST
