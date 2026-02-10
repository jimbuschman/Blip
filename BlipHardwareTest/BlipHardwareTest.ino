/***************************************************
 * OLED + Touch + Light Sensor Test
 *
 * ESP32 + OLED + two TTP223 + LDR on GPIO 34
 ****************************************************/

#include <Wire.h>
#include <U8g2lib.h>

#define TOUCH_TOP_PIN   4
#define TOUCH_BACK_PIN  17
#define LIGHT_SENSOR_PIN 34

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== OLED + Touch + Light Test ===");

    pinMode(TOUCH_TOP_PIN, INPUT);
    pinMode(TOUCH_BACK_PIN, INPUT);
    pinMode(LIGHT_SENSOR_PIN, INPUT);

    u8g2.begin();
    Serial.println("Ready.");
}

void loop() {
    bool top = digitalRead(TOUCH_TOP_PIN) == HIGH;
    bool back = digitalRead(TOUCH_BACK_PIN) == HIGH;
    int light = analogRead(LIGHT_SENSOR_PIN);

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);

    char buf[32];
    sprintf(buf, "Top:  %s", top ? "PRESSED" : "---");
    u8g2.drawStr(5, 15, buf);
    sprintf(buf, "Back: %s", back ? "PRESSED" : "---");
    u8g2.drawStr(5, 30, buf);
    sprintf(buf, "Light: %d", light);
    u8g2.drawStr(5, 50, buf);
    u8g2.sendBuffer();

    Serial.print("Top: ");
    Serial.print(top ? "PRESSED" : "---");
    Serial.print("  Back: ");
    Serial.print(back ? "PRESSED" : "---");
    Serial.print("  Light: ");
    Serial.println(light);

    delay(200);
}
