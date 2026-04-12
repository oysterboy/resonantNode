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
    _behavior.update(_levelInput.energy(), now);

    if (_behavior.shouldStartChirp()) {
        Serial.println("NODE: chirp start");
        _chirpOutput.start();
    }

    _chirpOutput.update();

    if (_chirpOutput.finished()) {
        Serial.println("NODE: chirp finished");
        _behavior.notifyChirpFinished(now);
    }

    digitalWrite(_ledPin, _chirpOutput.isActive() ? HIGH : LOW);

    // Value debug
    float raw = _levelInput.raw() / 4095.0f;
    float energy = _levelInput.energy() / 300.0f;
    float smooth = _levelInput.smoothed() / 300.0f;
    float activity = _behavior.activity();

    (void)raw;
    (void)energy;
    (void)smooth;
    (void)activity;
/*
    Serial.print("r:");
    Serial.print(raw);
    Serial.print(" e:");
    Serial.print(energy);
    Serial.print(" s:");
    Serial.print(smooth);
    Serial.print(" act:");
    Serial.println(activity);
*/
}
