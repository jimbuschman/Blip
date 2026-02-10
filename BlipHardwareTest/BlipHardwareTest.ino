/***************************************************
 * OLED + Touch + Light + LEDs Test
 *
 * ESP32 + OLED + two TTP223 + LDR + WS2812B x3
 * LEDs on GPIO 16
 ****************************************************/

#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN         16
#define NUM_LEDS        3
#define TOUCH_TOP_PIN   4
#define TOUCH_BACK_PIN  17
#define LIGHT_SENSOR_PIN 34

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);
Adafruit_NeoPixel leds(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

int color = 0;
unsigned long lastColorChange = 0;

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== OLED + Touch + Light + LED Test ===");

    pinMode(TOUCH_TOP_PIN, INPUT);
    pinMode(TOUCH_BACK_PIN, INPUT);
    pinMode(LIGHT_SENSOR_PIN, INPUT);

    u8g2.begin();
    leds.begin();
    leds.setBrightness(80);
    Serial.println("Ready. LEDs cycling colors.");
}

void loop() {
    bool top = digitalRead(TOUCH_TOP_PIN) == HIGH;
    bool back = digitalRead(TOUCH_BACK_PIN) == HIGH;
    int light = analogRead(LIGHT_SENSOR_PIN);

    // Cycle LED colors
    if (millis() - lastColorChange > 700) {
        lastColorChange = millis();
        switch (color % 4) {
            case 0: setAllLEDs(255, 0, 0); break;
            case 1: setAllLEDs(0, 255, 0); break;
            case 2: setAllLEDs(0, 0, 255); break;
            case 3: setAllLEDs(255, 255, 255); break;
        }
        color++;
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);

    char buf[32];
    sprintf(buf, "Top:  %s", top ? "PRESSED" : "---");
    u8g2.drawStr(5, 15, buf);
    sprintf(buf, "Back: %s", back ? "PRESSED" : "---");
    u8g2.drawStr(5, 30, buf);
    sprintf(buf, "Light: %d", light);
    u8g2.drawStr(5, 45, buf);

    const char* colors[] = {"RED", "GREEN", "BLUE", "WHITE"};
    sprintf(buf, "LEDs: %s", colors[(color - 1) % 4]);
    u8g2.drawStr(5, 60, buf);
    u8g2.sendBuffer();

    delay(100);
}

void setAllLEDs(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < NUM_LEDS; i++)
        leds.setPixelColor(i, leds.Color(r, g, b));
    leds.show();
}
