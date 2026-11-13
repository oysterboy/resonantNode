#include "PixelStrip.h"

#include <Arduino.h>
#include <driver/rmt.h>
#include <stdlib.h>
#include <string.h>

namespace {

constexpr rmt_channel_t kChannel = RMT_CHANNEL_0;
constexpr uint8_t kClockDiv = 2;  // 80 MHz / 2 = 40 MHz: one tick is 25 ns
constexpr uint16_t kT0High = 14;  // 350 ns
constexpr uint16_t kT0Low = 36;   // 900 ns
constexpr uint16_t kT1High = 28;  // 700 ns
constexpr uint16_t kT1Low = 22;   // 550 ns
// One colour channel of one pixel at full level draws about this much (the data sheets' 20 mA per colour).
constexpr float kChannelFullMa = 20.0f;

// Turns the frame's bytes into RMT items, most significant bit first; the driver calls this from its interrupt.
void IRAM_ATTR translate(const void* src, rmt_item32_t* dest, size_t srcSize, size_t wantedItems,
                         size_t* translatedSize, size_t* itemCount) {
    if (src == nullptr || dest == nullptr) {
        *translatedSize = 0;
        *itemCount = 0;
        return;
    }
    rmt_item32_t bit0;
    bit0.level0 = 1;
    bit0.duration0 = kT0High;
    bit0.level1 = 0;
    bit0.duration1 = kT0Low;
    rmt_item32_t bit1;
    bit1.level0 = 1;
    bit1.duration0 = kT1High;
    bit1.level1 = 0;
    bit1.duration1 = kT1Low;

    size_t size = 0;
    size_t items = 0;
    const uint8_t* bytes = static_cast<const uint8_t*>(src);
    while (size < srcSize && items + 8 <= wantedItems) {
        for (int bit = 7; bit >= 0; --bit) {
            dest->val = (bytes[size] >> bit) & 1 ? bit1.val : bit0.val;
            ++dest;
            ++items;
        }
        ++size;
    }
    *translatedSize = size;
    *itemCount = items;
}

}  // namespace

bool PixelStrip::begin(int pin, uint16_t pixels, uint16_t capMa) {
    _ready = false;
    if (pin < 0 || pixels == 0) {
        return false;
    }
    free(_frame);
    _frame = static_cast<uint8_t*>(malloc(static_cast<size_t>(pixels) * 4U));
    if (_frame == nullptr) {
        return false;
    }
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(static_cast<gpio_num_t>(pin), kChannel);
    config.clk_div = kClockDiv;
    if (rmt_config(&config) != ESP_OK) {
        return false;
    }
    // The driver may already be installed (a second begin()): that is not an error.
    const esp_err_t installed = rmt_driver_install(config.channel, 0, 0);
    if (installed != ESP_OK && installed != ESP_ERR_INVALID_STATE) {
        return false;
    }
    if (rmt_translator_init(config.channel, translate) != ESP_OK) {
        return false;
    }
    _pixels = pixels;
    _capMa = capMa;
    _ready = true;
    showRgb(0, 0, 0);  // the strip starts dark
    return true;
}

bool PixelStrip::showRgb(uint8_t r, uint8_t g, uint8_t b) {
    const uint8_t channels[4] = {r, g, b, 0};
    return show(channels, channels, 3);
}

bool PixelStrip::showRgbw(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    const uint8_t channels[4] = {r, g, b, w};
    return show(channels, channels, 4);
}

bool PixelStrip::showAlternating(const uint8_t* even, const uint8_t* odd, uint8_t bytesPerPixel) {
    return show(even, odd, bytesPerPixel);
}

bool PixelStrip::show(const uint8_t* even, const uint8_t* odd, uint8_t bytesPerPixel) {
    if (!_ready) {
        return false;
    }
    float evenLevel = 0.0f;
    float oddLevel = 0.0f;
    for (uint8_t i = 0; i < bytesPerPixel; ++i) {
        evenLevel += even[i] / 255.0f;
        oddLevel += odd[i] / 255.0f;
    }
    const uint16_t evenCount = (_pixels + 1) / 2;
    const uint16_t oddCount = _pixels / 2;
    const float wantedMa = (evenLevel * evenCount + oddLevel * oddCount) * kChannelFullMa;
    float scale = 1.0f;
    if (wantedMa > static_cast<float>(_capMa)) {
        scale = static_cast<float>(_capMa) / wantedMa;
    }
    for (uint8_t i = 0; i < 4; ++i) {
        _sent[i] = i < bytesPerPixel ? static_cast<uint8_t>(even[i] * scale) : 0;  // the even pixels' colour
    }
    _scalePct = static_cast<uint8_t>(scale * 100.0f);
    _estimatedMa = static_cast<uint16_t>(wantedMa * scale);

    // On the wire: green, red, blue (and white last).
    for (uint16_t pixel = 0; pixel < _pixels; ++pixel) {
        const uint8_t* colour = (pixel % 2 == 0) ? even : odd;
        const uint8_t wire[4] = {static_cast<uint8_t>(colour[1] * scale), static_cast<uint8_t>(colour[0] * scale),
                                 static_cast<uint8_t>(colour[2] * scale), static_cast<uint8_t>(colour[3] * scale)};
        memcpy(_frame + static_cast<size_t>(pixel) * bytesPerPixel, wire, bytesPerPixel);
    }
    // Blocks until the frame is out (about 1 ms for 30 pixels): only a test command calls this.
    return rmt_write_sample(kChannel, _frame, static_cast<size_t>(_pixels) * bytesPerPixel, true) == ESP_OK;
}
