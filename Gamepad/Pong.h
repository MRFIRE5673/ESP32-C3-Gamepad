#pragma once
#include "Config.h"
#include "Game.h"
#include "Menu.h"
#include "PauseMenu.h"

/* ================= Pong CONST ================= */
#define PONG_HUD       10
#define PONG_PADDLE_W  2
#define PONG_PADDLE_H  14
#define PONG_BALL_SZ   2
#define PONG_WIN_SCORE 5


/* ================= PONG CLASS ================= */
class Pong : public Game {
public:
  void init() override {
    playerScore = 0;
    aiScore = 0;
    score = 0;
    highScore = loadHighScore("pong_hi");
    gameOver = false;
    restartLatch = false;
    playerY = 32 - PONG_PADDLE_H / 2;
    aiY = 32 - PONG_PADDLE_H / 2;
    rallyCount = 0;
    resetBall();
    lastUpdate = millis();
    pause.close();
  }

  void update() override {
    // --- GAME OVER ---
    if (gameOver) {
      if (btn(BTN_START) && !restartLatch) {
        init();
        restartLatch = true;
      }
      if (!btn(BTN_START)) restartLatch = false;
      return;
    }

    // --- OPEN PAUSE ---
    static bool selectLatch = false;
    if (btn(BTN_SELECT) && !selectLatch && !pause.isActive()) {
      pause.open();
      selectLatch = true;
      return;
    }
    if (!btn(BTN_SELECT)) selectLatch = false;

    // --- PAUSE MENU ---
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

    // --- UPDATE INTERVAL based on speed ---
    unsigned long interval = 35 - (settings.pongSpeed - 1) * 8;  // 35, 27, 19 ms
    if (millis() - lastUpdate < interval) return;
    lastUpdate = millis();

    // --- PLAYER PADDLE ---
    if (btn(BTN_UP) && playerY > PONG_HUD) playerY -= 2;
    if (btn(BTN_DOWN) && playerY + PONG_PADDLE_H < 63) playerY += 2;

    // --- AI PADDLE ---
    updateAI();

    // --- BALL MOVEMENT ---
    ballX += ballDX;
    ballY += ballDY;

    // Top/bottom bounce
    if (ballY <= PONG_HUD) {
      ballY = PONG_HUD;
      ballDY = abs(ballDY);
      if (ballDY == 0) ballDY = 1;
    }
    if (ballY >= 63 - PONG_BALL_SZ) {
      ballY = 63 - PONG_BALL_SZ;
      ballDY = -abs(ballDY);
      if (ballDY == 0) ballDY = -1;
    }

    // Player paddle collision (left side)
    if (ballX <= 4 + PONG_PADDLE_W && ballX + PONG_BALL_SZ >= 4 &&
        ballY + PONG_BALL_SZ >= playerY && ballY <= playerY + PONG_PADDLE_H &&
        ballDX < 0) {
      ballDX = abs(ballDX);
      int hitPos = (ballY + PONG_BALL_SZ / 2) - (playerY + PONG_PADDLE_H / 2);
      ballDY = hitPos / 3;
      ballX = 4 + PONG_PADDLE_W;
      rallyCount++;
      // Speed up ball slightly every 4 rallies
      if (rallyCount % 4 == 0 && abs(ballDX) < 4)
        ballDX++;
    }

    // AI paddle collision (right side)
    int aiPaddleX = 128 - PONG_PADDLE_W - 4;
    if (ballX + PONG_BALL_SZ >= aiPaddleX && ballX <= aiPaddleX + PONG_PADDLE_W &&
        ballY + PONG_BALL_SZ >= aiY && ballY <= aiY + PONG_PADDLE_H &&
        ballDX > 0) {
      ballDX = -abs(ballDX);
      int hitPos = (ballY + PONG_BALL_SZ / 2) - (aiY + PONG_PADDLE_H / 2);
      ballDY = hitPos / 3;
      ballX = aiPaddleX - PONG_BALL_SZ;
      rallyCount++;
      if (rallyCount % 4 == 0 && abs(ballDX) < 4)
        ballDX--;  // increase magnitude (ball going left, so more negative)
    }

    // --- SCORING ---
    // Ball goes past player (left)
    if (ballX < -4) {
      aiScore++;
      if (aiScore >= PONG_WIN_SCORE) {
        gameOver = true;
        checkAndSaveHigh(highScore, score, "pong_hi");
      }
      resetBall();
    }

    // Ball goes past AI (right)
    if (ballX > 132) {
      playerScore++;
      score += 100;
      if (playerScore >= PONG_WIN_SCORE) {
        // Player wins the match — bonus + new match
        score += 500;
        display.clearDisplay();
        display.setCursor(22, 20);
        display.print("MATCH WON!");
        display.setCursor(26, 36);
        display.print("BONUS: +500");
        display.display();
        delay(1200);
        playerScore = 0;
        aiScore = 0;
      }
      resetBall();
    }
  }

  void draw() override {
    display.clearDisplay();

    // HUD
    display.setCursor(0, 0);
    display.print("YOU:");
    display.print(playerScore);
    display.setCursor(48, 0);
    display.print("AI:");
    display.print(aiScore);
    display.setCursor(84, 0);
    display.print("S:");
    display.print(score);

    // Divider
    display.drawLine(0, PONG_HUD - 2, 127, PONG_HUD - 2, SSD1306_WHITE);

    // Center dashed line
    for (int y = PONG_HUD; y < 64; y += 4) {
      display.drawPixel(64, y, SSD1306_WHITE);
      display.drawPixel(64, y + 1, SSD1306_WHITE);
    }

    // Player paddle (left)
    display.fillRect(4, playerY, PONG_PADDLE_W, PONG_PADDLE_H, SSD1306_WHITE);

    // AI paddle (right)
    display.fillRect(128 - PONG_PADDLE_W - 4, aiY, PONG_PADDLE_W, PONG_PADDLE_H, SSD1306_WHITE);

    // Ball
    display.fillRect(ballX, ballY, PONG_BALL_SZ, PONG_BALL_SZ, SSD1306_WHITE);

    // Game over overlay
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
  int playerScore, aiScore;
  long score;
  long highScore;
  int playerY, aiY;
  int ballX, ballY, ballDX, ballDY;
  bool gameOver;
  bool restartLatch;
  int rallyCount;
  unsigned long lastUpdate;
  int aiUpdateCount;
  PauseMenu pause;

  void resetBall() {
    ballX = 63;
    ballY = 32;
    ballDX = (random(2) == 0) ? 2 : -2;
    ballDY = random(0, 3) - 1;   // -1, 0, or 1
    rallyCount = 0;
    aiUpdateCount = 0;
    delay(400);  // brief pause after scoring
  }

  void updateAI() {
    aiUpdateCount++;
    // AI reacts every frame at all speeds — speed controls tracking precision
    int reactEvery = max(1, 3 - settings.pongSpeed);  // 2, 1, 1
    if (aiUpdateCount % reactEvery != 0) return;

    // Always track ball position (prevents missing post-serve balls)
    int target = ballY - PONG_PADDLE_H / 2;

    // Add prediction when ball moves toward AI
    if (ballDX > 0) {
      target += ballDY * 3;  // predict ahead
    }

    // Speed = pongSpeed directly (1, 2, or 3 px per update)
    int speed = settings.pongSpeed;
    if (aiY < target - 1) aiY += speed;
    else if (aiY > target + 1) aiY -= speed;

    // Clamp
    if (aiY < PONG_HUD) aiY = PONG_HUD;
    if (aiY + PONG_PADDLE_H > 63) aiY = 63 - PONG_PADDLE_H;
  }
};

