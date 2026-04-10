#pragma once

class ResonantBehavior {
public:
    void update(float inputLevel);

    float activity() const;
    bool isActive() const;

private:
    float _activity = 0.0f;
};