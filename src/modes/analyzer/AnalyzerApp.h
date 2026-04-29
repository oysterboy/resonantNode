#pragma once

#include <stddef.h>

#include "../../hal/AudioSourceAnalog.h"
#include "../../hal/AudioSourceI2S.h"
#include "../../io/AudioFrequencyDetector.h"
#include "../../io/AudioOnsetDetector.h"
#include "../../io/AudioSignal.h"
#include "../../hal/AudioSource.h"

class AnalyzerApp {
public:
    enum class AudioSourceKind {
        Analog,
        I2S
    };

    enum class DetectorKind {
        Amplitude,
        Frequency
    };

    AnalyzerApp(int inputPin = 34, AudioSourceKind sourceKind = AudioSourceKind::I2S);

    void begin();
    void update();
    unsigned long loopDelayMs() const;

private:
    struct SequenceTest {
        bool active = false;
        unsigned long totalTrials = 100;
        unsigned long periodMs = 2500;
        unsigned long windowStartOffsetMs = 0;
        unsigned long windowEndOffsetMs = 2200;
        unsigned long toneHz = 3200;
        unsigned long durationMs = 100;
        unsigned long startedAtMs = 0;
        unsigned long nextTriggerAtMs = 0;
        unsigned long currentTrial = 0;
        unsigned long currentTrialStartMs = 0;
        unsigned long currentTrialEndMs = 0;
        bool currentTrialHit = false;
        bool currentTrialFinalized = false;
        unsigned long currentTrialUnexpected = 0;
        unsigned long hits = 0;
        unsigned long misses = 0;
        unsigned long unexpected = 0;
        unsigned long duplicates = 0;
        unsigned long totalHitStrengthScaled = 0;
        unsigned long totalHitDurationMs = 0;
        unsigned long lastStatusPrintMs = 0;
        bool quiet = false;
        bool progressLineStarted = false;
        bool showDetails = true;
    };

    struct CaptureSession {
        bool active = false;
        unsigned long totalTrials = 20;
        unsigned long periodMs = 2500;
        unsigned long windowStartOffsetMs = 0;
        unsigned long windowEndOffsetMs = 500;
        unsigned long toneHz = 3200;
        unsigned long durationMs = 100;
        unsigned long startedAtMs = 0;
        unsigned long nextTriggerAtMs = 0;
        unsigned long currentTrial = 0;
        unsigned long currentTrialStartMs = 0;
        unsigned long currentTrialEndMs = 0;
        bool currentTrialFinalized = false;
        int currentRawMin = 0;
        int currentRawMax = 0;
        float currentDeltaMin = 0.0f;
        float currentDeltaMax = 0.0f;
        int quietRawMin = 0;
        int quietRawMax = 0;
        unsigned long quietRawSum = 0;
        unsigned long quietRawSamples = 0;
        float quietDeltaMin = 0.0f;
        float quietDeltaMax = 0.0f;
        float quietDeltaSum = 0.0f;
        unsigned long quietDeltaSamples = 0;
        unsigned long completed = 0;
        unsigned long totalRawSwing = 0;
        float totalDeltaSwing = 0.0f;
        int bestRawSwing = 0;
        float bestDeltaSwing = 0.0f;
        unsigned long lastStatusPrintMs = 0;
        bool quiet = false;
    };

    struct BaseSession {
        bool active = false;
        unsigned long durationMs = 10000;
        unsigned long startedAtMs = 0;
        unsigned long lastStatusPrintMs = 0;
        unsigned long ignoredRawSamples = 0;
        unsigned long samples = 0;
        unsigned long rawSum = 0;
        int rawMin = 0;
        int rawMax = 0;
        float deltaSum = 0.0f;
        float deltaMin = 0.0f;
        float deltaMax = 0.0f;
        float baselineSum = 0.0f;
        float baselineMin = 0.0f;
        float baselineMax = 0.0f;
        bool quiet = false;
    };

    struct TuneSession {
        enum class Stage {
            MinStrength,
            ReleaseDebounce,
            MinDuration,
        };

        bool active = false;
        bool waitingForSequenceResult = false;
        Stage stage = Stage::MinStrength;
        unsigned long stageCandidateStart = 0;
        unsigned long stageCandidateStop = 0;
        unsigned long stageCandidateStep = 0;
        unsigned long currentCandidateValue = 0;
        unsigned long currentMinStrength = 0;
        unsigned long currentReleaseDebounce = 0;
        unsigned long currentMinDuration = 0;
        unsigned long bestMinStrength = 0;
        unsigned long bestReleaseDebounce = 0;
        unsigned long bestMinDuration = 0;
        unsigned long bestSuccessRate = 0;
        unsigned long bestHits = 0;
        unsigned long bestMisses = 0;
        unsigned long bestUnexpected = 0;
        unsigned long bestDuplicates = 0;
        unsigned long stageTotalCandidates = 0;
        unsigned long stageCandidateIndex = 0;
        unsigned long tries = 100;
        unsigned long periodMs = 2500;
        unsigned long windowEndOffsetMs = 2200;
        unsigned long toneHz = 3200;
        unsigned long durationMs = 100;
        unsigned long stage2Min = 4;
        unsigned long stage2Max = 20;
        unsigned long stage2Step = 4;
        unsigned long stage3Min = 8;
        unsigned long stage3Max = 32;
        unsigned long stage3Step = 4;
    };

