#pragma once

/*
ParamBindings (PAR-003)

The shared param descriptions in one include: each config struct describes its own fields as param paths, ids and
ranges next to its header (`param::describeParams(registry, config)`, review A10). The Node registers them all
(modes/resonant/ResonantNodeParams.h, NodeParams::describe, in the PARAM LIST order), the Analyzer its SEQ tuning,
the host replay the profile its --set flags override: same paths, same ranges, same ids on every build.

  detection.scalar_*                    detection/detectors/scalar/ScalarTransientParams.h
  detection.shape_*, *_medium           detection/inspection/InspectionParams.h
  detection.band_*                      detection/features/NetworkBand.h (the Node only)
  behavior.*                            behavior/BehaviorParams.h
  output.hop_*, output.idle_announce_*  modes/resonant/ResonantNodeSignalComposer.h (the Node only)
  output.silent, node.radio_enabled     modes/resonant/ResonantNodeParams.h (the Node only)
  node.radio_log                        modes/resonant/ResonantNodeParams.h; the echo modes register it too
  light.*                               modes/resonant/ResonantNodeLight.h (the Node), modes/echo/EchoLight.h
*/
#include "../behavior/BehaviorParams.h"
#include "../detection/detectors/scalar/ScalarTransientParams.h"
#include "../detection/inspection/InspectionParams.h"
