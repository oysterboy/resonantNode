p = "src/modes/resonant/ResonantNodeApp.cpp"
s = open(p, encoding="utf-8").read()
def P(tag):
    return '    serialOut().print("PROBE %s free_min="); serialOut().println(static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)));\n' % tag
old = """    _audioSignal.begin(false);
    _freqBandStream.resetState();
    _freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
    _freqBandStream.setTargetFrequencyHz(_chirpOutput.toneHz());
    _detection.resetState();
"""
assert s.count(old) == 1
new = """    _audioSignal.begin(false);
""" + P("after_signal_begin") + """    _freqBandStream.resetState();
    _freqBandStream.setSampleRateHz(_audioSource.sampleRateHz());
    _freqBandStream.setTargetFrequencyHz(_chirpOutput.toneHz());
""" + P("after_band_stream") + """    _detection.resetState();
""" + P("after_detection_reset")
s = s.replace(old, new)
open(p, "w", encoding="utf-8", newline="").write(s)
