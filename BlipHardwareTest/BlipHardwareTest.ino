/***************************************************
 * DAC Test - bypass amp entirely
 * Connect speaker directly to GPIO25 and GND
 ****************************************************/

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== DAC Tone Test (no amp) ===");
    Serial.println("Speaker should be on GPIO25 + GND");
    Serial.println("Playing 440Hz tone for 3 seconds...");

    const int sample_rate = 16000;
    int total_samples = sample_rate * 3;
    unsigned long sample_period_us = 1000000 / sample_rate;

    for (int i = 0; i < total_samples; i++) {
        uint8_t val = (uint8_t)(128 + 127 * sin(2.0 * 3.14159 * 440 * i / (float)sample_rate));
        dacWrite(25, val);
        delayMicroseconds(sample_period_us);
    }

    dacWrite(25, 128); // silence
    Serial.println("Done. Was it a clean tone?");
}

void loop() {
    delay(10000);
}
