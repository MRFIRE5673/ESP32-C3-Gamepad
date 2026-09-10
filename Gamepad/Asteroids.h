#pragma once
#include "Config.h"
#include "Game.h"
#include "Menu.h"
#include "PauseMenu.h"

/* ================= Asteroids CONST ================= */
#define AST_MAX_BULLETS   5
#define AST_MAX_ASTEROIDS 8
#define AST_SHIP_HALF     4   // half-size of ship bounding box


/* ================= ASTEROIDS CLASS ================= */
class Asteroids : public Game {
public:
  void init() override {
    score = 0;
    highScore = loadHighScore("ast_hi");
    gameOver = false;
    restartLatch = false;
    shipX = 64.0f;
    shipY = 32.0f;
    shipAngle = 0.0f;
    shipVX = 0.0f;
    shipVY = 0.0f;
    thrust = false;
    numBullets = 0;
    fireLatch = false;
    numAsteroids = 0;
    lastUpdate = millis();
    spawnWave(4);
    pause.close();
  }

  void update() override {
    if (gameOver) {
      if (btn(BTN_START) && !restartLatch) { init(); restartLatch = true; }
      if (!btn(BTN_START)) restartLatch = false;
      return;
    }

    static bool selectLatch = false;
    if (btn(BTN_SELECT) && !selectLatch && !pause.isActive()) {
      pause.open(); selectLatch = true; return;
    }
    if (!btn(BTN_SELECT)) selectLatch = false;

    pause.update();
    if (pause.hasSelection()) {
      int sel = pause.getSelection();
      if (sel == 1) { init(); return; }
      else if (sel == 2) { currentGame = &menu; currentGame->init(); return; }
    }
    if (pause.isActive()) return;

    unsigned long now = millis();
    if (now - lastUpdate < 40) return;   // ~25 fps for smooth floats
    lastUpdate = now;

    // --- Input ---
    if (btn(BTN_LEFT))  shipAngle -= 0.20f;
    if (btn(BTN_RIGHT)) shipAngle += 0.20f;

    thrust = btn(BTN_UP);
    if (thrust) {
      shipVX += cos(shipAngle) * 0.4f;
      shipVY += sin(shipAngle) * 0.4f;
    }

    // Clamp speed
    float spd = sqrt(shipVX*shipVX + shipVY*shipVY);
    if (spd > 4.0f) { shipVX = shipVX/spd*4.0f; shipVY = shipVY/spd*4.0f; }

    // Friction
    shipVX *= 0.97f;
    shipVY *= 0.97f;

    // Move ship + wrap
    shipX = fmod(shipX + shipVX + 128.0f, 128.0f);
    shipY = fmod(shipY + shipVY +  64.0f,  64.0f);

    // --- Fire ---
    if (btn(BTN_A)) {
      if (!fireLatch && numBullets < AST_MAX_BULLETS) {
        bX[numBullets] = shipX;
        bY[numBullets] = shipY;
        bVX[numBullets] = cos(shipAngle) * 4.5f + shipVX;
        bVY[numBullets] = sin(shipAngle) * 4.5f + shipVY;
        bLife[numBullets] = 18;
        numBullets++;
        fireLatch = true;
      }
    } else {
      fireLatch = false;
    }

    // --- Update bullets ---
    for (int i = 0; i < numBullets; ) {
      bX[i] = fmod(bX[i] + bVX[i] + 128.0f, 128.0f);
      bY[i] = fmod(bY[i] + bVY[i] +  64.0f,  64.0f);
      bLife[i]--;
      if (bLife[i] <= 0) { removeBullet(i); } else { i++; }
    }

    // --- Update asteroids ---
    for (int i = 0; i < numAsteroids; i++) {
      aX[i] = fmod(aX[i] + aVX[i] + 128.0f, 128.0f);
      aY[i] = fmod(aY[i] + aVY[i] +  64.0f,  64.0f);
    }

    // --- Bullet vs asteroid ---
    for (int b = 0; b < numBullets; ) {
      bool hit = false;
      for (int a = 0; a < numAsteroids; a++) {
        float dx = bX[b] - aX[a];
        float dy = bY[b] - aY[a];
        float r  = aSize[a];
        if (dx*dx + dy*dy < r*r) {
          // Break asteroid
          int pts = (aSize[a] > 6) ? 20 : (aSize[a] > 3 ? 50 : 100);
          score += pts;
          checkAndSaveHigh(highScore, score, "ast_hi");
          splitAsteroid(a);
          removeBullet(b);
          hit = true;
          break;
        }
      }
      if (!hit) b++;
    }

    // --- Ship vs asteroid ---
    for (int a = 0; a < numAsteroids; a++) {
      float dx = shipX - aX[a];
      float dy = shipY - aY[a];
      float r  = aSize[a] + AST_SHIP_HALF;
      if (dx*dx + dy*dy < r*r) {
        gameOver = true;
        checkAndSaveHigh(highScore, score, "ast_hi");
        return;
      }
    }

    // --- Wave clear ---
    if (numAsteroids == 0) {
      display.clearDisplay();
      display.setCursor(24, 28);
      display.print("WAVE CLEAR!");
      display.display();
      delay(800);
      spawnWave(min(4 + (int)(score / 300), AST_MAX_ASTEROIDS));
    }
  }

