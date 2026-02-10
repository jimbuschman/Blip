/***************************************************
 * OLED + Touch Sensor Test
 *
 * ESP32 + OLED + two TTP223 touch sensors.
 * Top = GPIO 4, Back = GPIO 17
 ****************************************************/

#include <Wire.h>
#include <U8g2lib.h>

#define TOUCH_TOP_PIN   4
#define TOUCH_BACK_PIN  17

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== OLED + Touch Test ===");

    pinMode(TOUCH_TOP_PIN, INPUT);
    pinMode(TOUCH_BACK_PIN, INPUT);

    u8g2.begin();
    Serial.println("Ready. Touch the sensors.");
}

void loop() {
    bool top = digitalRead(TOUCH_TOP_PIN) == HIGH;
    bool back = digitalRead(TOUCH_BACK_PIN) == HIGH;

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(5, 15, "Touch Test");
    u8g2.setFont(u8g2_font_ncenB08_tr);

    char buf[32];
    sprintf(buf, "Top:  %s", top ? "PRESSED" : "---");
    u8g2.drawStr(5, 35, buf);
    sprintf(buf, "Back: %s", back ? "PRESSED" : "---");
    u8g2.drawStr(5, 50, buf);
    u8g2.sendBuffer();

    Serial.print("Top: ");
    Serial.print(top ? "PRESSED" : "---");
    Serial.print("  Back: ");
    Serial.println(back ? "PRESSED" : "---");

    delay(200);
}
