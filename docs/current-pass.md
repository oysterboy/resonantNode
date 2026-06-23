Codex-Instruktion: Inspector-Strings zentralisieren

Zentralisiere alle Inspector-bezogenen Namen und Labels in:

src/detection/inspection/InspectionNames.h
1. Keine semantischen const char* label mehr

"amp", "target", "contrast" und "band" werden aktuell als Log-Text und für Programmlogik verwendet. Ersetze:

const char* label;

durch:

enum class InspectionTarget {
    None,
    Amp,
    TargetScore,
    Contrast,
    TargetBand,
};

in InspectorTypes.h.

InspectionModuleConfig erhält:

InspectionTarget target = InspectionTarget::None;
2. Zentrale Textabbildung

In InspectionNames.h ausschließlich:

const char* inspectionTargetName(InspectionTarget);
const char* inspectionModuleKindName(InspectionModuleKind);
const char* scalarInspectionModeName(ScalarInspectionMode);
const char* scalarInspectionBasisName(ScalarInspectionBasis);
const char* scalarInspectionNoteName(ScalarInspectionNote);
const char* scalarInspectionAnchorName(ScalarInspectionAnchor);
const char* strengthClassName(StrengthClass);

scalarInspectionModeName() aus InspectorTypes.h nach InspectionNames.h verschieben. InspectorTypes.h darf keine Ausgabetexte enthalten.

3. Stringvergleiche entfernen

Ersetze alle:

strcmp(label, "amp")
strcmp(label, "target")
strcmp(label, "contrast")
strcmp(label, "band")

durch switch (target).

Betroffene Stellen mindestens:

OccurrenceInspector.cpp
PatternMatcher.cpp
AnalyzerModeApp.cpp
AnalyzerSeqReporter.cpp
DetectionProfile.h
ResonantNodeApp.cpp
4. Reports typisiert halten

Ersetze Felder wie:

const char* inspectionObservationLabels[];
const char* firstFailedLabel;

durch enum-basierte Felder:

InspectionTarget inspectionObservationTargets[];
InspectionTarget firstFailedTarget;

Strings erst beim Serialisieren über inspectionTargetName() erzeugen.

Bestehende Analyzer-Ausgabewerte bleiben kompatibel:

amp
target
contrast
band
none
5. Scope
Keine Detection- oder Pattern-Logik ändern.
Keine Output-Keys ändern.
Keine neuen dynamischen Strings.
Profile setzen Targets per Enum.
Am Ende nach verbliebenen Inspector-Stringvergleichen und direkten Namen suchen.
Analyzer- und ResonantNode-Build kompilieren.