  void draw() override {
    display.clearDisplay();

    // HUD
    display.setCursor(0, 0);
    display.print("S:");
    display.print(score);
    display.setCursor(70, 0);
    display.print("HI:");
    display.print(highScore);

    // Asteroids (circles approximated as squares)
    for (int i = 0; i < numAsteroids; i++) {
      int r = (int)aSize[i];
      display.drawRect((int)aX[i]-r, (int)aY[i]-r, r*2, r*2, SSD1306_WHITE);
      // Add an inner detail pixel for larger ones
      if (r > 5) {
        display.drawLine((int)aX[i]-r+2, (int)aY[i],
                         (int)aX[i]+r-2, (int)aY[i], SSD1306_WHITE);
      }
    }

    // Bullets
    for (int i = 0; i < numBullets; i++)
      display.fillRect((int)bX[i], (int)bY[i], 2, 2, SSD1306_WHITE);

    // Ship — triangle pointing in shipAngle direction
    {
      float ca = cos(shipAngle), sa = sin(shipAngle);
      // Nose
      int nx = (int)(shipX + ca * 5);
      int ny = (int)(shipY + sa * 5);
      // Left wing
      int lx = (int)(shipX + cos(shipAngle + 2.4f) * 4);
      int ly = (int)(shipY + sin(shipAngle + 2.4f) * 4);
      // Right wing
      int rx = (int)(shipX + cos(shipAngle - 2.4f) * 4);
      int ry = (int)(shipY + sin(shipAngle - 2.4f) * 4);

      display.drawLine(nx, ny, lx, ly, SSD1306_WHITE);
      display.drawLine(nx, ny, rx, ry, SSD1306_WHITE);
      display.drawLine(lx, ly, rx, ry, SSD1306_WHITE);

      // Thrust flame
      if (thrust && (millis() / 100) % 2 == 0) {
        int fx = (int)(shipX - ca * 5);
        int fy = (int)(shipY - sa * 5);
        display.drawLine((int)shipX, (int)shipY, fx, fy, SSD1306_WHITE);
      }
    }

    if (gameOver) {
      display.fillRect(14, 12, 100, 44, SSD1306_BLACK);
      display.drawRect(14, 12, 100, 44, SSD1306_WHITE);
      display.setCursor(24, 15);
      display.print("GAME OVER!");
      display.setCursor(20, 27);
      display.print("SCORE:");
      display.print(score);
      display.setCursor(20, 37);
      display.print("HI:");
      display.print(highScore);
      display.setCursor(20, 47);
      display.print("START=RETRY");
    }

    pause.draw();
    display.display();
  }

private:
  // Ship
  float shipX, shipY, shipVX, shipVY, shipAngle;
  bool  thrust;

  // Bullets
  float bX[AST_MAX_BULLETS], bY[AST_MAX_BULLETS];
  float bVX[AST_MAX_BULLETS], bVY[AST_MAX_BULLETS];
  int   bLife[AST_MAX_BULLETS];
  int   numBullets;
  bool  fireLatch;

  // Asteroids
  float aX[AST_MAX_ASTEROIDS], aY[AST_MAX_ASTEROIDS];
  float aVX[AST_MAX_ASTEROIDS], aVY[AST_MAX_ASTEROIDS];
  float aSize[AST_MAX_ASTEROIDS];
  int   numAsteroids;

  long  score, highScore;
  bool  gameOver, restartLatch;
  unsigned long lastUpdate;
  PauseMenu pause;

  void removeBullet(int i) {
    numBullets--;
    bX[i] = bX[numBullets]; bY[i] = bY[numBullets];
    bVX[i] = bVX[numBullets]; bVY[i] = bVY[numBullets];
    bLife[i] = bLife[numBullets];
  }

  void spawnWave(int count) {
    numAsteroids = 0;
    for (int i = 0; i < count && numAsteroids < AST_MAX_ASTEROIDS; i++) {
      // Spawn off screen edge, not on top of ship
      float x, y;
      do {
        x = random(128);
        y = random(64);
      } while (abs(x - shipX) < 20 && abs(y - shipY) < 20);
      addAsteroid(x, y, 9.0f);
    }
  }

  void addAsteroid(float x, float y, float sz) {
    if (numAsteroids >= AST_MAX_ASTEROIDS) return;
    aX[numAsteroids] = x;
    aY[numAsteroids] = y;
    float spd = 0.5f + (float)random(100) / 100.0f;
    float ang = (float)random(628) / 100.0f;
    aVX[numAsteroids] = cos(ang) * spd;
    aVY[numAsteroids] = sin(ang) * spd;
    aSize[numAsteroids] = sz;
    numAsteroids++;
  }

  void splitAsteroid(int idx) {
    float x = aX[idx], y = aY[idx], sz = aSize[idx];
    // Remove this asteroid (swap with last)
    numAsteroids--;
    aX[idx] = aX[numAsteroids]; aY[idx] = aY[numAsteroids];
    aVX[idx] = aVX[numAsteroids]; aVY[idx] = aVY[numAsteroids];
    aSize[idx] = aSize[numAsteroids];
    // Spawn two smaller ones if big enough
    if (sz > 4.0f) {
      addAsteroid(x, y, sz * 0.55f);
      addAsteroid(x, y, sz * 0.55f);
    }
  }
};

