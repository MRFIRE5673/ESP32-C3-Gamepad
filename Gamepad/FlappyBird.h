#pragma once
#include "Config.h"
#include "Game.h"
#include "Menu.h"
#include "PauseMenu.h"

/* ================= Flappy Bird CLASS ================= */
class FlappyBird : public Game {
public:
  void init() override {
    birdY = 32.0;
    birdVy = 0.0;
    score = 0;
    highScore = loadHighScore("flappy_hi");
    gameOver = false;
    restartLatch = false;
    flapLatch = false;

    // Initialize pipes
    pipeX[0] = 128;
    pipeGapY[0] = random(12, 36);
    pipeX[1] = 128 + 70;
    pipeGapY[1] = random(12, 36);

    lastUpdate = millis();
    pause.close();
  }

  void update() override {
    if (gameOver) {
      if (btn(BTN_START) && !restartLatch) {
        init();
        restartLatch = true;
      }
      if (!btn(BTN_START)) restartLatch = false;
      return;
    }

    static bool selectLatch = false;
    if (btn(BTN_SELECT) && !selectLatch && !pause.isActive()) {
      pause.open();
      selectLatch = true;
      return;
    }
    if (!btn(BTN_SELECT)) selectLatch = false;

    pause.update();
    if (pause.hasSelection()) {
      int sel = pause.getSelection();
      if (sel == 1) { init(); return; }
      else if (sel == 2) {
        currentGame = &menu;
        currentGame->init();
        return;
      }
    }
    if (pause.isActive()) return;

    unsigned long now = millis();
    if (now - lastUpdate < 30) return;
    lastUpdate = now;

    if (btn(BTN_A) || btn(BTN_UP)) {
      if (!flapLatch) {
        birdVy = -1.6;
        flapLatch = true;
      }
    } else {
      flapLatch = false;
    }

    birdVy += 0.15;
    if (birdVy > 4.0) birdVy = 4.0;
    birdY += birdVy;

    if (birdY < 8) {
      birdY = 8;
      birdVy = 0;
    }
    if (birdY > 58) {
      gameOver = true;
      checkAndSaveHigh(highScore, score, "flappy_hi");
    }

    for (int i = 0; i < 2; i++) {
      pipeX[i] -= 1;

      if (pipeX[i] == 19) {
        score++;
      }

      if (pipeX[i] < -12) {
        pipeX[i] = 128;
        pipeGapY[i] = random(12, 36);
      }

      int birdLeft = 20;
      int birdRight = 25;
      int birdTop = (int)birdY;
      int birdBottom = (int)birdY + 4;

      int pipeLeft = pipeX[i];
      int pipeRight = pipeX[i] + 12;
      int gapTop = pipeGapY[i];
      int gapBottom = pipeGapY[i] + 20;

      if (birdRight >= pipeLeft && birdLeft <= pipeRight) {
        if (birdTop < gapTop || birdBottom > gapBottom) {
          gameOver = true;
          checkAndSaveHigh(highScore, score, "flappy_hi");
        }
      }
    }
  }

  void draw() override {
    display.clearDisplay();

    // HUD
    display.setCursor(0, 0);
    display.print("Score:");
    display.print(score);
    display.setCursor(70, 0);
    display.print("HI:");
    display.print(highScore);

    // Ground
    display.drawLine(0, 63, 127, 63, SSD1306_WHITE);

    // Pipes
    for (int i = 0; i < 2; i++) {
      if (pipeX[i] >= 128 || pipeX[i] < -12) continue;

      int px = pipeX[i];
      int gapTop = pipeGapY[i];
      int gapBottom = pipeGapY[i] + 20;

      display.fillRect(px, 8, 12, gapTop - 8, SSD1306_WHITE);
      display.fillRect(px, gapBottom, 12, 63 - gapBottom, SSD1306_WHITE);
    }

    // Bird Sprite (7x5 pixels)
    int bx = 20;
    int by = (int)birdY;
    display.fillRect(bx + 1, by, 4, 5, SSD1306_WHITE);
    display.drawPixel(bx + 5, by + 1, SSD1306_WHITE);
    display.drawPixel(bx, by + 2, SSD1306_WHITE);
    display.drawPixel(bx + 3, by + 1, SSD1306_BLACK);

    if (gameOver) {
      display.fillRect(14, 12, 100, 44, SSD1306_BLACK);
      display.drawRect(14, 12, 100, 44, SSD1306_WHITE);
      display.setCursor(30, 15);
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
  float birdY;
  float birdVy;
  long score;
  long highScore;
  bool gameOver;
  bool restartLatch;
  bool flapLatch;
  unsigned long lastUpdate;

  int pipeX[2];
  int pipeGapY[2];

  PauseMenu pause;
};

