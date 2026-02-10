/***************************************************
 * Sine Tone Test - ESP32 Arduino 3.x I2S API
 ****************************************************/

#include <ESP_I2S.h>

#define I2S_DOUT  25
#define I2S_BCLK  26
#define I2S_LRC   27

I2SClass i2s;

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== Tone Test (new I2S API) ===");

    i2s.setPins(I2S_BCLK, I2S_LRC, I2S_DOUT);
    if (!i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
        Serial.println("I2S init FAILED!");
        return;
    }
    Serial.println("I2S initialized.");

    Serial.println("Playing 440Hz tone for 2 seconds...");
    const int sample_rate = 16000;
    int total_samples = sample_rate * 2;
    int16_t samples[128];
    int written = 0;

    while (written < total_samples) {
        int chunk = 64;
        if (written + chunk > total_samples) chunk = total_samples - written;
        for (int i = 0; i < chunk; i++) {
            int16_t val = (int16_t)(sin(2.0 * 3.14159 * 440 * (written + i) / (float)sample_rate) * 10000);
            samples[i * 2] = val;
            samples[i * 2 + 1] = val;
        }
        i2s.write((uint8_t*)samples, chunk * 4);
        written += chunk;
    }

    memset(samples, 0, sizeof(samples));
    i2s.write((uint8_t*)samples, sizeof(samples));
    Serial.println("Done.");
}

void loop() {
    delay(10000);
}
