# Detection Fix — Aggregated Feature History + Inspector Routing

## Ziel

Behebe den aktuellen Inspector-/History-Datenmismatch:

- AMP-Inspector meldet `sample_count=0`, obwohl ein Peak vorhanden ist.
- Die aktuelle Raw-Sample-History ist mit 128 Einträgen bei 16 kHz nur etwa 8 ms lang.
- Inspector-Fenster liegen typischerweise bei 100–130 ms.
- Reporter/Inspector-Zuordnung ist zusätzlich falsch bzw. uneindeutig.
- Die Lösung soll eine kompakte, zeitbasierte History mit 1-ms-Bins verwenden.

Keine Detector-Retuning-Arbeit in diesem Pass.

---

## Zielarchitektur

```text
FeatureExtractor
    -> aktuelle Featurewerte
FeatureHistory
    -> stream-spezifische 1-ms-Aggregation
    -> 256 Bins pro Stream
Inspector
    -> liest Bins im Candidate-Fenster
    -> berechnet Window-Statistiken
Analyzer
    -> berichtet exakt dieselben Inspector-Ergebnisse
```

`FeatureHistory` besitzt Aggregation und Speicherung.

Der Inspector darf keine eigene Live-Aggregation durchführen und nicht auf getrennte, kürzere Raw-Sample-Ringe zurückfallen.

---

## 1. History auf 1-ms-Bins umstellen

Pro Stream:

```cpp
static constexpr size_t kBinsPerStream = 256;
```

Das entspricht ungefähr 256 ms History.

Separate kurze Raw-Sample-Ringe entfernen, sofern sie ausschließlich für Inspector-Statistiken verwendet werden.

Keine profilabhängige Allokation in diesem Pass. Die History bleibt statisch reserviert, damit Profile weiterhin zur Laufzeit wechselbar bleiben.

---

## 2. Gemeinsame Zeitbasis, stream-spezifische Aggregation

Alle Streams verwenden dieselben 1-ms-Zeitgrenzen.

Die Aggregationsregel darf je Stream unterschiedlich sein.

Vorgeschlagene Kernstrukturen:

```cpp
struct FeatureHistoryBin {
    uint32_t startMs = 0;
    uint16_t inputCount = 0;
    uint16_t freshCount = 0;

    float mean = 0.0f;
    float rms = 0.0f;
    float peak = 0.0f;
    float meanAbs = 0.0f;
    float last = 0.0f;

    bool valid = false;
};
```

```cpp
struct FeatureBinAccumulator {
    uint32_t startMs = 0;
    uint16_t inputCount = 0;
    uint16_t freshCount = 0;

    double sum = 0.0;
    double sumSquares = 0.0;
    double sumAbs = 0.0;

    float peak = 0.0f;
    float last = 0.0f;
};
```

Die konkrete Struktur darf an vorhandene Typen angepasst werden. Keine Paralleltypen erzeugen, wenn bestehende `FeatureBin`-/History-Typen sauber erweitert werden können.

---

## 3. Aggregation sitzt in `FeatureHistory`

Öffentlicher Write-Pfad sinngemäß:

```cpp
void FeatureHistory::push(
    FeatureStreamId stream,
    float value,
    bool fresh,
    uint32_t timestampMs);
```

Intern:

```cpp
void FeatureHistory::aggregateIntoCurrentBin(
    StreamHistory& history,
    float value,
    bool fresh,
    uint32_t timestampMs);
```

Bei Wechsel der Millisekunde:

1. aktiven Bin finalisieren,
2. in den Ringbuffer schreiben,
3. fehlende Millisekunden optional als ungültige Bins überspringen oder markieren,
4. neuen Accumulator starten.

Keine Logik nach dem Muster „ein Sample = ein History-Eintrag“.

---

## 4. Aggregationsregeln

### AMP / Envelope

Für jeden Eingangswert:

```cpp
absValue = fabsf(value);
sum += value;
sumAbs += absValue;
sumSquares += value * value;
peak = max(peak, absValue);
last = value;
inputCount++;
freshCount++;
```

