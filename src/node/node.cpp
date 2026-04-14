#include "node.h"
#include <Arduino.h>

Node::Node(int inputPin, int ledPin, int chirpPin)
    : _ledPin(ledPin),
      _analogIn(inputPin),
      _levelInput(_analogIn),
      _chirpOutput(chirpPin) {}

void Node::begin() {
    pinMode(_ledPin, OUTPUT);
    _levelInput.begin();
    _chirpOutput.begin();
}

void Node::update() {
    const unsigned long now = millis();

    _levelInput.update();
    _behavior.update(_levelInput.activityPresent(), _levelInput.activityLevel(), now);

    if (_behavior.shouldStartChirp()) {
        printEvent("chirp_start");
        _chirpOutput.start();
    }

    _chirpOutput.update();

    if (_chirpOutput.finished()) {
        printEvent("chirp_finished");
        _behavior.notifyChirpFinished(now);
    }

    digitalWrite(_ledPin, _chirpOutput.isActive() ? HIGH : LOW);

    printPlotValues(now);
}

void Node::printEvent(const char* event) {
    if (!_debugEvents) return;

    Serial.print("EVT ");
    Serial.println(event);
}

void Node::printPlotValues(unsigned long now) {
    if (!_debugPlot) return;
    if (now < 1000) return;
    if (now - _lastDebugPrintMs < _debugIntervalMs) return;

    _lastDebugPrintMs = now;

    const float centeredSignal = _levelInput.centeredSignal() / 300.0f;
    const float signalMagnitude = _levelInput.signalMagnitude() / 300.0f;
    const float smoothedSignalMagnitude = _levelInput.smoothedSignalMagnitude() / 300.0f;
    const float activity = _behavior.activity();
    const int state = _behavior.stateCode();
    const int chirp = _chirpOutput.isActive() ? 1 : 0;

    Serial.print("centered:");
    Serial.print(centeredSignal, 3);
    Serial.print(" magnitude:");
    Serial.print(signalMagnitude, 3);
    Serial.print(" smooth:");
    Serial.print(smoothedSignalMagnitude, 3);
    Serial.print(" activity:");
    Serial.print(activity, 3);
    Serial.print(" state:");
    Serial.print(state);
    Serial.print(" chirp:");
    Serial.println(chirp);
}
