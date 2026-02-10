/***************************************************
 * WAV Playback Test
 * Plays a sine tone then a WAV file for comparison
 ****************************************************/

#include <FS.h>
#include <LittleFS.h>
#include <driver/i2s.h>

#define I2S_DOUT        25
#define I2S_BCLK        26
#define I2S_LRC         27
#define TOUCH_TOP_PIN   4

bool lastTop = false;

void setupI2S() {
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
}

void playTone(int frequency, int duration_ms) {
    Serial.printf("Playing %dHz tone...\n", frequency);
    const int sample_rate = 16000;
    int total_samples = sample_rate * duration_ms / 1000;
    int16_t samples[128];
    size_t bytes_written;
    int written = 0;
    while (written < total_samples) {
        int chunk = 64;
        if (written + chunk > total_samples) chunk = total_samples - written;
        for (int i = 0; i < chunk; i++) {
            int16_t val = (int16_t)(sin(2.0 * 3.14159 * frequency * (written + i) / (float)sample_rate) * 16000);
            samples[i * 2] = val;
            samples[i * 2 + 1] = val;
        }
        i2s_write(I2S_NUM_0, samples, chunk * 4, &bytes_written, portMAX_DELAY);
        written += chunk;
    }
    memset(samples, 0, sizeof(samples));
    i2s_write(I2S_NUM_0, samples, sizeof(samples), &bytes_written, portMAX_DELAY);
    i2s_zero_dma_buffer(I2S_NUM_0);
    Serial.println("Tone done.");
}

void playWav(const char* path) {
    Serial.printf("Playing WAV: %s\n", path);
    File file = LittleFS.open(path, "r");
    if (!file) { Serial.println("File not found!"); return; }

    // Read WAV header
    uint8_t header[44];
    file.read(header, 44);
    Serial.printf("File size: %d, data starts at byte 44\n", file.size());

    uint8_t rawBuf[256];
    int16_t i2sBuf[512];
    size_t bytes_written;

    while (file.available()) {
        int bytesRead = file.read(rawBuf, 256);
        for (int i = 0; i < bytesRead; i++) {
            int16_t sample = ((int16_t)rawBuf[i] - 128) << 8;
            i2sBuf[i * 2] = sample;
            i2sBuf[i * 2 + 1] = sample;
        }
        i2s_write(I2S_NUM_0, i2sBuf, bytesRead * 4, &bytes_written, portMAX_DELAY);
    }
    file.close();

    memset(i2sBuf, 0, sizeof(i2sBuf));
    for (int i = 0; i < 8; i++) {
        i2s_write(I2S_NUM_0, i2sBuf, sizeof(i2sBuf), &bytes_written, portMAX_DELAY);
    }
    i2s_zero_dma_buffer(I2S_NUM_0);
    Serial.println("WAV done.");
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== WAV Playback Test ===");

    pinMode(TOUCH_TOP_PIN, INPUT);
    setupI2S();

    if (!LittleFS.begin(false)) {
        Serial.println("LittleFS FAILED!");
        return;
    }
    Serial.println("LittleFS OK");

    // First: play a sine tone (we know this works)
    Serial.println("\n--- SINE TONE ---");
    playTone(440, 1000);
    delay(500);

    // Then: play a WAV file
    Serial.println("\n--- WAV FILE ---");
    playWav("/happy/happy_01.wav");

    Serial.println("\nTap TOP to replay both.");
}

void loop() {
    bool top = digitalRead(TOUCH_TOP_PIN) == HIGH;
    if (top && !lastTop) {
        playTone(440, 500);
        delay(300);
        playWav("/happy/happy_01.wav");
    }
    lastTop = top;
    delay(50);
}
