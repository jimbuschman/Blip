/***************************************************
 * Minimal Sine Tone Test
 * Exact same code that worked before, nothing else
 ****************************************************/

#include <driver/i2s.h>

#define I2S_DOUT        25
#define I2S_BCLK        26
#define I2S_LRC         27

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== Minimal Tone Test ===");

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = 0,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK,
        .ws_io_num = I2S_LRC,
        .data_out_num = I2S_DOUT,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);

    Serial.println("Playing 440Hz tone for 2 seconds...");

    const int sample_rate = 16000;
    int total_samples = sample_rate * 2;
    int16_t samples[128];
    size_t bytes_written;
    int written = 0;

    while (written < total_samples) {
        int chunk = 64;
        if (written + chunk > total_samples) chunk = total_samples - written;
        for (int i = 0; i < chunk; i++) {
            int16_t val = (int16_t)(sin(2.0 * 3.14159 * 440 * (written + i) / (float)sample_rate) * 16000);
            samples[i * 2] = val;
            samples[i * 2 + 1] = val;
        }
        i2s_write(I2S_NUM_0, samples, chunk * 4, &bytes_written, portMAX_DELAY);
        written += chunk;
    }

    memset(samples, 0, sizeof(samples));
    i2s_write(I2S_NUM_0, samples, sizeof(samples), &bytes_written, portMAX_DELAY);
    i2s_zero_dma_buffer(I2S_NUM_0);
    Serial.println("Done. You should have heard a clean tone.");
}

void loop() {
    delay(10000);
}
