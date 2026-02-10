/***************************************************
 * ESP32 Droid Firmware v1
 * 
 * Hardware:
 *   - SSD1306 128x64 OLED (I2C: GPIO 21/22)
 *   - WS2812B LEDs x3 (GPIO 16)
 *   - MAX98357 I2S Amp (GPIO 25/26/27)
 *   - TTP223 Touch x2 (GPIO 4 top, GPIO 17 back)
 *   - Light sensor/LDR (GPIO 34)
 * 
 * Interactions:
 *   - Touch top: happy reaction
 *   - Touch top (spam): annoyed
 *   - Touch top (hold 1s): pet mode
 *   - Touch top (hold 2s): sleep
 *   - Touch top (triple-tap): Simon Says game
 *   - Touch back: surprised
 *   - Double-tap back: dance
 *   - Dark: sleepy → sleep
 *   - Light: wake up
 ****************************************************/

#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <FS.h>
#include <LittleFS.h>
#include <driver/adc.h>
#include "Face.h"
#include "Audio.h"

// === PIN DEFINITIONS ===
#define LED_PIN         16
#define NUM_LEDS        3
#define LED_LEFT        0
#define LED_MID         1
#define LED_RIGHT       2

#define I2S_DOUT        25
#define I2S_BCLK        26
#define I2S_LRC         27

#define TOUCH_TOP_PIN   4
#define TOUCH_BACK_PIN  17
#define LIGHT_SENSOR_PIN 34

// === THRESHOLDS & TIMING ===
#define DARK_LEVEL          800
#define BRIGHT_LEVEL        2000
#define TOUCH_DEBOUNCE_MS   150
#define LONG_PRESS_MS       2000
#define PET_HOLD_MS         1000
#define DOUBLE_TAP_MS       400
#define TRIPLE_TAP_MS       500
#define SPAM_WINDOW_MS      2000
#define SPAM_TOUCH_COUNT    4
#define LIGHT_CHECK_MS      1000
#define IDLE_CHIRP_CHANCE   5
#define IDLE_CHIRP_INTERVAL 5000

// === ENUMS ===
enum Mood { HAPPY, ANNOYED, SLEEPY };
enum State { IDLE, REACTING, SLEEPING, WAKING, PETTING, DANCING, GAME_MENU, PLAYING_SIMON, PLAYING_PONG };

