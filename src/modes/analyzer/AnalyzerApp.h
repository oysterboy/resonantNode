#pragma once

#include <stddef.h>

#include "../../hal/AudioSourceAnalog.h"
#include "../../hal/AudioSourceI2S.h"
#include "../../io/AudioOnsetDetector.h"
#include "../../io/AudioSignal.h"
#include "../../hal/AudioSource.h"

class AnalyzerApp {
public:
    enum class AudioSourceKind {
        Analog,
        I2S
    };

    AnalyzerApp(int inputPin = 34, AudioSourceKind sourceKind = AudioSourceKind::I2S);

    void begin();
    void update();
    unsigned long loopDelayMs() const;

private:
    // Per-session state bundles keep the long-running modes readable and isolated.
    struct SequenceTest {
        struct TrialDiagnostics {
            bool onsetSeen = false;
            bool transientAccepted = false;

            unsigned long firstOnsetMs = 0;
            unsigned long lastOnsetMs = 0;
            unsigned long acceptedTransientMs = 0;
            float acceptedTransientStrength = 0.0f;
            unsigned long acceptedTransientDurationMs = 0;

            unsigned long duplicateCount = 0;

            unsigned long onsetRejectBelowThreshold = 0;
            unsigned long onsetRejectPeakActive = 0;
            unsigned long onsetRejectCooldown = 0;
            unsigned long onsetRejectOther = 0;

            unsigned long firstOnsetRejectMs = 0;
            unsigned long lastOnsetRejectMs = 0;

            AudioOnsetDetector::TransientRejectReason lastTransientRejectReason = AudioOnsetDetector::TransientRejectReason::None;
            float lastRejectStrength = 0.0f;
            unsigned long lastRejectDurationMs = 0;
            bool peakActiveAtEnd = false;
        };

        bool active = false;
        bool quiet = false;
        bool showDetails = true;
        bool progressLineStarted = false;

        unsigned long totalTrials = 100;
        unsigned long periodMs = 2500;
        unsigned long windowStartOffsetMs = 0;
        unsigned long windowEndOffsetMs = 2200;
        unsigned long toneHz = 3200;
        unsigned long durationMs = 100;
        char setupLabel[48] = TEST_SETUP_LABEL;

        unsigned long startedAtMs = 0;
        unsigned long nextTriggerAtMs = 0;
        unsigned long currentTrial = 0;
        unsigned long currentTrialScheduledAtMs = 0;
        unsigned long currentTrialStartMs = 0;
        unsigned long currentTrialEndMs = 0;
        unsigned long currentTrialOnsetDetectedMs = 0;
        unsigned long currentTrialTransientDetectedMs = 0;
        bool currentTrialHit = false;
        bool currentTrialFinalized = false;
        unsigned long currentTrialUnexpected = 0;
        bool trialHadAudioOverflow = false;
        unsigned long trialOverflowCountAtStart = 0;
        TrialDiagnostics currentTrialDiagnostics;

        unsigned long hits = 0;
        unsigned long expectedHits = 0;
        unsigned long lateHits = 0;
        unsigned long earlyHits = 0;
        unsigned long misses = 0;
        unsigned long unexpected = 0;
        unsigned long duplicates = 0;
        unsigned long invalidAudio = 0;
        unsigned long samplesProcessed = 0;
        unsigned long maxSamplesPerLoop = 0;
        unsigned long emptySourceLoops = 0;
        unsigned long totalHitStrengthScaled = 0;
        unsigned long totalHitDurationMs = 0;
    };

    struct CaptureSession {
        bool active = false;
        bool quiet = false;

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
    };

    struct BaseSession {
        bool active = false;
        bool quiet = false;

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
    };

