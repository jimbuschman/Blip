/***************************************************
 * Hardware Test - Component by Component
 *
 * Tests each component as it's connected:
 * - OLED: shows test pattern on boot
 * - Touch: shows press status on OLED
 * - Speaker: tap TOP to play tone
 * - LEDs: tap BACK to cycle colors
 * - Light: shows reading on OLED
 ****************************************************/

#include <Wire.h>
#include <U8g2lib.h>
#include <driver/i2s.h>
#include <driver/adc.h>
#include <Adafruit_NeoPixel.h>

#define TOUCH_TOP_PIN   4
#define TOUCH_BACK_PIN  17
#define I2S_DOUT        25
#define I2S_BCLK        26
#define I2S_LRC         27
#define LED_PIN         16
#define NUM_LEDS        3

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);
Adafruit_NeoPixel leds(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

bool lastTop = false;
bool lastBack = false;
int colorIndex = 0;
bool i2sReady = false;

const char* colorNames[] = {"OFF", "RED", "GREEN", "BLUE", "WHITE"};
uint32_t colors[] = {
    0,
    Adafruit_NeoPixel::Color(255, 0, 0),
    Adafruit_NeoPixel::Color(0, 255, 0),
    Adafruit_NeoPixel::Color(0, 0, 255),
    Adafruit_NeoPixel::Color(255, 255, 255)
};
const int numColors = 5;

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
    if (i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL) == ESP_OK) {
        i2s_set_pin(I2S_NUM_0, &pin_config);
        i2sReady = true;
    }
}

void playTone(int frequency, int duration_ms) {
    if (!i2sReady) return;
    const int sample_rate = 16000;
    int total_samples = sample_rate * duration_ms / 1000;
    int16_t samples[128];
    size_t bytes_written;
    int written = 0;
    while (written < total_samples) {
        int chunk = 64;
        if (written + chunk > total_samples) chunk = total_samples - written;
        for (int i = 0; i < chunk; i++) {
            int16_t val = (int16_t)(sin(2.0 * 3.14159 * frequency * (written + i) / (float)sample_rate) * 10000);
            samples[i * 2] = val;
            samples[i * 2 + 1] = val;
        }
        i2s_write(I2S_NUM_0, samples, chunk * 4, &bytes_written, portMAX_DELAY);
        written += chunk;
    }
    memset(samples, 0, sizeof(samples));
    i2s_write(I2S_NUM_0, samples, sizeof(samples), &bytes_written, portMAX_DELAY);
    i2s_zero_dma_buffer(I2S_NUM_0);
}

void setAllLeds(uint32_t color) {
    for (int i = 0; i < NUM_LEDS; i++) leds.setPixelColor(i, color);
    leds.show();
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== Hardware Test ===");

    pinMode(TOUCH_TOP_PIN, INPUT);
    pinMode(TOUCH_BACK_PIN, INPUT);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);

    u8g2.begin();
    setupI2S();

    leds.begin();
    leds.setBrightness(50);
    leds.show();

    // Startup screen test
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(20, 15, "HARDWARE TEST");
    u8g2.drawStr(5, 35, "TOP = tone");
    u8g2.drawStr(5, 50, "BACK = LEDs");
    u8g2.sendBuffer();

    Serial.println("Ready. TOP=tone, BACK=LEDs");
}

void loop() {
    bool top = digitalRead(TOUCH_TOP_PIN) == HIGH;
    bool back = digitalRead(TOUCH_BACK_PIN) == HIGH;
    int light = adc1_get_raw(ADC1_CHANNEL_6);

    if (top && !lastTop) {
        playTone(440, 500);
    }

    if (back && !lastBack) {
        colorIndex = (colorIndex + 1) % numColors;
        setAllLeds(colors[colorIndex]);
    }

    lastTop = top;
    lastBack = back;

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);

    char buf[32];
    sprintf(buf, "Top:  %s", top ? "PRESSED" : "---");
    u8g2.drawStr(5, 12, buf);
    sprintf(buf, "Back: %s", back ? "PRESSED" : "---");
    u8g2.drawStr(5, 24, buf);
    sprintf(buf, "Light: %d", light);
    u8g2.drawStr(5, 36, buf);
    sprintf(buf, "LEDs: %s", colorNames[colorIndex]);
    u8g2.drawStr(5, 48, buf);
    sprintf(buf, "Speaker: %s", i2sReady ? "OK" : "N/A");
    u8g2.drawStr(5, 60, buf);

    u8g2.sendBuffer();
    delay(50);
}