// === GLOBALS ===
Face *face;
Adafruit_NeoPixel leds(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
Audio audio;

Mood currentMood = HAPPY;
State currentState = IDLE;

// Touch tracking
unsigned long topPressStart = 0;
unsigned long topReleaseTime = 0;
unsigned long backReleaseTime = 0;
int topTapCount = 0;
int backTapCount = 0;
int recentTopTouches = 0;
unsigned long topTouchWindow = 0;
bool topWasPressed = false;
bool backWasPressed = false;

// Timing
unsigned long lastLightCheck = 0;
unsigned long lastIdleChirp = 0;

// LED state
uint8_t ledR = 0, ledG = 150, ledB = 150;
bool ledPulsing = false;
float pulsePhase = 0;

// Game state - Simon
int gamePattern[20];
int gameLength = 0;
int gameInput = 0;
int gameScore = 0;
int gameSpeed = 500;

// Game state - Pong
float ballX, ballY;
float ballVelX, ballVelY;
int playerY, aiY;
int playerScore, aiScore;
const int PADDLE_HEIGHT = 16;
const int PADDLE_WIDTH = 3;
const int BALL_SIZE = 3;
const int WIN_SCORE = 5;
unsigned long lastPongUpdate = 0;
const int PONG_SPEED_MS = 30;

// Game menu
int menuSelection = 0;

// Sound tracking
int lastSoundIndex[8] = {-1, -1, -1, -1, -1, -1, -1, -1};

// === SOUND CATEGORIES ===
const char* happySounds[] = {
    "/happy/happy_01.wav", "/happy/happy_02.wav", "/happy/happy_03.wav",
    "/happy/happy_04.wav", "/happy/happy_05.wav", "/happy/happy_06.wav"
};
const int numHappySounds = 6;

const char* annoyedSounds[] = {
    "/annoyed/annoyed_01.wav", "/annoyed/annoyed_02.wav", "/annoyed/annoyed_03.wav",
    "/annoyed/annoyed_04.wav", "/annoyed/annoyed_05.wav", "/annoyed/annoyed_06.wav",
    "/annoyed/annoyed_07.wav", "/annoyed/annoyed_08.wav", "/annoyed/annoyed_09.wav",
    "/annoyed/annoyed_10.wav"
};
const int numAnnoyedSounds = 10;

const char* surprisedSounds[] = {
    "/surprised/surprised_01.wav", "/surprised/surprised_02.wav",
    "/surprised/surprised_03.wav", "/surprised/surprised_04.wav",
    "/surprised/surprised_05.wav", "/surprised/surprised_06.wav",
    "/surprised/surprised_07.wav", "/surprised/surprised_08.wav"
};
const int numSurprisedSounds = 8;

const char* sleepySounds[] = {
    "/sleepy/sleepy_01.wav", "/sleepy/sleepy_02.wav", "/sleepy/sleepy_03.wav",
    "/sleepy/sleepy_04.wav", "/sleepy/sleepy_05.wav"
};
const int numSleepySounds = 5;

const char* wakeSounds[] = {
    "/wake/wake_01.wav", "/wake/wake_02.wav", "/wake/wake_03.wav",
    "/wake/wake_04.wav", "/wake/wake_05.wav", "/wake/wake_06.wav",
    "/wake/wake_07.wav", "/wake/wake_08.wav"
};
const int numWakeSounds = 8;

const char* idleSounds[] = {
    "/idle/idle_01.wav", "/idle/idle_02.wav", "/idle/idle_03.wav",
    "/idle/idle_04.wav", "/idle/idle_05.wav", "/idle/idle_06.wav",
    "/idle/idle_07.wav", "/idle/idle_08.wav", "/idle/idle_09.wav",
    "/idle/idle_10.wav"
};
const int numIdleSounds = 10;

const char* excitedSounds[] = {
    "/excited/excited_01.wav", "/excited/excited_02.wav", "/excited/excited_03.wav",
    "/excited/excited_04.wav", "/excited/excited_05.wav", "/excited/excited_06.wav",
    "/excited/excited_07.wav", "/excited/excited_08.wav", "/excited/excited_09.wav",
    "/excited/excited_10.wav"
};
const int numExcitedSounds = 10;

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

void playHappySound() { playSound(happySounds[pickRandomSound(0, numHappySounds)]); }
void playAnnoyedSound() { playSound(annoyedSounds[pickRandomSound(1, numAnnoyedSounds)]); }
void playSurprisedSound() { playSound(surprisedSounds[pickRandomSound(2, numSurprisedSounds)]); }
void playSleepySound() { playSound(sleepySounds[pickRandomSound(3, numSleepySounds)]); }
void playWakeSound() { playSound(wakeSounds[pickRandomSound(4, numWakeSounds)]); }
void playIdleSound() { playSound(idleSounds[pickRandomSound(5, numIdleSounds)]); }
void playExcitedSound() { playSound(excitedSounds[pickRandomSound(6, numExcitedSounds)]); }

void playTone(int freq, int duration) {
    // Simple beep via audio library or just LED flash if no tone file
    // For now, just visual feedback
}

// === LED FUNCTIONS ===
void setLEDColor(uint8_t r, uint8_t g, uint8_t b, bool pulse = false) {
    ledR = r; ledG = g; ledB = b;
    ledPulsing = pulse;
}

void setAllLEDs(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < NUM_LEDS; i++) {
        leds.setPixelColor(i, leds.Color(r, g, b));
    }
    leds.show();
}

void setSingleLED(int index, uint8_t r, uint8_t g, uint8_t b) {
    leds.setPixelColor(index, leds.Color(r, g, b));
    leds.show();
}

void updateLEDs() {
    if (currentState == PLAYING_SIMON || currentState == PLAYING_PONG) return;  // Game controls LEDs directly
    
    uint8_t r = ledR, g = ledG, b = ledB;
    
    if (ledPulsing) {
        pulsePhase += 0.05;
        float brightness = (sin(pulsePhase) + 1.0) / 2.0;
        brightness = 0.3 + brightness * 0.7;
        r = ledR * brightness;
        g = ledG * brightness;
        b = ledB * brightness;
    }
    
    setAllLEDs(r, g, b);
}

