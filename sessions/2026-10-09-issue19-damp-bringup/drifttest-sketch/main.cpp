// Throwaway drift A/B for issue #19 item 5a (not part of resonantNode).
// D-AMP pinout, full duplex (TX silent). Serial commands at 921600 baud:
//   CFG rate=<Hz> align=<0|1> dmacount=<n> dmalen=<n>   reinstall the port
//   CAP n=<samples> label=<text>                          capture mic slot 0, dump RAW rows
// Rows use the RAW_BEGIN / "ms,pcm" / RAW_SUMMARY shape that
// tools/logging/raw_capture_slope.py parses. pcm = 32-bit word >> 8.
#include <Arduino.h>
#include <driver/i2s.h>
#include <soc/i2s_struct.h>

static uint32_t gRate = 16000;
static int gAlign = 1;
static int gDmaCount = 3;
static int gDmaLen = 128;
static int32_t* gBuf = nullptr;
static constexpr size_t kMaxSamples = 16384;
static uint32_t gCapId = 0;

static void installPort() {
    i2s_driver_uninstall(I2S_NUM_0);
    i2s_config_t c = {};
    c.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX);
    c.sample_rate = gRate;
    c.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    c.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    c.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    c.intr_alloc_flags = ESP_INTR_FLAG_LEVEL2;
    c.dma_buf_count = gDmaCount;
    c.dma_buf_len = gDmaLen;
    c.tx_desc_auto_clear = true;
    i2s_pin_config_t p = {};
    p.mck_io_num = I2S_PIN_NO_CHANGE;
    p.bck_io_num = 26;
    p.ws_io_num = 25;
    p.data_out_num = 32;
    p.data_in_num = 33;
    const esp_err_t e1 = i2s_driver_install(I2S_NUM_0, &c, 0, nullptr);
    const esp_err_t e2 = i2s_set_pin(I2S_NUM_0, &p);
    const esp_err_t e3 = i2s_set_clk(I2S_NUM_0, gRate, I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_STEREO);
    if (gAlign) {
        i2s_stop(I2S_NUM_0);
        I2S0.conf.rx_msb_shift = 0;
        i2s_start(I2S_NUM_0);
    }
    i2s_zero_dma_buffer(I2S_NUM_0);
    Serial.printf("CFG_OK rate=%u align=%d dmacount=%d dmalen=%d err=%d/%d/%d rx_msb_shift=%d\n",
                  gRate, gAlign, gDmaCount, gDmaLen, e1, e2, e3, (int)I2S0.conf.rx_msb_shift);
}

static long argValue(const String& line, const char* key, long fallback) {
    const int at = line.indexOf(key);
    if (at < 0) return fallback;
    return line.substring(at + strlen(key)).toInt();
}

static String argText(const String& line, const char* key) {
    const int at = line.indexOf(key);
    if (at < 0) return String("none");
    String rest = line.substring(at + strlen(key));
    const int sp = rest.indexOf(' ');
    return sp < 0 ? rest : rest.substring(0, sp);
}

static void capture(size_t n, const String& label) {
    if (n > kMaxSamples) n = kMaxSamples;
    int32_t frame[2 * 64];
    size_t got = 0;
    // Flush what is queued so the capture is contiguous from now.
    size_t r = 0;
    for (int i = 0; i < gDmaCount + 1; ++i) i2s_read(I2S_NUM_0, frame, sizeof(frame), &r, portMAX_DELAY);
    while (got < n) {
        i2s_read(I2S_NUM_0, frame, sizeof(frame), &r, portMAX_DELAY);
        for (size_t i = 0; i + 1 < r / 4 && got < n; i += 2) gBuf[got++] = frame[i] >> 8;
    }
    ++gCapId;
    Serial.printf("RAW_BEGIN id=%u sr=%u rate=%u align=%d dmacount=%d dmalen=%d label=%s\n",
                  gCapId, gRate, gRate, gAlign, gDmaCount, gDmaLen, label.c_str());
    Serial.println("ms,pcm");
    long odd = 0;
    for (size_t i = 0; i < got; ++i) {
        Serial.printf("%lu,%ld\n", (unsigned long)((uint64_t)i * 1000ULL / gRate), (long)gBuf[i]);
        odd += gBuf[i] & 1;
    }
    Serial.printf("RAW_SUMMARY id=%u captured=%u odd_lsb=%ld\n", gCapId, (unsigned)got, odd);
}

void setup() {
    Serial.begin(921600);
    delay(200);
    gBuf = (int32_t*)malloc(kMaxSamples * sizeof(int32_t));
    Serial.printf("DRIFTTEST ready buf=%s\n", gBuf ? "ok" : "FAIL");
    installPort();
}

void loop() {
    // Keep the RX queue drained between captures, like a running firmware.
    int32_t frame[2 * 64];
    size_t r = 0;
    i2s_read(I2S_NUM_0, frame, sizeof(frame), &r, 0);
    static String line;
    while (Serial.available()) {
        const char c = (char)Serial.read();
        if (c == '\n') {
            line.trim();
            if (line.startsWith("CFG")) {
                gRate = argValue(line, "rate=", gRate);
                gAlign = argValue(line, "align=", gAlign);
                gDmaCount = argValue(line, "dmacount=", gDmaCount);
                gDmaLen = argValue(line, "dmalen=", gDmaLen);
                installPort();
            } else if (line.startsWith("CAP")) {
                capture((size_t)argValue(line, "n=", 16384), argText(line, "label="));
            }
            line = "";
        } else if (c != '\r') {
            line += c;
        }
    }
}
