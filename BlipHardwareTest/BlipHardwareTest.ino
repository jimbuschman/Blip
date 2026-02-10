/***************************************************
 * OLED + Touch + Light + Speaker Test
 *
 * ESP32 + OLED + two TTP223 + LDR + MAX98357 amp
 * Tap TOP to play a tone, tap BACK to change pitch
 ****************************************************/

#include <Wire.h>
#include <U8g2lib.h>

#define TOUCH_TOP_PIN   4
#define TOUCH_BACK_PIN  17
#define LIGHT_SENSOR_PIN 34
#define SPEAKER_PIN     25

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

bool lastTop = false;
bool lastBack = false;
int freq = 440;

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== OLED + Touch + Light + Speaker Test ===");

    pinMode(TOUCH_TOP_PIN, INPUT);
    pinMode(TOUCH_BACK_PIN, INPUT);
    pinMode(LIGHT_SENSOR_PIN, INPUT);

    u8g2.begin();
    Serial.println("Ready. Tap TOP to play tone, BACK to change pitch.");
}

void playTone(int frequency, int duration_ms) {
    Serial.print("Playing tone: ");
    Serial.print(frequency);
    Serial.println("Hz");

    unsigned long start = millis();
    while (millis() - start < (unsigned long)duration_ms) {
        float t = (millis() - start) / 1000.0;
        uint8_t val = (uint8_t)(128 + 127 * sin(2.0 * 3.14159 * frequency * t));
        dacWrite(SPEAKER_PIN, val);
        delayMicroseconds(50);
    }
    dacWrite(SPEAKER_PIN, 128);
}

void loop() {
    bool top = digitalRead(TOUCH_TOP_PIN) == HIGH;
    bool back = digitalRead(TOUCH_BACK_PIN) == HIGH;
    int light = analogRead(LIGHT_SENSOR_PIN);

    // Top tap = play tone
    if (top && !lastTop) {
        playTone(freq, 500);
    }

    // Back tap = change frequency
    if (back && !lastBack) {
        freq += 100;
        if (freq > 1000) freq = 200;
        Serial.print("Frequency: ");
        Serial.println(freq);
    }

    lastTop = top;
    lastBack = back;

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);

    char buf[32];
    sprintf(buf, "Top:  %s", top ? "PRESSED" : "---");
    u8g2.drawStr(5, 15, buf);
    sprintf(buf, "Back: %s", back ? "PRESSED" : "---");
    u8g2.drawStr(5, 30, buf);
    sprintf(buf, "Light: %d", light);
    u8g2.drawStr(5, 45, buf);
    sprintf(buf, "Freq: %dHz (tap TOP)", freq);
    u8g2.drawStr(5, 60, buf);
    u8g2.sendBuffer();

    delay(50);
}