void flashLEDs(uint8_t r, uint8_t g, uint8_t b, int duration = 150) {
    setAllLEDs(r, g, b);
    delay(duration);
}

// === MOOD & STATE FUNCTIONS ===
void setMoodHappy() {
    currentMood = HAPPY;
    face->Expression.GoTo_Happy();
    setLEDColor(0, 150, 150, true);
}

void setMoodAnnoyed() {
    currentMood = ANNOYED;
    face->Expression.GoTo_Angry();
    setLEDColor(255, 100, 0, false);
}

void setMoodSleepy() {
    currentMood = SLEEPY;
    face->Expression.GoTo_Sleepy();
    setLEDColor(0, 0, 80, false);
}

// === SLEEP / WAKE ===
void goToSleep() {
    currentState = SLEEPING;
    playSleepySound();
    
    face->Expression.GoTo_Sleepy();
    face->Update();
    delay(1000);
    
    setAllLEDs(0, 0, 0);
    
    // Draw closed eyes
    u8g2.clearBuffer();
    u8g2.drawBox(20, 30, 36, 4);
    u8g2.drawBox(72, 30, 36, 4);
    u8g2.sendBuffer();
}

void wakeUp() {
    currentState = WAKING;
    flashLEDs(255, 255, 255, 100);
    playWakeSound();
    face->Expression.GoTo_Surprised();
    delay(500);
    setMoodHappy();
    currentState = IDLE;
}

// === PET MODE ===
void startPetting() {
    currentState = PETTING;
    face->Expression.GoTo_Happy();
    setLEDColor(255, 150, 50, true);  // Warm orange pulse
    playHappySound();
}

void updatePetting() {
    // Extra happy face, warm LEDs
    // Continuous gentle sounds could go here
}

void stopPetting() {
    setMoodHappy();
    currentState = IDLE;
}

// === DANCE ===
void doDance() {
    currentState = DANCING;
    playExcitedSound();
    
    // Wiggle animation
    for (int i = 0; i < 4; i++) {
        face->Expression.GoTo_Happy();
        face->Look.LookAt(-0.8, 0);
        face->Update();
        setAllLEDs(255, 0, 150);  // Pink
        delay(150);
        
        face->Look.LookAt(0.8, 0);
        face->Update();
        setAllLEDs(0, 255, 150);  // Cyan
        delay(150);
    }
    
    // Big finish
    face->Expression.GoTo_Surprised();
    face->Look.LookAt(0, 0);
    face->Update();
    flashLEDs(255, 255, 255, 200);
    
    setMoodHappy();
    currentState = IDLE;
}

// === SIMON SAYS GAME ===
void startGame() {
    currentState = PLAYING_SIMON;
    gameLength = 1;
    gameScore = 0;
    gameSpeed = 500;
    
    // Game face
    face->Expression.GoTo_Normal();
    face->RandomBlink = false;
    face->RandomLook = false;
    
    // Show "Let's play!"
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(20, 35, "Let's Play!");
    u8g2.sendBuffer();
    playExcitedSound();
    delay(1500);
    
    // Generate first pattern
    gamePattern[0] = random(2);  // 0 = top/left, 1 = back/right
    
    showGamePattern();
}

void showGamePattern() {
    // Show score
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    char buf[16];
    sprintf(buf, "Score: %d", gameScore);
    u8g2.drawStr(35, 15, buf);
    u8g2.drawStr(30, 35, "Watch me!");
    u8g2.sendBuffer();
    
    delay(800);
    
    setAllLEDs(0, 0, 0);
    delay(300);
    
    // Play pattern
    for (int i = 0; i < gameLength; i++) {
        if (gamePattern[i] == 0) {
            // Top = Left LED = Green
            setSingleLED(LED_LEFT, 0, 255, 0);
            face->Look.LookAt(-0.5, 0);
        } else {
            // Back = Right LED = Blue
            setSingleLED(LED_RIGHT, 0, 0, 255);
            face->Look.LookAt(0.5, 0);
        }
        face->Update();
        delay(gameSpeed);
        
        setAllLEDs(0, 0, 0);
        face->Look.LookAt(0, 0);
        face->Update();
        delay(200);
    }
    
    // Your turn
    u8g2.clearBuffer();
    u8g2.drawStr(30, 35, "Your turn!");
    u8g2.sendBuffer();
    
    gameInput = 0;
}