    void configureParameters();
    void configureSharedParameters();
    void configureAnalogParameters();
    void configureI2SParameters();
    void configureAmplitudeDetector();
    void configureFrequencyDetector();
    void beginEmitterControl();
    void pollUsbConsole();
    void pollEmitterSerial();
    void handleUsbLine(const char* line);
    void sendEmitterCommand(const char* command);
    void setDetectorKind(DetectorKind kind);
    void resetDetectorState();
    bool detectorOnsetDetected() const;
    float detectorOnsetStrength() const;
    bool detectorTransientDetected() const;
    float detectorTransientStrength() const;
    unsigned long detectorTransientDurationMs() const;
    float detectorFrequencyScore() const;
    float detectorFrequencyTargetPower() const;
    float detectorFrequencyNeighborPower() const;
    float detectorFrequencyTotalEnergy() const;
    float detectorFrequencySpectralContrast() const;
    float detectorFrequencyBinSpacingHz() const;
    float detectorOnsetDetectionThreshold() const;
    float detectorOnsetReleaseThreshold() const;
    unsigned long detectorCooldownAfterOnsetMs() const;
    unsigned long detectorMinTransientDurationMs() const;
    unsigned long detectorMaxTransientDurationMs() const;
    float detectorMinTransientPeakStrength() const;
    unsigned long detectorReleaseDebounceMs() const;
    unsigned long detectorTargetFrequencyHz() const;
    unsigned long detectorSampleRateHz() const;
    unsigned long detectorWindowSizeSamples() const;
    void setDetectorOnsetDetectionThreshold(float value);
    void setDetectorOnsetReleaseThreshold(float value);
    void setDetectorCooldownAfterOnsetMs(unsigned long value);
    void setDetectorMinTransientDurationMs(unsigned long value);
    void setDetectorMaxTransientDurationMs(unsigned long value);
    void setDetectorMinTransientPeakStrength(float value);
    void setDetectorReleaseDebounceMs(unsigned long value);
    void setDetectorTargetFrequencyHz(unsigned long value);
    void setDetectorSampleRateHz(unsigned long value);
    void setDetectorWindowSizeSamples(unsigned long value);
    const char* detectorKindName() const;
    void startBaseSession(unsigned long durationMs, bool quiet = false);
    void stopBaseSession();
    void updateBaseSession(unsigned long now);
    void printBaseSummary() const;
    void printBaseHints() const;
    void printFrequencyDebugSummary(const char* prefix) const;
    void startSequenceTest(unsigned long totalTrials, unsigned long periodMs, unsigned long windowEndOffsetMs, unsigned long toneHz, unsigned long durationMs, bool quiet = false, bool showDetails = true);
    void stopSequenceTest();
    void updateSequenceTest(unsigned long now);
    void handleSequenceTransient(unsigned long now);
    void finalizeSequenceTrial(unsigned long now);
    void printCaptureSummary() const;
    void startCaptureSession(unsigned long totalTrials, unsigned long periodMs, unsigned long windowEndOffsetMs, unsigned long toneHz, unsigned long durationMs, bool quiet = false);
    void stopCaptureSession();
    void updateCaptureSession(unsigned long now);
    void updateCaptureQuietStats(unsigned long now);
    void updateCaptureTrial(unsigned long now);
    void finalizeCaptureTrial(unsigned long now);
    void startTuneSession(unsigned long startMinStrength, unsigned long stopMinStrength, unsigned long stepMinStrength,
                          unsigned long startReleaseDebounce, unsigned long stopReleaseDebounce, unsigned long stepReleaseDebounce,
                          unsigned long startMinDuration, unsigned long stopMinDuration, unsigned long stepMinDuration,
                          unsigned long tries, unsigned long periodMs, unsigned long windowEndOffsetMs, unsigned long toneHz, unsigned long durationMs);
    void stopTuneSession();
    void updateTuneSession(unsigned long now);
    void startNextTuneCandidate(unsigned long now);
    void recordTuneCandidateResult();
    bool advanceTuneStage();
    void printCaptureHints() const;
    void printDetectionParameters() const;
    void printTransientAcceptedDebug(unsigned long now, float strength, unsigned long durationMs) const;
    void printTransientStatsDebug(unsigned long now) const;
    void printSequenceStatus(unsigned long now) const;
    void printSequenceSummary() const;
    void printTuneStageSummary() const;
    void printTuneSummary() const;
    void printValueFrame(unsigned long now) const;
    void printValueModeBanner() const;

    int _inputPin;
    AudioSourceAnalog _analogSource;
    AudioSourceI2S _i2sSource;
    AudioSource& _audioSource;
    AudioSourceKind _sourceKind;
    DetectorKind _detectorKind = DetectorKind::Amplitude;
    AudioSignal _audioSignal;
    AudioOnsetDetector _audioOnsetDetector;
    AudioFrequencyDetector _audioFrequencyDetector;
    unsigned long _controlBaudRate = 115200;
    int _controlRxPin = 16;
    int _controlTxPin = 17;
    char _usbLineBuffer[96];
    size_t _usbLineLength = 0;
    char _emitterLineBuffer[96];
    size_t _emitterLineLength = 0;
    bool _valMode = false;
    mutable unsigned long _valOnsetLatchedUntilMs = 0;
    mutable unsigned long _valTransientLatchedUntilMs = 0;
    bool _controlClaimPending = false;
    bool _controlClaimSent = false;
    unsigned long _controlClaimAtMs = 0;
    BaseSession _baseSession;
    SequenceTest _sequenceTest;
    CaptureSession _captureSession;
    TuneSession _tuneSession;
    mutable unsigned long _lastPrintMs = 0;
    static constexpr unsigned long kPrintIntervalMs = 100;
};
