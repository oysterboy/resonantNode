p = "src/modes/resonant/ResonantNodeApp.cpp"
s = open(p, encoding="utf-8").read()
probe = '{ serialOut().print("PROBE stack %s free_min=" ); }'
def P(tag):
    return '    serialOut().print("PROBE %s free_min="); serialOut().println(static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)));\n' % tag
marks = [
 ("void Node::begin() {\n", "void Node::begin() {\n" + P("begin_start")),
 ("    configureParameters();\n    _audioSource.begin();\n", "    configureParameters();\n" + P("after_configure") + "    _audioSource.begin();\n" + P("after_audio_begin")),
 ("    applyActiveProfiles();\n    // Per-node", "    applyActiveProfiles();\n" + P("after_apply_profiles") + "    // Per-node"),
 ("    loadStoredParams();\n    {\n", "    loadStoredParams();\n" + P("after_load") + "    {\n"),
 ("    printSignalConfig();\n    printHopGapCheck();\n    printInspectionPlanCheck();\n", "    printSignalConfig();\n" + P("after_signal_config") + "    printHopGapCheck();\n    printInspectionPlanCheck();\n" + P("after_checks")),
 ("    _chirpOutput.begin();\n", "    _chirpOutput.begin();\n" + P("after_chirp_begin")),
]
for old, new in marks:
    assert s.count(old) == 1, old[:40]
    s = s.replace(old, new)
open(p, "w", encoding="utf-8", newline="").write(s)