void handleGameInput(int input) {
    // Flash LED for feedback
    if (input == 0) {
        setSingleLED(LED_LEFT, 0, 255, 0);
    } else {
        setSingleLED(LED_RIGHT, 0, 0, 255);
    }
    delay(150);
    setAllLEDs(0, 0, 0);
    
    // Check if correct
    if (input == gamePattern[gameInput]) {
        gameInput++;
        
        // Completed pattern?
        if (gameInput >= gameLength) {
            // Success!
            gameScore++;
            playHappySound();
            
            face->Expression.GoTo_Happy();
            face->Update();
            flashLEDs(0, 255, 0, 300);
            
            // Next round
            if (gameLength < 20) {
                gameLength++;
                gamePattern[gameLength - 1] = random(2);
                
                // Speed up every 3 rounds
                if (gameScore % 3 == 0 && gameSpeed > 200) {
                    gameSpeed -= 50;
                }
            }
            
            delay(500);
            showGamePattern();
        }
    } else {
        // Wrong! Game over
        gameOver();
    }
}

void gameOver() {
    playAnnoyedSound();
    face->Expression.GoTo_Sad();
    face->Update();
    
    // Sad flash
    for (int i = 0; i < 3; i++) {
        setAllLEDs(255, 0, 0);
        delay(100);
        setAllLEDs(0, 0, 0);
        delay(100);
    }
    
    // Show final score
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(20, 25, "Game Over!");
    u8g2.setFont(u8g2_font_ncenB08_tr);
    char buf[20];
    sprintf(buf, "Score: %d", gameScore);
    u8g2.drawStr(35, 45, buf);
    u8g2.sendBuffer();
    
    delay(3000);
    
    // Return to normal
    face->RandomBlink = true;
    face->RandomLook = true;
    setMoodHappy();
    currentState = IDLE;
}

// === GAME MENU ===
void showGameMenu() {
    currentState = GAME_MENU;
    menuSelection = 0;
    
    face->RandomBlink = false;
    face->RandomLook = false;
    
    playExcitedSound();
    drawGameMenu();
}

void drawGameMenu() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(25, 15, "Let's Play!");
    
    u8g2.setFont(u8g2_font_ncenB08_tr);
    
    // Simon option
    if (menuSelection == 0) {
        u8g2.drawBox(15, 25, 98, 14);
        u8g2.setDrawColor(0);
        u8g2.drawStr(20, 36, "> Simon Says");
        u8g2.setDrawColor(1);
    } else {
        u8g2.drawStr(20, 36, "  Simon Says");
    }
    
    // Pong option
    if (menuSelection == 1) {
        u8g2.drawBox(15, 43, 98, 14);
        u8g2.setDrawColor(0);
        u8g2.drawStr(20, 54, "> Pong");
        u8g2.setDrawColor(1);
    } else {
        u8g2.drawStr(20, 54, "  Pong");
    }
    
    u8g2.sendBuffer();
}

void handleMenuInput(int input) {
    if (input == 0) {
        // Top touch - change selection
        menuSelection = (menuSelection + 1) % 2;
        drawGameMenu();
        delay(200);
    } else {
        // Back touch - select game
        if (menuSelection == 0) {
            startSimon();
        } else {
            startPong();
        }
    }
}

// === SIMON SAYS (renamed from startGame) ===
void startSimon() {
    currentState = PLAYING_SIMON;
    gameLength = 1;
    gameScore = 0;
    gameSpeed = 500;
    
    face->Expression.GoTo_Normal();
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(15, 35, "Simon Says!");
    u8g2.sendBuffer();
    delay(1000);
    
    gamePattern[0] = random(2);
    showGamePattern();
}

