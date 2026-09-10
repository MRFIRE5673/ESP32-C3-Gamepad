#pragma once
#include "Config.h"
#include "Game.h"
#include "Menu.h"
#include "PauseMenu.h"

/* ================= Snake CONST ================= */
#define SNAKE_CELL 6
#define BORDER_PX 1
#define HUD_H 10
#define SNAKE_W ((SCREEN_WIDTH - 2*BORDER_PX) / SNAKE_CELL)
#define SNAKE_H ((SCREEN_HEIGHT - HUD_H - 2*BORDER_PX) / SNAKE_CELL)
#define PLAY_X BORDER_PX
#define PLAY_Y HUD_H


/* ================= SNAKE CLASS ================= */
class Snake : public Game {
public:
  void init() override {
    length = 3;
    dir = RIGHT;
    highScore = loadHighScore("snake_hi");
    score = 0;
    gameOver = false;

    sx[0] = 10; sy[0] = 5;
    sx[1] = 9;  sy[1] = 5;
    sx[2] = 8;  sy[2] = 5;

    spawnFruit();
    lastMove = millis();
    pause.close();
  }

  void update() override {
    static bool restartLatch = false;

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

      if (sel == 1) {        // RESTART
        init();
        return;
      }
      else if (sel == 2) {   // QUIT
        currentGame = &menu;
        currentGame->init();
        return;
      }
    }
    if (pause.isActive()) return;

    handleInput();

    if (millis() - lastMove > getMoveDelay()) {
      move();
      lastMove = millis();
    }
  }

  void draw() override {
    display.clearDisplay();

    /* ===== HUD ===== */
    display.setCursor(0, 0);
    display.print("Score:");
    display.print(score);
    display.setCursor(70, 0);
    display.print("HI:");
    display.print(highScore);

    display.drawRect(
      PLAY_X - 1,
      PLAY_Y - 1,
      SNAKE_W * SNAKE_CELL + 2,
      SNAKE_H * SNAKE_CELL + 2,
      SSD1306_WHITE
    );

    /* ===== FRUIT ===== */
    display.fillRect(
      PLAY_X + fruitX * SNAKE_CELL,
      PLAY_Y + fruitY * SNAKE_CELL,
      SNAKE_CELL - 1,
      SNAKE_CELL - 1,
      SSD1306_WHITE
    );

    /* ===== SNAKE ===== */
    for (int i = 0; i < length; i++) {
      display.fillRect(
        PLAY_X + sx[i] * SNAKE_CELL,
        PLAY_Y + sy[i] * SNAKE_CELL,
        SNAKE_CELL - 1,
        SNAKE_CELL - 1,
        SSD1306_WHITE
      );
    }

    if (gameOver) {
      display.setCursor(30, 28);
      display.print("GAME OVER");
      display.setCursor(22, 40);
      display.print("START=RETRY");
    }
    pause.draw();
    display.display();
  }

private:
  enum Direction { UP, DOWN, LEFT, RIGHT };

  int sx[200], sy[200];
  int length;
  int fruitX, fruitY;
  long score;
  long highScore;
  PauseMenu pause;
  bool gameOver;
  Direction dir;
  unsigned long lastMove;

  /* ---------- DIFFICULTY ---------- */
  unsigned long getMoveDelay() {
    unsigned long d = 180 - (score / 50) * 10;
    return d < 80 ? 80 : d;
  }

  /* ---------- LOGIC ---------- */

  void handleInput() {
    if (btn(BTN_UP) && dir != DOWN) dir = UP;
    if (btn(BTN_DOWN) && dir != UP) dir = DOWN;
    if (btn(BTN_LEFT) && dir != RIGHT) dir = LEFT;
    if (btn(BTN_RIGHT) && dir != LEFT) dir = RIGHT;
  }

  void move() {
    // shift body
    for (int i = length - 1; i > 0; i--) {
      sx[i] = sx[i - 1];
      sy[i] = sy[i - 1];
    }

    // move head
    if (dir == UP) sy[0]--;
    if (dir == DOWN) sy[0]++;
    if (dir == LEFT) sx[0]--;
    if (dir == RIGHT) sx[0]++;

    // wall collision
    if (sx[0] < 0 || sx[0] >= SNAKE_W || sy[0] < 0 || sy[0] >= SNAKE_H) {
      gameOver = true;
      checkAndSaveHigh(highScore, score, "snake_hi");
      return;
    }

    // self collision
    for (int i = 1; i < length; i++) {
      if (sx[0] == sx[i] && sy[0] == sy[i]) {
        gameOver = true;
        checkAndSaveHigh(highScore, score, "snake_hi");
        return;
      }
    }

    // fruit eaten
    if (sx[0] == fruitX && sy[0] == fruitY) {
      if (length < 199) length++;   // cap to prevent overflow
      score += 10;
      spawnFruit();
    }
  }

  void spawnFruit() {
    bool bad;
    do {
      bad = false;
      fruitX = random(1, SNAKE_W - 1);
      fruitY = random(1, SNAKE_H - 1);

      for (int i = 0; i < length; i++) {
        if (fruitX == sx[i] && fruitY == sy[i]) {
          bad = true;
          break;
        }
      }
    } while (bad);
  }
};

