Codex instruction:

Inspect ScalarTransientDetector.cpp, ScalarTransientOccurrence.cpp, and ScalarTransientReport.cpp.

Delete all obsolete #if 0 duplicate implementations from ScalarTransientDetector.cpp.
Remove duplicated/unneeded local helpers from Detector.cpp.
Keep lifecycle and accept/reject decisions in Detector.cpp.
Keep pending occurrence construction and popOccurrence() in Occurrence.cpp.
Keep accepted/rejected summaries and reject classification in Report.cpp.
Preserve behavior, output keys, and public APIs.
Compile after cleanup and report remaining duplicate symbols or responsibility overlap.