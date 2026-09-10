/*
 * ESP32-C3 SuperMini Multi-Game Handheld Console
 * 
 * Hardware:
 *   - ESP32-C3 SuperMini
 *   - 0.96" or 1.3" 128x64 I2C OLED (SSD1306)
 *   - 8 Tactile Buttons (INPUT_PULLUP)
 *   - Keep-Alive Pin 5 for smart power banks
 */

#include "Config.h"
#include "Game.h"
#include "Menu.h"
#include "PauseMenu.h"
#include "Tetris.h"
#include "Snake.h"
#include "PacMan.h"
#include "Pong.h"
#include "FlappyBird.h"
#include "Minesweeper.h"
#include "Breakout.h"
#include "Asteroids.h"
#include "Game2048.h"
#include "SettingsGame.h"

// Hardware & Preference globals
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Preferences prefs;
SettingsData settings;
const uint32_t SLEEP_TIMES_MS[4] = { 10000, 15000, 30000, 45000 };
unsigned long lastInputTime = 0;

// Keep-alive state
unsigned long lastKick = 0;
unsigned long kickStart = 0;
bool kicking = false;

// Game Instances
Game* currentGame = nullptr;
Menu menu;
Tetris tetris;
Snake snake;
PacMan pacman;
Pong pong;
FlappyBird flappy;
Minesweeper minesGame;
Breakout breakout;
Asteroids asteroids;
Game2048 game2048;
Settings settingsGame;

// Menu launcher implementation
void Menu::launchSelected() {
  if (selected == 0) currentGame = &tetris;
  else if (selected == 1) currentGame = &snake;
  else if (selected == 2) currentGame = &pacman;
  else if (selected == 3) currentGame = &pong;
  else if (selected == 4) currentGame = &flappy;
  else if (selected == 5) currentGame = &minesGame;
  else if (selected == 6) currentGame = &breakout;
  else if (selected == 7) currentGame = &asteroids;
  else if (selected == 8) currentGame = &game2048;
  else if (selected == 9) currentGame = &settingsGame;

  currentGame->init();
}

/* ================= BOOT ANIMATION ================= */
void bootAnimation() {
  // GameCube-inspired spinning wireframe cube
  const int edges[12][2] = {
    {0,1},{1,2},{2,3},{3,0},   // front face
    {4,5},{5,6},{6,7},{7,4},   // back face
    {0,4},{1,5},{2,6},{3,7}    // connecting edges
  };

  float angleX = 0, angleY = 0;

  for (int frame = 0; frame < 60; frame++) {
    display.clearDisplay();

    // Cube grows from small to full size
    float t = frame / 60.0f;
    float sz = 6.0f + t * 16.0f;

    // Rotation slows down in the last 15 frames
    float rotSpeed = (frame < 45) ? 1.0f : (60 - frame) / 15.0f;
    angleX += 0.08f * rotSpeed;
    angleY += 0.13f * rotSpeed;

    float ca = cos(angleX), sa = sin(angleX);
    float cb = cos(angleY), sb = sin(angleY);

    int sx[8], sy[8];
    for (int i = 0; i < 8; i++) {
      float x = (i & 1) ? 1.0f : -1.0f;
      float y = (i & 2) ? 1.0f : -1.0f;
      float z = (i & 4) ? 1.0f : -1.0f;

      // Rotate around Y axis
      float x1 = x * cb - z * sb;
      float z1 = x * sb + z * cb;
      // Rotate around X axis
      float y1 = y * ca - z1 * sa;
      float z2 = y * sa + z1 * ca;

      // Perspective projection
      float d = 3.5f + z2;
      if (d < 0.5f) d = 0.5f;
      sx[i] = 64 + (int)(x1 * sz * 2.0f / d);
      sy[i] = 32 + (int)(y1 * sz * 2.0f / d);
    }

    // Draw all 12 edges
    for (int e = 0; e < 12; e++) {
      display.drawLine(
        sx[edges[e][0]], sy[edges[e][0]],
        sx[edges[e][1]], sy[edges[e][1]],
        SSD1306_WHITE
      );
    }

    display.display();
    delay(25);
  }

  delay(300);  // hold final frame
}


void goToSleep() {
  // Turn off display
  display.clearDisplay();
  display.display();
  display.ssd1306_command(SSD1306_DISPLAYOFF);

  // Enable GPIO wakeup (ESP32-C3)
  esp_sleep_enable_gpio_wakeup();

  // Buttons are INPUT_PULLUP -> wake on LOW
  gpio_wakeup_enable((gpio_num_t)BTN_UP, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable((gpio_num_t)BTN_DOWN, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable((gpio_num_t)BTN_LEFT, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable((gpio_num_t)BTN_RIGHT, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable((gpio_num_t)BTN_A, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable((gpio_num_t)BTN_B, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable((gpio_num_t)BTN_START, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable((gpio_num_t)BTN_SELECT, GPIO_INTR_LOW_LEVEL);

  delay(100);
  esp_light_sleep_start();

  // ---- WAKEUP ----
  display.ssd1306_command(SSD1306_DISPLAYON);
  applyBrightness();

  lastInputTime = millis();
  currentGame = &menu;
  currentGame->init();
}


void setup() {
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);
  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  loadSettings();
  applyBrightness();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);

  randomSeed(esp_random());

  // Boot animation
  bootAnimation();

  currentGame = &menu;
  currentGame->init();
  lastInputTime = millis();
  pinMode(KEEP_ALIVE_PIN, INPUT); // float
}


void loop() {
  uint32_t timeout = SLEEP_TIMES_MS[settings.sleepIndex];

  if (millis() - lastInputTime > timeout) {
    goToSleep();
  }

  currentGame->update();
  currentGame->draw();
  unsigned long now = millis();

  // Start pull-down every 20 seconds
  if (!kicking && now - lastKick >= 20000) {
    pinMode(KEEP_ALIVE_PIN, OUTPUT);
    digitalWrite(KEEP_ALIVE_PIN, LOW);
    kickStart = now;
    kicking = true;
  }

  // Release after 150 ms
  if (kicking && now - kickStart >= 150) {
    pinMode(KEEP_ALIVE_PIN, INPUT); // float again
    kicking = false;
    lastKick = now;
  }
}
