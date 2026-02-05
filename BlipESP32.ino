/***************************************************
 * ESP32 Droid Firmware v1
 * 
 * Hardware:
 *   - SSD1306 128x64 OLED (I2C: GPIO 21/22)
 *   - WS2812B LEDs x3 (GPIO 13)
 *   - MAX98357 I2S Amp (GPIO 25/26/27)
 *   - TTP223 Touch x2 (GPIO 4 top, GPIO 15 back)
 *   - Light sensor/LDR (GPIO 34)
 * 
 * Uses esp32-eyes library for face animations
 ****************************************************/

#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <FS.h>
#include <LittleFS.h>
#include "Face.h"
#include "Audio.h"  // ESP32-audioI2S library

// === PIN DEFINITIONS ===
#define LED_PIN         13
#define NUM_LEDS        3

#define I2S_DOUT        25
#define I2S_BCLK        26
#define I2S_LRC         27

#define TOUCH_TOP_PIN   4
#define TOUCH_BACK_PIN  15

#define LIGHT_SENSOR_PIN 34

// === THRESHOLDS & TIMING ===
#define DARK_LEVEL          800     // ADC value (lower = darker)
#define BRIGHT_LEVEL        2000    // ADC value (higher = brighter)
#define TOUCH_DEBOUNCE_MS   200
#define SPAM_WINDOW_MS      2000
#define SPAM_TOUCH_COUNT    3
#define LIGHT_CHECK_MS      1000
#define IDLE_CHIRP_CHANCE   5       // % chance per 5 seconds
#define IDLE_CHIRP_INTERVAL 5000

// === ENUMS ===
enum Mood { HAPPY, ANNOYED, SLEEPY };
enum State { IDLE, REACTING, SLEEPING, WAKING };

