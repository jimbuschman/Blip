/***************************************************
 * I2C Scanner - find devices on the bus
 ****************************************************/

#include <Wire.h>

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== I2C Scanner ===");

    Wire.begin(4, 17);  // Using GPIO 4 (SDA) and GPIO 17 (SCL) to test
    Serial.println("Scanning...");

    int found = 0;
    for (byte addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("Found device at 0x%02X\n", addr);
            found++;
        }
    }
    Serial.printf("Scan done. %d device(s) found.\n", found);
}

void loop() {
    delay(10000);
}
