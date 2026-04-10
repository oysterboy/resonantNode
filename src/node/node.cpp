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
    _levelInput.update();
_behavior.update(_levelInput.energy(), millis());

if (_behavior.shouldStartChirp()) {
    Serial.println("NODE: chirp start");
    _chirpOutput.start();
}

_chirpOutput.update();

bool chirpNowActive = _chirpOutput.isActive();

if (_chirpWasActive && !chirpNowActive) {
    Serial.println("NODE: chirp finished");
    _behavior.notifyChirpFinished(millis());
}

_chirpWasActive = chirpNowActive;

    

   


    //DEBUG
   //digitalWrite(_ledPin, _behavior.isActive() ? HIGH : LOW);
    digitalWrite(_ledPin, _chirpOutput.isActive() ? HIGH : LOW);
    float raw = _levelInput.raw() / 4095.0f;
    float energy = _levelInput.energy() / 300.0f;
    float smooth = _levelInput.smoothed() / 300.0f;
    float activity = _behavior.activity();
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
    if (_behavior.shouldStartChirp()) {
    Serial.println("NODE: chirp start");
    _chirpOutput.start();
}
if (_chirpOutput.isActive()) {
    Serial.println("CHIRPING");
}
}