// === PONG ===
void startPong() {
    currentState = PLAYING_PONG;
    playerScore = 0;
    aiScore = 0;
    playerY = 32 - PADDLE_HEIGHT / 2;
    aiY = 32 - PADDLE_HEIGHT / 2;
    
    face->Expression.GoTo_Focused();
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(35, 35, "Pong!");
    u8g2.sendBuffer();
    delay(1000);
    
    resetBall(true);
    lastPongUpdate = millis();
}

void resetBall(bool towardsPlayer) {
    ballX = 64;
    ballY = 32;
    ballVelX = towardsPlayer ? -2.5 : 2.5;
    ballVelY = random(-15, 16) / 10.0;
}

void updatePong(bool topHeld, bool backHeld) {
    unsigned long now = millis();
    if (now - lastPongUpdate < PONG_SPEED_MS) return;
    lastPongUpdate = now;
    
    // Player paddle movement
    if (topHeld && playerY > 0) {
        playerY -= 3;
    }
    if (backHeld && playerY < 64 - PADDLE_HEIGHT) {
        playerY += 3;
    }
    
    // Simple AI - follow ball with some delay
    int aiTarget = ballY - PADDLE_HEIGHT / 2;
    if (aiY < aiTarget - 2) {
        aiY += 2;
    } else if (aiY > aiTarget + 2) {
        aiY -= 2;
    }
    aiY = constrain(aiY, 0, 64 - PADDLE_HEIGHT);
    
    // Ball movement
    ballX += ballVelX;
    ballY += ballVelY;
    
    // Top/bottom bounce
    if (ballY <= 0 || ballY >= 64 - BALL_SIZE) {
        ballVelY = -ballVelY;
        ballY = constrain(ballY, 0, 64 - BALL_SIZE);
    }
    
    // Player paddle collision (left side)
    if (ballX <= PADDLE_WIDTH + 4 && ballX > 0) {
        if (ballY + BALL_SIZE >= playerY && ballY <= playerY + PADDLE_HEIGHT) {
            ballVelX = abs(ballVelX) * 1.05;  // Speed up slightly
            ballVelX = min(ballVelX, 5.0f);
            // Add spin based on where it hit the paddle
            float hitPos = (ballY - playerY) / (float)PADDLE_HEIGHT;
            ballVelY = (hitPos - 0.5) * 4;
            ballX = PADDLE_WIDTH + 5;
        }
    }
    
    // AI paddle collision (right side)
    if (ballX >= 128 - PADDLE_WIDTH - 4 - BALL_SIZE && ballX < 128) {
        if (ballY + BALL_SIZE >= aiY && ballY <= aiY + PADDLE_HEIGHT) {
            ballVelX = -abs(ballVelX) * 1.05;
            ballVelX = max(ballVelX, -5.0f);
            float hitPos = (ballY - aiY) / (float)PADDLE_HEIGHT;
            ballVelY = (hitPos - 0.5) * 4;
            ballX = 128 - PADDLE_WIDTH - 5 - BALL_SIZE;
        }
    }
    
    // Scoring
    if (ballX <= 0) {
        // AI scores
        aiScore++;
        pongAIScored();
        if (aiScore >= WIN_SCORE) {
            pongGameOver(false);
            return;
        }
        resetBall(true);
    }
    else if (ballX >= 128) {
        // Player scores
        playerScore++;
        pongPlayerScored();
        if (playerScore >= WIN_SCORE) {
            pongGameOver(true);
            return;
        }
        resetBall(false);
    }
    
    // LED intensity based on ball speed
    int intensity = map(abs(ballVelX) * 10, 25, 50, 80, 255);
    setAllLEDs(0, intensity, intensity);
    
    drawPong();
}

void drawPong() {
    u8g2.clearBuffer();
    
    // Scores
    u8g2.setFont(u8g2_font_ncenB08_tr);
    char buf[8];
    sprintf(buf, "%d", playerScore);
    u8g2.drawStr(25, 10, buf);
    sprintf(buf, "%d", aiScore);
    u8g2.drawStr(98, 10, buf);
    
    // Center line (dashed)
    for (int y = 0; y < 64; y += 8) {
        u8g2.drawBox(63, y, 2, 4);
    }
    
    // Paddles
    u8g2.drawBox(2, playerY, PADDLE_WIDTH, PADDLE_HEIGHT);
    u8g2.drawBox(128 - PADDLE_WIDTH - 2, aiY, PADDLE_WIDTH, PADDLE_HEIGHT);
    
    // Ball
    u8g2.drawBox((int)ballX, (int)ballY, BALL_SIZE, BALL_SIZE);
    
    u8g2.sendBuffer();
}

