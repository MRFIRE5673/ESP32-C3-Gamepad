#pragma once
#include "Config.h"
#include "Game.h"
#include "Menu.h"
#include "PauseMenu.h"

/* ================= Minesweeper CLASS ================= */
class Minesweeper : public Game {
public:
  void init() override {
    cx = 6; cy = 3; // Start cursor at center
    score = 0;
    highScore = loadHighScore("mines_hi");
    gameOver = false;
    won = false;
    firstMove = true;
    revealedCount = 0;
    flagCount = 0;
    elapsedSeconds = 0;
    startTime = 0;
    restartLatch = false;
    memset(board, 0, sizeof(board));
    pause.close();
  }

  void update() override {
    if (gameOver || won) {
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

    // Movement
    static unsigned long lastInput = 0;
    if (millis() - lastInput > 150) {
      if (btn(BTN_UP) && cy > 0)    { cy--; lastInput = millis(); }
      if (btn(BTN_DOWN) && cy < ROWS-1) { cy++; lastInput = millis(); }
      if (btn(BTN_LEFT) && cx > 0)  { cx--; lastInput = millis(); }
      if (btn(BTN_RIGHT) && cx < COLS-1) { cx++; lastInput = millis(); }
    }

    // Action A - Reveal
    static bool aLatch = false;
    if (btn(BTN_A)) {
      if (!aLatch) {
        aLatch = true;
        if (!(board[cy][cx] & 4)) { // Can't reveal flagged
          if (firstMove) {
            generateBoard(cx, cy);
            firstMove = false;
            startTime = millis();
          }
          
          if (board[cy][cx] & 1) { // Hit mine
            gameOver = true;
            revealAll();
            checkAndSaveHigh(highScore, score, "mines_hi");
          } else {
            revealCell(cx, cy);
            checkWin();
          }
        }
      }
    } else {
      aLatch = false;
    }

    // Action B - Flag
    static bool bLatch = false;
    if (btn(BTN_B)) {
      if (!bLatch) {
        bLatch = true;
        if (!(board[cy][cx] & 2)) { // Can't flag revealed
          board[cy][cx] ^= 4; // Toggle flag bit
          if (board[cy][cx] & 4) flagCount++;
          else flagCount--;
        }
      }
    } else {
      bLatch = false;
    }

    // Update timer
    if (!firstMove && !gameOver && !won) {
      elapsedSeconds = (millis() - startTime) / 1000;
    }
  }

  void draw() override {
    display.clearDisplay();

    // HUD
    display.setCursor(0, 0);
    display.print("M:");
    display.print(max(0, (int)settings.minesCount - flagCount));
    display.setCursor(44, 0);
    display.print("T:");
    display.print(elapsedSeconds);
    display.setCursor(84, 0);
    display.print("HI:");
    display.print(highScore);

    // Draw grid board
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        int cellX = 16 + x * 8;
        int cellY = 14 + y * 8;
        
        display.drawRect(cellX, cellY, 9, 9, SSD1306_WHITE); // Share borders

        bool isMine = board[y][x] & 1;
        bool isRevealed = board[y][x] & 2;
        bool isFlagged = board[y][x] & 4;
        int count = (board[y][x] >> 4) & 15;

        if (isRevealed) {
          if (isMine) {
            // Draw mine icon (X)
            display.drawLine(cellX + 2, cellY + 2, cellX + 6, cellY + 6, SSD1306_WHITE);
            display.drawLine(cellX + 6, cellY + 2, cellX + 2, cellY + 6, SSD1306_WHITE);
          } else if (count > 0) {
            // Draw count
            display.setCursor(cellX + 2, cellY + 1);
            display.print(count);
          }
        } else {
          // Unrevealed
          if (isFlagged) {
            // Flag block
            display.fillRect(cellX + 1, cellY + 1, 7, 7, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            display.setCursor(cellX + 2, cellY + 1);
            display.print("F");
            display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
          } else {
            // Unrevealed clean box
            display.fillRect(cellX + 2, cellY + 2, 5, 5, SSD1306_WHITE);
          }
        }
      }
    }

    // Blinking Cursor
    if ((millis() / 250) % 2 == 0) {
      display.drawRect(16 + cx * 8 - 1, 14 + cy * 8 - 1, 11, 11, SSD1306_WHITE);
    }

    if (gameOver || won) {
      display.fillRect(14, 12, 100, 44, SSD1306_BLACK);
      display.drawRect(14, 12, 100, 44, SSD1306_WHITE);
      display.setCursor(30, 15);
      display.print(won ? "YOU WIN!" : "GAME OVER!");
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
  static const int COLS = 12;
  static const int ROWS = 6;
  uint8_t board[ROWS][COLS];
  int cx, cy;
  long score, highScore;
  bool gameOver, won, firstMove;
  int revealedCount, flagCount;
  unsigned long elapsedSeconds, startTime;
  bool restartLatch;
  PauseMenu pause;

  void generateBoard(int safeX, int safeY) {
    memset(board, 0, sizeof(board));
    int totalMines = settings.minesCount;
    int minesPlaced = 0;
    
    while (minesPlaced < totalMines) {
      int rx = random(COLS);
      int ry = random(ROWS);
      
      if (abs(rx - safeX) <= 1 && abs(ry - safeY) <= 1) continue;
      if (board[ry][rx] & 1) continue;
      
      board[ry][rx] |= 1;
      minesPlaced++;
    }

    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        if (board[y][x] & 1) continue;
        int count = 0;
        for (int dy = -1; dy <= 1; dy++) {
          for (int dx = -1; dx <= 1; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if (nx >= 0 && nx < COLS && ny >= 0 && ny < ROWS) {
              if (board[ny][nx] & 1) count++;
            }
          }
        }
        board[y][x] |= (count << 4);
      }
    }
  }

  void revealCell(int x, int y) {
    if (x < 0 || x >= COLS || y < 0 || y >= ROWS) return;
    if (board[y][x] & 2) return;
    if (board[y][x] & 4) return;

    board[y][x] |= 2;
    revealedCount++;
    score += 10;

    int count = (board[y][x] >> 4) & 15;
    if (count == 0 && !(board[y][x] & 1)) {
      for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
          if (dx == 0 && dy == 0) continue;
          revealCell(x + dx, y + dy);
        }
      }
    }
  }

  void revealAll() {
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        if (board[y][x] & 1) board[y][x] |= 2; // Reveal all mines
      }
    }
  }

  void checkWin() {
    int totalCells = COLS * ROWS;
    if (revealedCount == totalCells - settings.minesCount) {
      won = true;
      // Bonus score for speed
      long timeBonus = 1000 - elapsedSeconds;
      if (timeBonus < 0) timeBonus = 0;
      score += timeBonus;
      checkAndSaveHigh(highScore, score, "mines_hi");
    }
  }
};

