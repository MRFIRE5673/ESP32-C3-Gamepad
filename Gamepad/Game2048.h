#pragma once
#include "Config.h"
#include "Game.h"
#include "Menu.h"
#include "PauseMenu.h"

/* ================= 2048 CONST ================= */
#define T48_GRID  4
#define T48_CELL  14
#define T48_OX    ((128 - T48_GRID * T48_CELL) / 2)
#define T48_OY    10


/* ================= 2048 CLASS ================= */
class Game2048 : public Game {
public:
  void init() override {
    score = 0;
    highScore = loadHighScore("2048_hi");
    gameOver = false;
    won = false;
    wonAck = false;
    restartLatch = false;
    memset(board, 0, sizeof(board));
    spawnTile();
    spawnTile();
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

    // Debounce
    static unsigned long lastInput = 0;
    if (millis() - lastInput < 200) return;

    bool moved = false;
    if (btn(BTN_LEFT))  { moved = slide(0); lastInput = millis(); }
    if (btn(BTN_RIGHT)) { moved = slide(1); lastInput = millis(); }
    if (btn(BTN_UP))    { moved = slide(2); lastInput = millis(); }
    if (btn(BTN_DOWN))  { moved = slide(3); lastInput = millis(); }

    if (moved) {
      checkAndSaveHigh(highScore, score, "2048_hi");
      spawnTile();
      if (!hasValidMove()) {
        gameOver = true;
        checkAndSaveHigh(highScore, score, "2048_hi");
      }
    }
  }

  void draw() override {
    display.clearDisplay();

    // HUD
    display.setCursor(0, 0);
    display.print("S:");
    display.print(score);
    display.setCursor(64, 0);
    display.print("HI:");
    display.print(highScore);

    // Grid
    for (int r = 0; r < T48_GRID; r++) {
      for (int c = 0; c < T48_GRID; c++) {
        int x = T48_OX + c * T48_CELL;
        int y = T48_OY + r * T48_CELL;
        display.drawRect(x, y, T48_CELL, T48_CELL, SSD1306_WHITE);
        if (board[r][c] > 0) {
          // Print value — handle up to 4 digits
          char buf[6];
          itoa(board[r][c], buf, 10);
          int len = strlen(buf);
          // Centre text in cell (5px char width + 1 space)
          int tx = x + (T48_CELL - len * 6) / 2;
          int ty = y + (T48_CELL - 7) / 2;
          display.setCursor(tx, ty);
          display.print(buf);
        }
      }
    }

    if (won && !wonAck) {
      display.fillRect(10, 16, 108, 32, SSD1306_BLACK);
      display.drawRect(10, 16, 108, 32, SSD1306_WHITE);
      display.setCursor(22, 20);
      display.print("YOU REACHED 2048!");
      display.setCursor(22, 32);
      display.print("A=Continue  B=New");
      if (btn(BTN_A)) wonAck = true;
      if (btn(BTN_B)) init();
    }

    if (gameOver) {
      display.fillRect(14, 12, 100, 44, SSD1306_BLACK);
      display.drawRect(14, 12, 100, 44, SSD1306_WHITE);
      display.setCursor(22, 15);
      display.print("NO MORE MOVES");
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
  uint16_t board[T48_GRID][T48_GRID];
  long score, highScore;
  bool gameOver, won, wonAck, restartLatch;
  PauseMenu pause;

  void spawnTile() {
    // Collect empty cells
    int empX[T48_GRID * T48_GRID], empY[T48_GRID * T48_GRID];
    int cnt = 0;
    for (int r = 0; r < T48_GRID; r++)
      for (int c = 0; c < T48_GRID; c++)
        if (board[r][c] == 0) { empX[cnt] = c; empY[cnt] = r; cnt++; }
    if (cnt == 0) return;
    int idx = random(cnt);
    board[empY[idx]][empX[idx]] = (random(10) < 9) ? 2 : 4;
  }

  // Slide a single row/col toward the low index
  // Returns true if anything moved or merged
  bool slideRow(uint16_t* row) {
    bool moved = false;
    // Compress (remove zeros)
    uint16_t tmp[T48_GRID] = {};
    int pos = 0;
    for (int i = 0; i < T48_GRID; i++)
      if (row[i]) tmp[pos++] = row[i];
    // Merge adjacent equal tiles
    for (int i = 0; i < T48_GRID - 1; i++) {
      if (tmp[i] && tmp[i] == tmp[i+1]) {
        tmp[i] *= 2;
        score += tmp[i];
        if (tmp[i] == 2048 && !won) won = true;
        tmp[i+1] = 0;
        moved = true;
      }
    }
    // Compress again
    uint16_t out[T48_GRID] = {};
    pos = 0;
    for (int i = 0; i < T48_GRID; i++)
      if (tmp[i]) out[pos++] = tmp[i];
    // Check if changed
    for (int i = 0; i < T48_GRID; i++) {
      if (out[i] != row[i]) moved = true;
      row[i] = out[i];
    }
    return moved;
  }

  // dir: 0=left, 1=right, 2=up, 3=down
  bool slide(int dir) {
    bool moved = false;
    uint16_t row[T48_GRID];

    if (dir == 0) { // left — slide rows left
      for (int r = 0; r < T48_GRID; r++) {
        for (int c = 0; c < T48_GRID; c++) row[c] = board[r][c];
        if (slideRow(row)) { moved = true; for (int c = 0; c < T48_GRID; c++) board[r][c] = row[c]; }
      }
    } else if (dir == 1) { // right — reverse row, slide, reverse back
      for (int r = 0; r < T48_GRID; r++) {
        for (int c = 0; c < T48_GRID; c++) row[c] = board[r][T48_GRID-1-c];
        if (slideRow(row)) { moved = true; for (int c = 0; c < T48_GRID; c++) board[r][T48_GRID-1-c] = row[c]; }
      }
    } else if (dir == 2) { // up — extract columns as rows
      for (int c = 0; c < T48_GRID; c++) {
        for (int r = 0; r < T48_GRID; r++) row[r] = board[r][c];
        if (slideRow(row)) { moved = true; for (int r = 0; r < T48_GRID; r++) board[r][c] = row[r]; }
      }
    } else { // down — reverse column
      for (int c = 0; c < T48_GRID; c++) {
        for (int r = 0; r < T48_GRID; r++) row[r] = board[T48_GRID-1-r][c];
        if (slideRow(row)) { moved = true; for (int r = 0; r < T48_GRID; r++) board[T48_GRID-1-r][c] = row[r]; }
      }
    }
    return moved;
  }

  bool hasValidMove() {
    for (int r = 0; r < T48_GRID; r++)
      for (int c = 0; c < T48_GRID; c++) {
        if (board[r][c] == 0) return true;
        if (c+1 < T48_GRID && board[r][c] == board[r][c+1]) return true;
        if (r+1 < T48_GRID && board[r][c] == board[r+1][c]) return true;
      }
    return false;
  }
};

