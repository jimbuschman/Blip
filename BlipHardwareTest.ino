/***************************************************
 * Blip Hardware Test
 *
 * Tests each component one at a time:
 *   1. OLED display
 *   2. LEDs (cycle R, G, B)
 *   3. Touch sensors (live readout)
 *   4. Light sensor (live readout)
 *   5. Speaker (simple tone)
 *
 * Tap TOP touch sensor to advance to next test.
 * Watch Serial Monitor (115200) for extra info.
 ****************************************************/

#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_NeoPixel.h>
#include <driver/i2s.h>

// === PINS (must match your wiring) ===
#define LED_PIN         16
#define NUM_LEDS        3
#define TOUCH_TOP_PIN   4
#define TOUCH_BACK_PIN  17
#define LIGHT_SENSOR_PIN 34
#define I2S_DOUT        25
#define I2S_BCLK        26
#define I2S_LRC         27

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);
Adafruit_NeoPixel leds(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

int testStage = 0;
bool lastTopState = false;
unsigned long lastDebounce = 0;

void setup() {
    Serial.begin(115200);
    Serial.println("=== Blip Hardware Test ===");

    pinMode(TOUCH_TOP_PIN, INPUT);
    pinMode(TOUCH_BACK_PIN, INPUT);
    pinMode(LIGHT_SENSOR_PIN, INPUT);

    u8g2.begin();
    leds.begin();
    leds.setBrightness(80);
    leds.clear();
    leds.show();

    showWelcome();
}

void loop() {
    bool topPressed = digitalRead(TOUCH_TOP_PIN) == HIGH;

    // Debounced tap to advance tests
    if (topPressed && !lastTopState && millis() - lastDebounce > 300) {
        lastDebounce = millis();
        testStage++;
        Serial.print("Advancing to test stage: ");
        Serial.println(testStage);

        // Clean up between tests
        leds.clear();
        leds.show();
    }
    lastTopState = topPressed;

    switch (testStage) {
        case 0: /* welcome screen, waiting for tap */ break;
        case 1: testOLED(); break;
        case 2: testLEDs(); break;
        case 3: testTouch(); break;
        case 4: testLight(); break;
        case 5: testSpeaker(); break;
        default: showDone(); break;
    }
}

// === WELCOME ===
void showWelcome() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(10, 25, "Blip H/W Test");
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(10, 50, "Tap TOP to start");
    u8g2.sendBuffer();
    Serial.println("Display showing welcome. If you can read it, OLED works!");
}

// === TEST 1: OLED ===
void testOLED() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(5, 15, "1. OLED Test");
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(5, 35, "Can you read this?");
    u8g2.drawStr(5, 50, "If yes, OLED works!");
    u8g2.drawStr(5, 63, "Tap TOP for next...");
    u8g2.sendBuffer();
}

