/***************************************************
 * Blip Hardware Test (Serial Only - No OLED)
 *
 * Tests via Serial Monitor + LEDs only.
 * OLED is skipped to isolate I2C wiring issue.
 *
 * Tap TOP touch sensor to advance to next test.
 * Watch Serial Monitor at 115200 baud.
 ****************************************************/

#include <Adafruit_NeoPixel.h>

#define LED_PIN         16
#define NUM_LEDS        3
#define TOUCH_TOP_PIN   4
#define TOUCH_BACK_PIN  17
#define LIGHT_SENSOR_PIN 34
#define SPEAKER_PIN     25

Adafruit_NeoPixel leds(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

int testStage = 0;
bool lastTopState = false;
unsigned long lastDebounce = 0;

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== Blip Hardware Test (No OLED) ===");
    Serial.println("All output via Serial Monitor.");
    Serial.println("Tap TOP sensor to advance tests.\n");

    pinMode(TOUCH_TOP_PIN, INPUT);
    pinMode(TOUCH_BACK_PIN, INPUT);
    pinMode(LIGHT_SENSOR_PIN, INPUT);

    leds.begin();
    leds.setBrightness(80);
    leds.clear();
    leds.show();

    Serial.println(">> TEST 1: LEDs");
    Serial.println("   LEDs turning RED...");
    setAllLEDs(255, 0, 0);
    delay(1000);
    Serial.println("   GREEN...");
    setAllLEDs(0, 255, 0);
    delay(1000);
    Serial.println("   BLUE...");
    setAllLEDs(0, 0, 255);
    delay(1000);
    Serial.println("   WHITE...");
    setAllLEDs(255, 255, 255);
    delay(1000);
    leds.clear();
    leds.show();
    Serial.println("   Did you see R, G, B, White?");
    Serial.println("   Tap TOP for next test.\n");
}

void setAllLEDs(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < NUM_LEDS; i++)
        leds.setPixelColor(i, leds.Color(r, g, b));
    leds.show();
}

void loop() {
    bool topPressed = digitalRead(TOUCH_TOP_PIN) == HIGH;

    if (topPressed && !lastTopState && millis() - lastDebounce > 300) {
        lastDebounce = millis();
        testStage++;
        leds.clear();
        leds.show();
    }
    lastTopState = topPressed;

    switch (testStage) {
        case 0: /* waiting */ break;
        case 1: testTouch(); break;
        case 2: testLight(); break;
        case 3: testSpeaker(); break;
        default: showDone(); break;
    }
}

// === TEST 2: TOUCH SENSORS ===
void testTouch() {
    static bool headerShown = false;
    if (!headerShown) {
        headerShown = true;
        Serial.println(">> TEST 2: TOUCH SENSORS");
        Serial.println("   Touch each sensor. LED feedback:");
        Serial.println("   Top = green LED, Back = blue LED");
        Serial.println("   Hold BACK then tap TOP for next.\n");
    }

    bool top = digitalRead(TOUCH_TOP_PIN) == HIGH;
    bool back = digitalRead(TOUCH_BACK_PIN) == HIGH;

    if (top) leds.setPixelColor(0, leds.Color(0, 255, 0));
    else leds.setPixelColor(0, leds.Color(0, 0, 0));

    if (back) leds.setPixelColor(2, leds.Color(0, 0, 255));
    else leds.setPixelColor(2, leds.Color(0, 0, 0));

    leds.show();

    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 500) {
        lastPrint = millis();
        Serial.print("   Top: ");
        Serial.print(top ? "PRESSED" : "---");
        Serial.print("  Back: ");
        Serial.println(back ? "PRESSED" : "---");
    }
}

// === TEST 3: LIGHT SENSOR ===
void testLight() {
    static bool headerShown = false;
    if (!headerShown) {
        headerShown = true;
        Serial.println("\n>> TEST 3: LIGHT SENSOR");
        Serial.println("   Cover/uncover the LDR.");
        Serial.println("   LEDs brightness follows light level.");
        Serial.println("   Tap TOP for next.\n");
    }

    int light = analogRead(LIGHT_SENSOR_PIN);
    int brightness = map(light, 0, 4095, 0, 255);
    setAllLEDs(brightness, brightness, brightness);

    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 500) {
        lastPrint = millis();
        Serial.print("   Light reading: ");
        Serial.println(light);
    }
}

// === TEST 4: SPEAKER ===
void testSpeaker() {
    static bool tonePlayed = false;
    if (!tonePlayed) {
        tonePlayed = true;
        Serial.println("\n>> TEST 4: SPEAKER");
        Serial.println("   Playing 440Hz tone for 1 second...");

        unsigned long start = millis();
        while (millis() - start < 1000) {
            float t = (millis() - start) / 1000.0;
            uint8_t val = (uint8_t)(128 + 127 * sin(2.0 * 3.14159 * 440.0 * t));
            dacWrite(SPEAKER_PIN, val);
            delayMicroseconds(50);
        }
        dacWrite(SPEAKER_PIN, 128);

        Serial.println("   Did you hear a beep?");
        Serial.println("   Tap TOP to finish.\n");
    }
}

// === DONE ===
void showDone() {
    static bool shown = false;
    if (!shown) {
        shown = true;
        setAllLEDs(0, 255, 0);
        Serial.println("=============================");
        Serial.println("  ALL TESTS COMPLETE");
        Serial.println("=============================");
        Serial.println("\nOLED was skipped - check I2C wiring:");
        Serial.println("  SDA = GPIO 21");
        Serial.println("  SCL = GPIO 22");
        Serial.println("  Make sure SDA/SCL aren't swapped");
        Serial.println("  Check OLED VCC is on 3.3V, not 5V");
    }
}
