MEMORY STACK ANANLYSER

The main stack candidates are:
1. AnalyzerApp::startSequenceTest()
   - Stack frame: 0x670 = 1648 bytes
   - Many local configuration variables remain live throughout the function.
2. AnalyzerApp::update()
   - Stack frame: 1600 bytes
   - Calls processPendingSequenceStart(), which calls startSequenceTest().
   - This creates nested usage before audio processing even begins.
3. DetectionRuntime::resetDetectionState()
   - Stack frame: 0x610 = 1552 bytes
   - Calls additional reset functions while its own frame remains active.
4. Detector reset functions
   - FrequencyMatchDetector::resetState(): 416 bytes
   - ScalarTransientDetector::resetState(): 416 bytes
   - These are nested under resetDetectionState().
5. resetDiagnosticsCounters()
   - Stack frame: 464 bytes
6. waitForEmitterAck()
   - Stack frame: 144 bytes
   - Includes local char line[96].
   - Relevant only for normal SEQ, not OBS.
The dangerous startup nesting is approximately:
update()
  processPendingSequenceStart()
    startSequenceTest()
      resetDetectionState()
        resetDetectors()
          detector reset
The SequenceTest object at 9360 bytes and AnalyzerReport at 1172 bytes are members, not stack locals. They consume RAM, but they are not directly responsible for the loop-task stack overflow.
The strongest candidates to reduce are startSequenceTest() and resetDetectionState() nesting. Keeping the loop stack at 16384 is the immediate mitigation; splitting startup into smaller functions or moving configuration locals into persistent/member state would reduce the underlying risk.