// === GLOBALS ===
Face *face;
Adafruit_NeoPixel leds(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
Audio audio;

Mood currentMood = HAPPY;
State currentState = IDLE;

// Touch tracking
unsigned long lastTopTouch = 0;
unsigned long lastBackTouch = 0;
int recentTopTouches = 0;
unsigned long topTouchWindow = 0;

// Timing
unsigned long lastLightCheck = 0;
unsigned long lastIdleChirp = 0;
unsigned long lastBlinkTime = 0;
unsigned long sleepStartTime = 0;

// LED state
uint8_t ledR = 0, ledG = 150, ledB = 150;  // Cyan default
bool ledPulsing = false;
float pulsePhase = 0;

// Sound tracking (avoid repeats)
int lastSoundIndex[8] = {-1, -1, -1, -1, -1, -1, -1, -1};

// === SOUND CATEGORIES ===
const char* happySounds[] = {
    "/happy/happy_01.wav",
    "/happy/happy_02.wav",
    "/happy/happy_03.wav",
    "/happy/happy_04.wav"
};
const int numHappySounds = 4;

const char* annoyedSounds[] = {
    "/annoyed/annoyed_01.wav",
    "/annoyed/annoyed_02.wav",
    "/annoyed/annoyed_03.wav"
};
const int numAnnoyedSounds = 3;

const char* surprisedSounds[] = {
    "/surprised/surprised_01.wav",
    "/surprised/surprised_02.wav",
    "/surprised/surprised_03.wav"
};
const int numSurprisedSounds = 3;

const char* sleepySounds[] = {
    "/sleepy/sleepy_01.wav",
    "/sleepy/sleepy_02.wav"
};
const int numSleepySounds = 2;

const char* wakeSounds[] = {
    "/wake/wake_01.wav",
    "/wake/wake_02.wav",
    "/wake/wake_03.wav"
};
const int numWakeSounds = 3;

const char* idleSounds[] = {
    "/idle/idle_01.wav",
    "/idle/idle_02.wav",
    "/idle/idle_03.wav",
    "/idle/idle_04.wav"
};
const int numIdleSounds = 4;

// === SOUND FUNCTIONS ===
int pickRandomSound(int category, int numSounds) {
    int pick;
    int attempts = 0;
    do {
        pick = random(numSounds);
        attempts++;
    } while (pick == lastSoundIndex[category] && attempts < 5);
    lastSoundIndex[category] = pick;
    return pick;
}

void playSound(const char* path) {
    if (LittleFS.exists(path)) {
        audio.connecttoFS(LittleFS, path);
    } else {
        Serial.print("Sound not found: ");
        Serial.println(path);
    }
}

void playHappySound() {
    int i = pickRandomSound(0, numHappySounds);
    playSound(happySounds[i]);
}

void playAnnoyedSound() {
    int i = pickRandomSound(1, numAnnoyedSounds);
    playSound(annoyedSounds[i]);
}

void playSurprisedSound() {
    int i = pickRandomSound(2, numSurprisedSounds);
    playSound(surprisedSounds[i]);
}

void playSleepySound() {
    int i = pickRandomSound(3, numSleepySounds);
    playSound(sleepySounds[i]);
}

void playWakeSound() {
    int i = pickRandomSound(4, numWakeSounds);
    playSound(wakeSounds[i]);
}

void playIdleSound() {
    int i = pickRandomSound(5, numIdleSounds);
    playSound(idleSounds[i]);
}

// === LED FUNCTIONS ===
void setLEDColor(uint8_t r, uint8_t g, uint8_t b, bool pulse = false) {
    ledR = r;
    ledG = g;
    ledB = b;
    ledPulsing = pulse;
}

void updateLEDs() {
    uint8_t r = ledR, g = ledG, b = ledB;
    
    if (ledPulsing) {
        pulsePhase += 0.05;
        float brightness = (sin(pulsePhase) + 1.0) / 2.0;
        brightness = 0.3 + brightness * 0.7;  // Range 0.3 to 1.0
        r = ledR * brightness;
        g = ledG * brightness;
        b = ledB * brightness;
    }
    
    for (int i = 0; i < NUM_LEDS; i++) {
        leds.setPixelColor(i, leds.Color(r, g, b));
    }
    leds.show();
}

void flashLEDs(uint8_t r, uint8_t g, uint8_t b, int duration = 150) {
    for (int i = 0; i < NUM_LEDS; i++) {
        leds.setPixelColor(i, leds.Color(r, g, b));
    }
    leds.show();
    delay(duration);
}

// === MOOD & STATE FUNCTIONS ===
void setMoodHappy() {
    currentMood = HAPPY;
    face->Expression.GoTo_Happy();
    setLEDColor(0, 150, 150, true);  // Cyan pulse
}

void setMoodAnnoyed() {
    currentMood = ANNOYED;
    face->Expression.GoTo_Angry();
    setLEDColor(255, 100, 0, false);  // Orange solid
}

void setMoodSleepy() {
    currentMood = SLEEPY;
    face->Expression.GoTo_Sleepy();
    setLEDColor(0, 0, 80, false);  // Dim blue solid
}

void goToSleep() {
    currentState = SLEEPING;
    playSleepySound();
    
    // Animate to sleep
    face->Expression.GoTo_Sleepy();
    delay(1000);
    
    // Turn off LEDs
    setLEDColor(0, 0, 0, false);
    
    // Close eyes (draw flat lines)
    u8g2.clearBuffer();
    u8g2.drawBox(20, 30, 36, 4);   // Left eye closed
    u8g2.drawBox(72, 30, 36, 4);   // Right eye closed
    u8g2.sendBuffer();
    
    sleepStartTime = millis();
}

void wakeUp() {
    currentState = WAKING;
    
    // Flash LEDs
    flashLEDs(255, 255, 255, 100);
    
    // Play wake sound
    playWakeSound();
    
    // Wake up face animation
    face->Expression.GoTo_Surprised();
    delay(500);
    
    setMoodHappy();
    currentState = IDLE;
}

// === TOUCH HANDLERS ===
void handleTopTouch() {
    unsigned long now = millis();
    
    // Debounce
    if (now - lastTopTouch < TOUCH_DEBOUNCE_MS) return;
    lastTopTouch = now;
    
    // Track spam
    if (now - topTouchWindow < SPAM_WINDOW_MS) {
        recentTopTouches++;
    } else {
        recentTopTouches = 1;
        topTouchWindow = now;
    }
    
    currentState = REACTING;
    
    // Check for spam
    if (recentTopTouches >= SPAM_TOUCH_COUNT) {
        setMoodAnnoyed();
        playAnnoyedSound();
        recentTopTouches = 0;
    } else {
        setMoodHappy();
        playHappySound();
    }
    
    // Brief reaction time
    delay(100);
    currentState = IDLE;
}

void handleBackTouch() {
    unsigned long now = millis();
    
    // Debounce
    if (now - lastBackTouch < TOUCH_DEBOUNCE_MS) return;
    lastBackTouch = now;
    
    currentState = REACTING;
    
    // Always surprised
    face->Expression.GoTo_Surprised();
    flashLEDs(255, 255, 255, 100);  // White flash
    playSurprisedSound();
    
    delay(500);
    
    // Return to current mood
    if (currentMood == HAPPY) setMoodHappy();
    else if (currentMood == ANNOYED) setMoodAnnoyed();
    
    currentState = IDLE;
}

// === SENSOR READING ===
bool readTopTouch() {
    return digitalRead(TOUCH_TOP_PIN) == HIGH;
}

bool readBackTouch() {
    return digitalRead(TOUCH_BACK_PIN) == HIGH;
}

int readLightLevel() {
    return analogRead(LIGHT_SENSOR_PIN);
}

// === IDLE BEHAVIORS ===
void doIdleBehaviors() {
    unsigned long now = millis();
    
    // Random blink (handled by face library if RandomBlink is on)
    
    // Random chirp
    if (now - lastIdleChirp > IDLE_CHIRP_INTERVAL) {
        lastIdleChirp = now;
        if (random(100) < IDLE_CHIRP_CHANCE) {
            playIdleSound();
        }
    }
    
    // Calm down from annoyed
    if (currentMood == ANNOYED && now - lastTopTouch > 5000) {
        setMoodHappy();
    }
}

// === SETUP ===
void setup() {
    Serial.begin(115200);
    Serial.println("Droid starting...");
    
    // Initialize pins
    pinMode(TOUCH_TOP_PIN, INPUT);
    pinMode(TOUCH_BACK_PIN, INPUT);
    pinMode(LIGHT_SENSOR_PIN, INPUT);
    
    // Initialize filesystem
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed!");
    } else {
        Serial.println("LittleFS mounted");
    }
    
    // Initialize LEDs
    leds.begin();
    leds.setBrightness(80);
    setLEDColor(0, 150, 150, true);
    
    // Initialize audio
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(15);  // 0-21
    
    // Initialize face
    face = new Face(128, 64, 40);
    face->Expression.GoTo_Normal();
    face->RandomBlink = true;
    face->RandomLook = true;
    face->Blink.Timer.SetIntervalMillis(3000);
    face->Look.Timer.SetIntervalMillis(2500);
    
    // Startup sequence
    Serial.println("Waking up...");
    flashLEDs(0, 150, 150, 200);
    playWakeSound();
    face->Expression.GoTo_Happy();
    
    currentState = IDLE;
    currentMood = HAPPY;
    
    Serial.println("Droid ready!");
}

// === MAIN LOOP ===
void loop() {
    unsigned long now = millis();
    
    // Audio processing (must be called regularly)
    audio.loop();
    
    // Light check
    if (now - lastLightCheck > LIGHT_CHECK_MS) {
        lastLightCheck = now;
        int light = readLightLevel();
        
        if (light < DARK_LEVEL && currentState != SLEEPING) {
            goToSleep();
        } else if (light > BRIGHT_LEVEL && currentState == SLEEPING) {
            wakeUp();
        }
    }
    
    // Touch check (only when not sleeping)
    if (currentState != SLEEPING) {
        if (readTopTouch()) {
            handleTopTouch();
        }
        if (readBackTouch()) {
            handleBackTouch();
        }
    } else {
        // Wake on touch while sleeping
        if (readTopTouch() || readBackTouch()) {
            wakeUp();
        }
    }
    
    // Idle behaviors
    if (currentState == IDLE) {
        doIdleBehaviors();
    }
    
    // Update outputs
    if (currentState != SLEEPING) {
        face->Update();
    }
    updateLEDs();
}

// === AUDIO CALLBACKS (required by ESP32-audioI2S) ===
void audio_info(const char *info) {
    Serial.print("Audio: ");
    Serial.println(info);
}