Finaler Bin:

```cpp
mean = sum / inputCount;
meanAbs = sumAbs / inputCount;
rms = sqrt(sumSquares / inputCount);
peak = peakAbs;
```

Für AMP-Support soll der Inspector standardmäßig einen Bin-Repräsentanten verwenden, z. B.:

```text
bin metric = meanAbs
window metric = p75
```

RMS bleibt als alternative Inspector-Metrik verfügbar.

### FrequencyTarget / FrequencyContrast

Nur frische Updates zählen als Evidenz:

```cpp
if (!fresh) {
    return; // held value nicht aggregieren
}
```

Für frische Werte:

- `mean`
- `rms`
- `peak`
- `last`
- `freshCount`

berechnen.

Ein Bin ohne frisches Frequency-Update ist für Frequency-Evidenz ungültig.

Held values dürfen weiterhin außerhalb der History für Status/Plot vorhanden sein, aber nicht als neue Inspector-Evidenz zählen.

### Weitere langsame Streams

Falls vorhanden:

- `last` pro Millisekunde,
- `valid=true`, sobald mindestens ein relevanter Input vorliegt.

Keine generische Policy-Factory bauen. Kleine explizite stream-spezifische Funktionen genügen:

```cpp
aggregateAmp(...)
aggregateFreshFrequency(...)
aggregateLastValue(...)
```

---

## 5. Inspector liest nur die aggregierte History

Der Inspector selektiert Bins nach Zeit:

```text
candidate.startMs <= bin.startMs <= candidate.endMs
```

oder entsprechend der vorhandenen Fenstersemantik.

Aus den selektierten Bin-Repräsentanten werden berechnet:

- `sampleCount` = Anzahl gültiger Bins, nicht Anzahl Rohsamples
- `coverage`
- `peak`
- `mean`
- `rms`
- `median`
- `p75`
- `p90`
- `trimmedMean`

Wichtig:

- `sampleCount` sollte in der Ausgabe klar als Bin-Anzahl verstanden werden.
- Optional später in `bin_count` umbenennen; in diesem Pass nur ändern, wenn ohne große Output-Kompatibilitätskosten möglich.
- Peak und Verteilungsstatistiken müssen aus derselben selektierten Datenmenge stammen.
- Kein separates Peak-Feld aus einer anderen History oder Live-Quelle einmischen.

Damit darf nicht mehr auftreten:

```text
peak > 0
sample_count = 0
p75 = 0
```

außer ein explizit dokumentierter anderer Peak-Typ wird separat benannt.

---

## 6. Inspector-/Reporter-Routing korrigieren

Prüfe die Zuordnung zwischen:

- Inspector Observation Index
- Observation Target
- Stream
- Label
- Metric

Der aktuelle Pfad scheint Display-Index und Array-Index zu vermischen.

Falls intern 0-basiert und im Log 1-basiert:

```cpp
const size_t arrayIndex = observationIndex;
const size_t displayIndex = observationIndex + 1;
```

Nicht `displayIndex` zum Arrayzugriff verwenden.

Ziel:

```text
observation 1:
  label=contrast
  stream=FrequencyContrast

observation 2:
  label=amp
  stream=AmpEnvelope
```

Nicht:

```text
label=contrast
compare.label=amp
compare.stream=FrequencyContrast
```

`AmpEnvelope` im Reporter nicht als generisches `Scalar` ausgeben, sofern dadurch die tatsächliche Quelle verschleiert wird.

---

## 7. Datenvertrag säubern

Ein Inspector-Ergebnis soll seine Quelle explizit tragen:

```cpp
struct InspectionEvidence {
    FeatureStreamId stream;
    InspectionMetric metric;
    const char* label; // oder bestehender stabiler Label-Typ

    WindowStats stats;
    StrengthClass strengthClass;
};
```

Keine doppelten Wahrheiten:

- Stream kommt aus Inspector-Konfiguration.
- Statistik kommt aus genau diesem Stream und Fenster.
- Analyzer druckt nur diese Daten.
- Kein nachträgliches Umschreiben von Label oder Stream anhand der Observation-Position.