void pongPlayerScored() {
    flashLEDs(0, 255, 0, 200);
    face->Expression.GoTo_Happy();
    face->Update();
    playHappySound();
    delay(500);
    face->Expression.GoTo_Focused();
}

void pongAIScored() {
    flashLEDs(255, 0, 0, 200);
    face->Expression.GoTo_Angry();
    face->Update();
    playAnnoyedSound();
    delay(500);
    face->Expression.GoTo_Focused();
}

void pongGameOver(bool playerWon) {
    face->RandomBlink = true;
    face->RandomLook = true;
    
    if (playerWon) {
        // Victory dance!
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB10_tr);
        u8g2.drawStr(25, 35, "You Win!");
        u8g2.sendBuffer();
        
        playExcitedSound();
        face->Expression.GoTo_Happy();
        
        for (int i = 0; i < 5; i++) {
            setAllLEDs(0, 255, 0);
            delay(100);
            setAllLEDs(255, 255, 0);
            delay(100);
        }
    } else {
        // Sore loser!
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB10_tr);
        u8g2.drawStr(30, 30, "I Win!");
        u8g2.setFont(u8g2_font_ncenB08_tr);
        u8g2.drawStr(25, 48, "Hehe! >:)");
        u8g2.sendBuffer();
        
        playExcitedSound();  // Smug celebration
        face->Expression.GoTo_Happy();
        
        for (int i = 0; i < 3; i++) {
            setAllLEDs(255, 0, 255);
            delay(150);
            setAllLEDs(0, 0, 0);
            delay(150);
        }
        
        delay(1000);
        
        // Player is sad
        face->Expression.GoTo_Sad();
        face->Update();
        delay(500);
    }
    
    delay(2000);
    setMoodHappy();
    currentState = IDLE;
}

// === TOUCH HANDLERS ===
void processTopTouch(bool pressed) {
    unsigned long now = millis();
    
    if (pressed && !topWasPressed) {
        // Just pressed
        topPressStart = now;
        topWasPressed = true;
    }
    else if (!pressed && topWasPressed) {
        // Just released
        unsigned long holdTime = now - topPressStart;
        topWasPressed = false;
        
        // Was it a hold?
        if (holdTime >= LONG_PRESS_MS) {
            // Long press - go to sleep
            goToSleep();
            return;
        }
        else if (holdTime >= PET_HOLD_MS) {
            // Medium hold was petting, now stopped
            if (currentState == PETTING) {
                stopPetting();
            }
            return;
        }
        
        // It was a tap - count it
        if (now - topReleaseTime < TRIPLE_TAP_MS) {
            topTapCount++;
        } else {
            topTapCount = 1;
        }
        topReleaseTime = now;
        
        // Check for triple tap
        if (topTapCount >= 3) {
            topTapCount = 0;
            showGameMenu();
            return;
        }
        
        // Regular tap - track for spam
        if (now - topTouchWindow < SPAM_WINDOW_MS) {
            recentTopTouches++;
        } else {
            recentTopTouches = 1;
            topTouchWindow = now;
        }
        
        // React based on spam
        currentState = REACTING;
        if (recentTopTouches >= SPAM_TOUCH_COUNT) {
            setMoodAnnoyed();
            playAnnoyedSound();
            recentTopTouches = 0;
        } else {
            setMoodHappy();
            playHappySound();
        }
        delay(50);
        currentState = IDLE;
    }
    else if (pressed && topWasPressed) {
        // Being held
        unsigned long holdTime = now - topPressStart;
        
        if (holdTime >= PET_HOLD_MS && holdTime < LONG_PRESS_MS && currentState != PETTING) {
            startPetting();
        }
        
        if (currentState == PETTING) {
            updatePetting();
        }
    }
}

