#include "node.h"
#include <Arduino.h>

Node::Node(int inputPin, int ledPin)
    : _ledPin(ledPin),
      _analogIn(inputPin),
      _levelInput(_analogIn) {}

void Node::begin() {
    pinMode(_ledPin, OUTPUT);
    _levelInput.begin();
}

void Node::update() {
    _levelInput.update();
    _behavior.update(_levelInput.energy());

    


    // DEBUG
    digitalWrite(_ledPin, _behavior.isActive() ? HIGH : LOW);
    float raw = _levelInput.raw()/4095.0f;
    float energy = _levelInput.energy()/300.0f;
    float smooth = _levelInput.smoothed()/300.0f;
    float activity=_behavior.activity();
    Serial.print("r:");
    Serial.print(raw);
    Serial.print(" e:");
    Serial.print(energy);
    Serial.print(" s:");
    Serial.print(smooth);
    Serial.print(" act:");
    Serial.println(activity);
}