/***************************************************
 * OLED Screen Test
 ****************************************************/

#include <Wire.h>
#include <U8g2lib.h>

#define TOUCH_TOP_PIN   4
#define TOUCH_BACK_PIN  17

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== OLED Test ===");

    pinMode(TOUCH_TOP_PIN, INPUT);
    pinMode(TOUCH_BACK_PIN, INPUT);

    u8g2.begin();
    Serial.println("OLED initialized.");
}

void loop() {
    bool top = digitalRead(TOUCH_TOP_PIN) == HIGH;
    bool back = digitalRead(TOUCH_BACK_PIN) == HIGH;

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);

    char buf[32];
    u8g2.drawStr(20, 15, "SCREEN TEST");
    sprintf(buf, "Top:  %s", top ? "PRESSED" : "---");
    u8g2.drawStr(5, 30, buf);
    sprintf(buf, "Back: %s", back ? "PRESSED" : "---");
    u8g2.drawStr(5, 45, buf);
    u8g2.drawStr(5, 60, "Screen OK!");
    u8g2.sendBuffer();

    delay(50);
}
