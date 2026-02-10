/***************************************************
 * OLED + Touch + Speaker + LED Test
 *
 * ESP32 + OLED + two TTP223 + MAX98357 amp + WS2812B
 * No analogRead to avoid I2S/ADC conflict.
 * Tap TOP  = play tone
 * Tap BACK = cycle LED colors
 ****************************************************/

#include <Wire.h>
#include <U8g2lib.h>
#include <driver/i2s.h>
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

// Color cycle: red, green, blue, white, off
uint32_t colors[] = {
    Adafruit_NeoPixel::Color(255, 0, 0),
    Adafruit_NeoPixel::Color(0, 255, 0),
    Adafruit_NeoPixel::Color(0, 0, 255),
    Adafruit_NeoPixel::Color(255, 255, 255),
    Adafruit_NeoPixel::Color(0, 0, 0)
};
const char* colorNames[] = {"RED", "GREEN", "BLUE", "WHITE", "OFF"};
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

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
    Serial.println("I2S initialized.");
}

void playTone(int frequency, int duration_ms) {
    Serial.print("Playing ");
    Serial.print(frequency);
    Serial.println("Hz...");

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

    // Silence
    memset(samples, 0, 128);
    i2s_write(I2S_NUM_0, samples, 128, &bytes_written, portMAX_DELAY);
    Serial.println("Done.");
}

void setAllLeds(uint32_t color) {
    for (int i = 0; i < NUM_LEDS; i++) {
        leds.setPixelColor(i, color);
    }
    leds.show();
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== Speaker + LED Test ===");

    pinMode(TOUCH_TOP_PIN, INPUT);
    pinMode(TOUCH_BACK_PIN, INPUT);

    u8g2.begin();
    setupI2S();

    // Init LEDs
    leds.begin();
    leds.setBrightness(50);
    leds.show();
    Serial.println("LEDs initialized on GPIO 16.");

    // Startup: play tone and flash LEDs red
    Serial.println("Playing startup tone...");
    setAllLeds(colors[0]);  // Red
    playTone(440, 1000);
    setAllLeds(colors[4]);  // Off
    colorIndex = 4;

    Serial.println("TOP = play tone, BACK = cycle LED colors");
}

void loop() {
    bool top = digitalRead(TOUCH_TOP_PIN) == HIGH;
    bool back = digitalRead(TOUCH_BACK_PIN) == HIGH;

    if (top && !lastTop) {
        playTone(440, 500);
    }

    if (back && !lastBack) {
        colorIndex = (colorIndex + 1) % numColors;
        setAllLeds(colors[colorIndex]);
        Serial.print("LEDs: ");
        Serial.println(colorNames[colorIndex]);
    }

    lastTop = top;
    lastBack = back;

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);

    char buf[32];
    sprintf(buf, "Top:  %s", top ? "PRESSED" : "---");
    u8g2.drawStr(5, 12, buf);
    sprintf(buf, "Back: %s", back ? "PRESSED" : "---");
    u8g2.drawStr(5, 26, buf);
    sprintf(buf, "LEDs: %s", colorNames[colorIndex]);
    u8g2.drawStr(5, 40, buf);
    u8g2.drawStr(5, 56, "TOP=tone BACK=LEDs");
    u8g2.sendBuffer();

    delay(50);
}