void processBackTouch(bool pressed) {
    unsigned long now = millis();
    
    if (pressed && !backWasPressed) {
        backWasPressed = true;
    }
    else if (!pressed && backWasPressed) {
        backWasPressed = false;
        
        // Count taps
        if (now - backReleaseTime < DOUBLE_TAP_MS) {
            backTapCount++;
        } else {
            backTapCount = 1;
        }
        backReleaseTime = now;
        
        // Check for double tap
        if (backTapCount >= 2) {
            backTapCount = 0;
            doDance();
            return;
        }
        
        // Single tap - surprised
        currentState = REACTING;
        face->Expression.GoTo_Surprised();
        flashLEDs(255, 255, 255, 100);
        playSurprisedSound();
        delay(400);
        
        if (currentMood == HAPPY) setMoodHappy();
        else if (currentMood == ANNOYED) setMoodAnnoyed();
        currentState = IDLE;
    }
}

// === SENSOR READING ===
bool readTopTouch() { return digitalRead(TOUCH_TOP_PIN) == HIGH; }
bool readBackTouch() { return digitalRead(TOUCH_BACK_PIN) == HIGH; }
int readLightLevel() { return adc1_get_raw(ADC1_CHANNEL_6); } // GPIO 34 = ADC1 CH6

// === IDLE BEHAVIORS ===
void doIdleBehaviors() {
    unsigned long now = millis();
    
    if (now - lastIdleChirp > IDLE_CHIRP_INTERVAL) {
        lastIdleChirp = now;
        if (random(100) < IDLE_CHIRP_CHANCE) {
            playIdleSound();
        }
    }
    
    if (currentMood == ANNOYED && now - topReleaseTime > 5000) {
        setMoodHappy();
    }
}

// === SETUP ===
void setup() {
    Serial.begin(115200);
    Serial.println("Droid starting...");
    
    pinMode(TOUCH_TOP_PIN, INPUT);
    pinMode(TOUCH_BACK_PIN, INPUT);
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS failed!");
    }
    
    leds.begin();
    leds.setBrightness(80);
    setLEDColor(0, 150, 150, true);
    
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(15);
    
    face = new Face(128, 64, 40);
    face->Expression.GoTo_Normal();
    face->RandomBlink = true;
    face->RandomLook = true;
    face->Blink.Timer.SetIntervalMillis(3000);
    face->Look.Timer.SetIntervalMillis(2500);
    
    // Startup
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
    bool topPressed = readTopTouch();
    bool backPressed = readBackTouch();
    
    audio.loop();
    
    // Game menu input
    if (currentState == GAME_MENU) {
        if (topPressed && !topWasPressed) {
            topWasPressed = true;
            handleMenuInput(0);  // Top = change selection
        } else if (!topPressed) {
            topWasPressed = false;
        }
        
        if (backPressed && !backWasPressed) {
            backWasPressed = true;
            handleMenuInput(1);  // Back = select
        } else if (!backPressed) {
            backWasPressed = false;
        }
        return;
    }
    
    // Simon game input
    if (currentState == PLAYING_SIMON) {
        if (topPressed && !topWasPressed) {
            topWasPressed = true;
            handleGameInput(0);
        } else if (!topPressed) {
            topWasPressed = false;
        }
        
        if (backPressed && !backWasPressed) {
            backWasPressed = true;
            handleGameInput(1);
        } else if (!backPressed) {
            backWasPressed = false;
        }
        return;
    }
    
    // Pong game - continuous input
    if (currentState == PLAYING_PONG) {
        updatePong(topPressed, backPressed);
        return;
    }
    
    // Light check
    if (now - lastLightCheck > LIGHT_CHECK_MS) {
        lastLightCheck = now;
        int light = readLightLevel();
        
        if (light < DARK_LEVEL && currentState != SLEEPING && currentState != PETTING) {
            goToSleep();
        } else if (light > BRIGHT_LEVEL && currentState == SLEEPING) {
            wakeUp();
        }
    }
    
    // Touch processing
    if (currentState == SLEEPING) {
        if (topPressed || backPressed) {
            wakeUp();
        }
    } else {
        processTopTouch(topPressed);
        processBackTouch(backPressed);
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

void audio_info(const char *info) {
    Serial.print("Audio: ");
    Serial.println(info);
}