// === TEST 2: LEDs ===
void testLEDs() {
    static unsigned long lastChange = 0;
    static int color = 0;

    if (millis() - lastChange > 700) {
        lastChange = millis();

        switch (color % 4) {
            case 0:
                for (int i = 0; i < NUM_LEDS; i++)
                    leds.setPixelColor(i, leds.Color(255, 0, 0));
                break;
            case 1:
                for (int i = 0; i < NUM_LEDS; i++)
                    leds.setPixelColor(i, leds.Color(0, 255, 0));
                break;
            case 2:
                for (int i = 0; i < NUM_LEDS; i++)
                    leds.setPixelColor(i, leds.Color(0, 0, 255));
                break;
            case 3:
                for (int i = 0; i < NUM_LEDS; i++)
                    leds.setPixelColor(i, leds.Color(255, 255, 255));
                break;
        }
        leds.show();
        color++;
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(5, 15, "2. LED Test");
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(5, 35, "LEDs cycling:");
    u8g2.drawStr(5, 48, "R -> G -> B -> White");
    u8g2.drawStr(5, 63, "Tap TOP for next...");
    u8g2.sendBuffer();
}

// === TEST 3: TOUCH ===
void testTouch() {
    bool top = digitalRead(TOUCH_TOP_PIN) == HIGH;
    bool back = digitalRead(TOUCH_BACK_PIN) == HIGH;

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(5, 15, "3. Touch Test");
    u8g2.setFont(u8g2_font_ncenB08_tr);

    char buf[32];
    sprintf(buf, "Top:  %s", top ? "PRESSED" : "---");
    u8g2.drawStr(5, 35, buf);
    sprintf(buf, "Back: %s", back ? "PRESSED" : "---");
    u8g2.drawStr(5, 48, buf);

    u8g2.drawStr(5, 63, "Hold BACK, then tap TOP");
    u8g2.sendBuffer();

    // Light up LEDs as feedback
    if (top) leds.setPixelColor(0, leds.Color(0, 255, 0));
    else leds.setPixelColor(0, leds.Color(0, 0, 0));
    if (back) leds.setPixelColor(2, leds.Color(0, 0, 255));
    else leds.setPixelColor(2, leds.Color(0, 0, 0));
    leds.show();
}

// === TEST 4: LIGHT SENSOR ===
void testLight() {
    int light = analogRead(LIGHT_SENSOR_PIN);

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(5, 15, "4. Light Sensor");
    u8g2.setFont(u8g2_font_ncenB08_tr);

    char buf[32];
    sprintf(buf, "Reading: %d", light);
    u8g2.drawStr(5, 35, buf);
    u8g2.drawStr(5, 48, "Cover/uncover sensor");
    u8g2.drawStr(5, 63, "Tap TOP for next...");
    u8g2.sendBuffer();

    // Map light to LED brightness
    int brightness = map(light, 0, 4095, 0, 255);
    for (int i = 0; i < NUM_LEDS; i++)
        leds.setPixelColor(i, leds.Color(brightness, brightness, brightness));
    leds.show();

    Serial.print("Light: ");
    Serial.println(light);
}

// === TEST 5: SPEAKER ===
void testSpeaker() {
    static bool tonePlayed = false;

    if (!tonePlayed) {
        tonePlayed = true;
        Serial.println("Playing test tone...");

        // Init I2S
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

        // Generate a simple beep (440Hz for 1 second)
        const int duration_ms = 1000;
        const int freq = 440;
        const int sample_rate = 16000;
        const int total_samples = sample_rate * duration_ms / 1000;

        int16_t samples[128];
        size_t bytes_written;
        int samplesWritten = 0;

        while (samplesWritten < total_samples) {
            int chunk = min(64, total_samples - samplesWritten);
            for (int i = 0; i < chunk; i++) {
                int16_t val = (int16_t)(sin(2.0 * 3.14159 * freq * (samplesWritten + i) / sample_rate) * 16000);
                samples[i * 2] = val;      // Left
                samples[i * 2 + 1] = val;  // Right
            }
            i2s_write(I2S_NUM_0, samples, chunk * 4, &bytes_written, portMAX_DELAY);
            samplesWritten += chunk;
        }

        // Silence after tone
        memset(samples, 0, sizeof(samples));
        i2s_write(I2S_NUM_0, samples, sizeof(samples), &bytes_written, portMAX_DELAY);

        i2s_driver_uninstall(I2S_NUM_0);
        Serial.println("Tone done.");
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(5, 15, "5. Speaker Test");
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(5, 35, "Did you hear a beep?");
    u8g2.drawStr(5, 48, "If yes, amp works!");
    u8g2.drawStr(5, 63, "Tap TOP to finish...");
    u8g2.sendBuffer();
}

// === ALL DONE ===
void showDone() {
    leds.clear();
    for (int i = 0; i < NUM_LEDS; i++)
        leds.setPixelColor(i, leds.Color(0, 255, 0));
    leds.show();

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(15, 30, "All Done!");
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(10, 50, "All tests complete.");
    u8g2.sendBuffer();
}
