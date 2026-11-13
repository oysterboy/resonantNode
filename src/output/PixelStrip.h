#pragma once

#include <stdint.h>

/*
PixelStrip

Owns: one WS2812 / SK6812 pixel strip on one GPIO, driven by the RMT peripheral (the legacy driver of
Arduino-ESP32 2.x, no library): the bit timing runs in hardware, so the CPU and the I2S loop are not held.
Every pixel gets the same colour (the light test of issue #94; no per-pixel features), and the build-time current
cap (LIGHT_MAX_MA in BoardConfig.h) is applied to every frame: the estimated current of the colour at the pixel
count is scaled down to the cap, never above it.
Does not own: what the light shows (a LightOutput fed by Behavior, OUT-007), the pin choice (BoardConfig.h) or the
strip's kind beyond the byte count per pixel (3: GRB, 4: GRBW).

One timing serves both chips: 0 = 350 ns high / 900 ns low, 1 = 700 ns high / 550 ns low (WS2812B 400 +- 150 /
800 +- 150 ns, SK6812 300 +- 150 / 600 +- 150 ns).
*/
class PixelStrip {
public:
    // pin < 0: no light, every show() does nothing. capMa: the ceiling of the estimated current of a frame.
    bool begin(int pin, uint16_t pixels, uint16_t capMa);

    // One colour on all pixels, sent as 3 bytes per pixel (WS2812: GRB on the wire).
    bool showRgb(uint8_t r, uint8_t g, uint8_t b);
    // The same with a fourth byte per pixel (SK6812 RGBW: GRBW on the wire). A strip of the other kind shifts its
    // colours: that is what the test looks at.
    bool showRgbw(uint8_t r, uint8_t g, uint8_t b, uint8_t w);

    // Even pixels one colour, odd pixels another (channels r, g, b, w; bytesPerPixel 3 or 4): a wrong byte count or
    // colour order for the strip then shows per pixel. bytesPerPixel 3 ignores w.
    bool showAlternating(const uint8_t* even, const uint8_t* odd, uint8_t bytesPerPixel);

    bool ready() const { return _ready; }
    uint16_t pixels() const { return _pixels; }
    uint16_t capMa() const { return _capMa; }
    // The last frame as sent (after the cap) and its estimated current.
    uint8_t sentR() const { return _sent[0]; }
    uint8_t sentG() const { return _sent[1]; }
    uint8_t sentB() const { return _sent[2]; }
    uint8_t sentW() const { return _sent[3]; }
    uint16_t estimatedMa() const { return _estimatedMa; }
    uint8_t scalePct() const { return _scalePct; }

private:
    bool show(const uint8_t* even, const uint8_t* odd, uint8_t bytesPerPixel);

    bool _ready = false;
    uint16_t _pixels = 0;
    uint16_t _capMa = 0;
    uint8_t* _frame = nullptr;
    uint8_t _sent[4] = {0, 0, 0, 0};
    uint16_t _estimatedMa = 0;
    uint8_t _scalePct = 100;
};