Bestehende Typen verwenden/erweitern, wenn möglich.

---

## 8. Profil bleibt deklarativ

`TonalPulseScalar` darf weiterhin festlegen:

- welcher Stream inspiziert wird,
- welche Bin-Metrik verwendet wird,
- welche Window-Metrik verwendet wird,
- welche Strength-Schwellen gelten,
- ob eine Observation erforderlich oder optional ist.

Das Profil darf nicht:

- History-Kapazität allokieren,
- Aggregationscode besitzen,
- eigene Ringspeicher erzeugen.

Beispielkonzept:

```text
contrast:
  stream = FrequencyContrast
  bin_metric = mean
  window_metric = p75
  required = true

amp:
  stream = AmpEnvelope
  bin_metric = meanAbs
  window_metric = p75
  required = true/false according to profile
```

Keine Profil-Schwellen in diesem Pass neu tunen.

---

## 9. Tests und Akzeptanzkriterien

### Unit-/Host-Tests, falls Infrastruktur vorhanden

1. 16 AMP-Werte innerhalb derselben Millisekunde:
   - ein Bin,
   - `inputCount=16`,
   - korrekte `meanAbs`, `rms`, `peak`.

2. Wechsel über mehrere Millisekunden:
   - korrekte Bin-Grenzen,
   - korrekte Reihenfolge im Ring.

3. Mehr als 256 ms:
   - Ring überschreibt nur älteste Bins,
   - letzte 256 ms bleiben lesbar.

4. Frequency held/fresh:
   - held Updates erhöhen `freshCount` nicht,
   - Bin ohne fresh Update ist für Frequency-Evidenz ungültig.

5. Inspector-Fenster 100–130 ms:
   - erhält ungefähr 100–130 gültige AMP-Bins,
   - keine leere Statistik bei vorhandenem Signal.

6. Reporter:
   - Observation 1/2 zeigen korrekte Labels und Streams,
   - kein Off-by-one-Zugriff.

### Hardware-Sequenztest

Mit bestehendem 3200-Hz-/100-ms-Test:

Erwartung:

```text
detector_accepted_trials = 20
AMP sample_count/bin_count > 0
AMP p75/rms/median > 0
contrast stream correctly labeled
amp stream correctly labeled
```

Nicht akzeptabel:

```text
compare.peak > 0
compare.sample_count = 0
```

Zusätzlich:

- keine Buffer Overruns,
- Detector-Timing nicht verändert,
- keine Änderung der bestehenden Detector-Akzeptanzlogik,
- keine neue Duplicate-Occurrence-Erzeugung.

---

## 10. Reporting-Hinweis

Beim SEQ-Start einmal ausgeben:

```text
history.bin_ms=1
history.capacity_bins=256
history.coverage_ms=256
```

Optional pro Stream:

```text
history.stream=amp aggregation=mean_abs+rms+peak_abs
history.stream=freq_target aggregation=fresh_only
history.stream=freq_contrast aggregation=fresh_only
```

So ist später sichtbar, auf welcher Evidenzbasis der Inspector arbeitet.

---

## Nicht-Ziele

- Keine Detector-Schwellen neu tunen.
- Keine Profile automatisch anhand des RAM-Bedarfs instanziieren.
- Keine dynamische Heap-Allokation.
- Keine generische Policy-/Factory-Architektur.
- Keine Änderung der Pattern-Semantik, außer dass korrekte Inspector-Daten ankommen.
- Keine Rohsample-Capture-/Debug-Funktion entfernen, falls sie separat für RAW_CAPTURE benötigt wird. Nur Inspector-History von kurzen Rohsample-Ringen entkoppeln.

---

## Empfohlener Commit

```text
DetectionFix: aggregate feature history into 1 ms bins
```

```text
- replace short inspector raw-sample history with 256 x 1 ms bins
- aggregate AMP and fresh-only frequency streams in FeatureHistory
- make inspector statistics use one consistent binned data source
- fix observation target/index routing and stream labels
- preserve detector and pattern tuning
```
