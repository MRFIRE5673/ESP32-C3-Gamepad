#pragma once
#include "Config.h"
#include "Game.h"
#include "Menu.h"
#include "PauseMenu.h"

/* ================= Breakout CONST ================= */
#define BRK_HUD        8
#define BRK_PADDLE_W   18
#define BRK_PADDLE_H   2
#define BRK_BALL_SZ    2
#define BRK_BRICK_W    10
#define BRK_BRICK_H    4
#define BRK_COLS       11
#define BRK_ROWS       5


/* ================= BREAKOUT CLASS ================= */
class Breakout : public Game {
public:
  void init() override {
    score = 0;
    highScore = loadHighScore("brk_hi");
    lives = 3;
    level = 0;
    gameOver = false;
    restartLatch = false;
    flapLatch = false;
    startLevel();
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
    if (now - lastUpdate < 16) return;   // ~60 fps cap
    lastUpdate = now;

    // --- Paddle movement ---
    if (btn(BTN_LEFT)  && paddleX > 0) paddleX -= 3;
    if (btn(BTN_RIGHT) && paddleX + BRK_PADDLE_W < 128) paddleX += 3;

    // --- Serve / hold ball ---
    if (ballHeld) {
      ballX = paddleX + BRK_PADDLE_W / 2 - BRK_BALL_SZ / 2;
      ballY = 64 - BRK_PADDLE_H - BRK_BALL_SZ - 2;
      if (btn(BTN_A) || btn(BTN_UP)) {
        if (!flapLatch) { ballHeld = false; flapLatch = true; }
      } else { flapLatch = false; }
      return;
    }

    // --- Ball physics ---
    ballX += ballDX;
    ballY += ballDY;

    // Wall bounces
    if (ballX <= 0)            { ballX = 0;                        ballDX =  abs(ballDX); }
    if (ballX >= 128-BRK_BALL_SZ) { ballX = 128-BRK_BALL_SZ;     ballDX = -abs(ballDX); }
    if (ballY <= BRK_HUD)      { ballY = BRK_HUD;                  ballDY =  abs(ballDY); }

    // Paddle collision
    int py = 64 - BRK_PADDLE_H - 2;
    if (ballDY > 0 &&
        ballY + BRK_BALL_SZ >= py &&
        ballX + BRK_BALL_SZ >= paddleX &&
        ballX <= paddleX + BRK_PADDLE_W) {
      ballDY = -abs(ballDY);
      // Angle based on hit position
      int offset = (ballX + BRK_BALL_SZ / 2) - (paddleX + BRK_PADDLE_W / 2);
      ballDX = offset / 3;
      if (ballDX == 0) ballDX = 1;
      ballY = py - BRK_BALL_SZ;
    }

    // Ball lost
    if (ballY > 64) {
      lives--;
      if (lives <= 0) {
        gameOver = true;
        checkAndSaveHigh(highScore, score, "brk_hi");
      } else {
        ballHeld = true;
      }
    }

    // Brick collision
    for (int r = 0; r < BRK_ROWS; r++) {
      for (int c = 0; c < BRK_COLS; c++) {
        if (!bricks[r][c]) continue;
        int bx = c * BRK_BRICK_W + 1;
        int by = BRK_HUD + 2 + r * (BRK_BRICK_H + 1);
        // AABB overlap
        if (ballX + BRK_BALL_SZ > bx && ballX < bx + BRK_BRICK_W &&
            ballY + BRK_BALL_SZ > by && ballY < by + BRK_BRICK_H) {
          bricks[r][c] = false;
          bricksLeft--;
          score += 10 + level * 2;
          // Reflect ball
          float overlapL = (ballX + BRK_BALL_SZ) - bx;
          float overlapR = (bx + BRK_BRICK_W) - ballX;
          float overlapT = (ballY + BRK_BALL_SZ) - by;
          float overlapB = (by + BRK_BRICK_H) - ballY;
          float minH = overlapL < overlapR ? overlapL : overlapR;
          float minV = overlapT < overlapB ? overlapT : overlapB;
          if (minH < minV) ballDX = -ballDX;
          else             ballDY = -ballDY;
        }
      }
    }

    // Level clear
    if (bricksLeft == 0) {
      level++;
      // Flash message
      display.clearDisplay();
      display.setCursor(30, 24);
      display.print("LEVEL ");
      display.print(level + 1);
      display.display();
      delay(1000);
      startLevel();
    }
  }

  void draw() override {
    display.clearDisplay();

    // HUD
    display.setCursor(0, 0);
    display.print("S:");
    display.print(score);
    display.setCursor(54, 0);
    display.print("HI:");
    display.print(highScore);
    display.setCursor(102, 0);
    display.print("LV");
    display.print(level + 1);

    // Bricks
    for (int r = 0; r < BRK_ROWS; r++) {
      for (int c = 0; c < BRK_COLS; c++) {
        if (!bricks[r][c]) continue;
        int bx = c * BRK_BRICK_W + 1;
        int by = BRK_HUD + 2 + r * (BRK_BRICK_H + 1);
        display.fillRect(bx, by, BRK_BRICK_W - 1, BRK_BRICK_H - 1, SSD1306_WHITE);
        // Inner line for style
        if (BRK_BRICK_H > 2)
          display.drawFastHLine(bx + 1, by + 1, BRK_BRICK_W - 3, SSD1306_BLACK);
      }
    }

    // Ball
    display.fillRect(ballX, ballY, BRK_BALL_SZ, BRK_BALL_SZ, SSD1306_WHITE);

    // Paddle
    int py = 64 - BRK_PADDLE_H - 2;
    display.fillRect(paddleX, py, BRK_PADDLE_W, BRK_PADDLE_H, SSD1306_WHITE);

    // Lives as small dots
    for (int i = 0; i < lives && i < 5; i++)
      display.fillRect(1 + i * 4, 60, 3, 3, SSD1306_WHITE);

    // Hold hint
    if (ballHeld) {
      display.setCursor(24, 28);
      display.print("[A] to serve");
    }

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
  bool bricks[BRK_ROWS][BRK_COLS];
  int  bricksLeft;
  int  paddleX;
  int  ballX, ballY, ballDX, ballDY;
  bool ballHeld;
  int  lives, level;
  long score, highScore;
  bool gameOver, restartLatch, flapLatch;
  unsigned long lastUpdate;
  PauseMenu pause;

  void startLevel() {
    // Fill all bricks
    for (int r = 0; r < BRK_ROWS; r++)
      for (int c = 0; c < BRK_COLS; c++)
        bricks[r][c] = true;
    bricksLeft = BRK_ROWS * BRK_COLS;

    // Reset paddle & ball
    paddleX = 64 - BRK_PADDLE_W / 2;
    ballHeld = true;
    flapLatch = false;
    ballX = paddleX + BRK_PADDLE_W / 2 - BRK_BALL_SZ / 2;
    ballY = 64 - BRK_PADDLE_H - BRK_BALL_SZ - 2;
    // Speed increases slightly each level
    int spd = 1 + level / 3;
    if (spd > 3) spd = 3;
    ballDX = spd;
    ballDY = -(spd + 1);
    lastUpdate = millis();
  }
};