    // Setup and control helpers.
    void configureParameters();
    void configureSharedParameters();
    void configureAnalogParameters();
    void configureI2SParameters();
    void beginEmitterControl();
    void pollUsbConsole();
    void pollEmitterSerial();
    void handleUsbLine(const char* line);
    void sendEmitterCommand(const char* command);
    void resetDetectorState();
    bool detectorOnsetDetected() const;
    float detectorOnsetStrength() const;
    bool detectorTransientDetected() const;
    float detectorTransientStrength() const;
    unsigned long detectorTransientDurationMs() const;
    bool detectorTransientPeakActive() const;
    const char* detectorOnsetRejectReasonName() const;
    const char* detectorTransientRejectReasonName() const;
    unsigned long detectorTransientRejectedDurationMs() const;
    float detectorTransientRejectedStrength() const;
    float detectorOnsetDetectionThreshold() const;
    float detectorOnsetReleaseThreshold() const;
    unsigned long detectorCooldownAfterOnsetMs() const;
    unsigned long detectorMinTransientDurationMs() const;
    unsigned long detectorMaxTransientDurationMs() const;
    float detectorMinTransientPeakStrength() const;
    unsigned long detectorReleaseDebounceMs() const;
    void setDetectorOnsetDetectionThreshold(float value);
    void setDetectorOnsetReleaseThreshold(float value);
    void setDetectorCooldownAfterOnsetMs(unsigned long value);
    void setDetectorMinTransientDurationMs(unsigned long value);
    void setDetectorMaxTransientDurationMs(unsigned long value);
    void setDetectorMinTransientPeakStrength(float value);
    void setDetectorReleaseDebounceMs(unsigned long value);
    void startBaseSession(unsigned long durationMs, bool quiet = false);
    void stopBaseSession();
    void updateBaseSession(unsigned long now);
    void printBaseSummary() const;
    void printBaseHints() const;
    void startSequenceTest(unsigned long totalTrials, unsigned long periodMs, unsigned long windowEndOffsetMs, unsigned long toneHz, unsigned long durationMs, bool quiet = false, bool showDetails = true, const char* setupLabel = nullptr);
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
    void printAudioSourceSummary() const;
    void printSignalSummary() const;
    void printCaptureHints() const;
    void printDetectionParameters() const;
    void printTransientAcceptedDebug(unsigned long now, float strength, unsigned long durationMs) const;
    void printTransientStatsDebug(unsigned long now) const;
    void printSequenceOnsetRejectCounts(const SequenceTest::TrialDiagnostics& diagnostics) const;
    void printSequenceTrialResult(unsigned long trialNumber, const char* result, long dtMs, long durMs, float strength, bool audioOverflow, unsigned long duplicateCount) const;
    void printSequenceSummary() const;
    void handleSequenceCandidate(const DetectorCandidate& candidate);
    void printValueFrame(unsigned long now) const;
    void printValueModeBanner() const;

    // Hardware and signal chain.
    int _inputPin;
    AudioSourceAnalog _analogSource;
    AudioSourceI2S _i2sSource;
    AudioSource& _audioSource;
    AudioSourceKind _sourceKind;
    AudioSignal _audioSignal;
    AudioOnsetDetector _audioOnsetDetector;

    // Console and emitter control.
    unsigned long _controlBaudRate = 115200;
    int _controlRxPin = 16;
    int _controlTxPin = 17;
    char _usbLineBuffer[96];
    size_t _usbLineLength = 0;
    char _emitterLineBuffer[96];
    size_t _emitterLineLength = 0;

    // Value mode and detector latching.
    bool _valMode = false;
    mutable unsigned long _valOnsetLatchedUntilMs = 0;
    mutable unsigned long _valTransientLatchedUntilMs = 0;
    bool _controlClaimPending = false;
    bool _controlClaimSent = false;
    unsigned long _controlClaimAtMs = 0;

    // Session state.
    BaseSession _baseSession;
    SequenceTest _sequenceTest;
    CaptureSession _captureSession;

    // Print throttling for the VAL view.
    mutable unsigned long _lastPrintMs = 0;
    static constexpr unsigned long kPrintIntervalMs = 100;
};
