/***************************************************
 * OLED Only Test
 *
 * Just the ESP32 + OLED display, nothing else.
 * SDA = GPIO 21, SCL = GPIO 22
 ****************************************************/

#include <Wire.h>
#include <U8g2lib.h>

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== OLED Only Test ===");

    Serial.println("Starting I2C scan...");
    Wire.begin(21, 22);
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.print("  Found device at 0x");
            Serial.println(addr, HEX);
        }
    }
    Serial.println("I2C scan done.");

    Serial.println("Initializing OLED...");
    u8g2.begin();
    Serial.println("OLED initialized.");

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(20, 35, "Hello Blip!");
    u8g2.sendBuffer();
    Serial.println("Text sent to display.");
}

void loop() {
    delay(1000);
    Serial.println("alive");
}
