#pragma once
#include "Config.h"
#include "Game.h"
#include "Menu.h"
#include "PauseMenu.h"

/* ================= TETRIS CONST ================= */
#define GW 10
#define GH 20
#define BW 6
#define BH 3
#define KILL_LINE_Y 2

#define NEXT_X 72
#define NEXT_Y 41
#define NEXT_BW 4
#define NEXT_BH 4


/* ================= PIECES ================= */
const byte PIECES[7][4][4][4] = {
  {{{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},
   {{0,0,1,0},{0,0,1,0},{0,0,1,0},{0,0,1,0}},
   {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},
   {{0,1,0,0},{0,1,0,0},{0,1,0,0},{0,1,0,0}}},

  {{{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
   {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
   {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
   {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}}},

  {{{0,1,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
   {{0,1,0,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}},
   {{0,0,0,0},{1,1,1,0},{0,1,0,0},{0,0,0,0}},
   {{0,1,0,0},{1,1,0,0},{0,1,0,0},{0,0,0,0}}},

  {{{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
   {{0,1,0,0},{0,1,1,0},{0,0,1,0},{0,0,0,0}},
   {{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
   {{0,1,0,0},{0,1,1,0},{0,0,1,0},{0,0,0,0}}},

  {{{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
   {{0,0,1,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}},
   {{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
   {{0,0,1,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}}},

  {{{1,0,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
   {{0,1,1,0},{0,1,0,0},{0,1,0,0},{0,0,0,0}},
   {{0,0,0,0},{1,1,1,0},{0,0,1,0},{0,0,0,0}},
   {{0,1,0,0},{0,1,0,0},{1,1,0,0},{0,0,0,0}}},

  {{{0,0,1,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
   {{0,1,0,0},{0,1,0,0},{0,1,1,0},{0,0,0,0}},
   {{0,0,0,0},{1,1,1,0},{1,0,0,0},{0,0,0,0}},
   {{1,1,0,0},{0,1,0,0},{0,1,0,0},{0,0,0,0}}}
};

/* ================= TETRIS CLASS ================= */
class Tetris : public Game {
public:
  void init() override {
    reset();
    restartLatch = false;
  }

  void update() override {
    // --- GAME OVER ---
    if (gameOver) {
      if (btn(BTN_START) && !restartLatch) {
        reset();
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
        reset();
        return;
      }
      else if (sel == 2) {   // QUIT
        currentGame = &menu;
        currentGame->init();
        return;
      }
    }

    if (pause.isActive()) return;

    // --- GAMEPLAY ---
    if (millis() - inputTimer > inputDelay) {
      inputTimer = millis();
      handleInput();
    }

    if (millis() - fallTimer > getFallDelay()) {
      fallTimer = millis();
      if (!collide(px, py + 1, rot)) py++;
      else lockPiece();
    }
  }

  void draw() override {
    display.clearDisplay();

    display.drawLine(0, KILL_LINE_Y * BH, GW * BW - 1,
                     KILL_LINE_Y * BH, SSD1306_WHITE);

    drawField();
    drawPiece();
    drawGhost();
    drawNext();
    drawScore();

    if (gameOver) {
      display.fillRect(0, 0, 128, 64, SSD1306_BLACK);
      display.setCursor(36, 20);
      display.print("GAME OVER");
      display.setCursor(30, 36);
      display.print("START=RETRY");
      display.setCursor(66, 2);
      display.print("SCORE");
      display.setCursor(66, 12);
      display.print(score);
      display.setCursor(0, 2);
      display.print("HI");
      display.setCursor(0, 12);
      display.print(highScore);
    }
    pause.draw();
    display.display();
  }

private:
  int field[GH][GW];
  int curPiece, nextPiece, rot, px, py;
  int bag[7], bagIndex;
  long score;
  long highScore;
  int linesCleared;
  bool gameOver, hardDropLatch;
  PauseMenu pause;
  bool restartLatch;
  unsigned long fallTimer, inputTimer;
  const unsigned long inputDelay = 120;

  /* ---------- DIFFICULTY ---------- */
  unsigned long getFallDelay() {
    unsigned long d = 500 - (linesCleared / 5) * 40;
    return d < 100 ? 100 : d;
  }

  /* ---------- CORE ---------- */
  void reset() {
    memset(field, 0, sizeof(field));
    highScore = loadHighScore("tetris_hi");
    score = 0;
    linesCleared = 0;
    gameOver = false;
    hardDropLatch = false;
    refillBag();
    curPiece = drawFromBag();
    nextPiece = drawFromBag();
    rot = 0; px = 3; py = 0;
    fallTimer = millis();
    inputTimer = millis();
  }

  void refillBag() {
    for (int i = 0; i < 7; i++) bag[i] = i;
    for (int i = 6; i > 0; i--) {
      int j = random(i + 1);
      int t = bag[i]; bag[i] = bag[j]; bag[j] = t;
    }
    bagIndex = 0;
  }

  int drawFromBag() {
    if (bagIndex >= 7) refillBag();
    return bag[bagIndex++];
  }

  bool collide(int nx, int ny, int r) {
    for (int y = 0; y < 4; y++)
      for (int x = 0; x < 4; x++)
        if (PIECES[curPiece][r][y][x]) {
          int gx = nx + x, gy = ny + y;
          if (gx < 0 || gx >= GW || gy >= GH) return true;
          if (gy >= 0 && field[gy][gx]) return true;
        }
    return false;
  }

  int ghostY() {
    int gy = py;
    while (!collide(px, gy + 1, rot)) gy++;
    return gy;
  }

  void clearLines() {
    int lines = 0;
    for (int y = GH - 1; y >= 0; y--) {
      bool full = true;
      for (int x = 0; x < GW; x++) if (!field[y][x]) full = false;
      if (full) {
        lines++;
        for (int yy = y; yy > 0; yy--)
          for (int x = 0; x < GW; x++)
            field[yy][x] = field[yy - 1][x];
        y++;
      }
    }
    linesCleared += lines;
    if (lines == 1) score += 40;
    else if (lines == 2) score += 100;
    else if (lines == 3) score += 300;
    else if (lines == 4) score += 1200;
  }

  void lockPiece() {
    for (int y = 0; y < 4; y++)
      for (int x = 0; x < 4; x++)
        if (PIECES[curPiece][rot][y][x]) {
          int gx = px + x, gy = py + y;
          if (gy >= 0) field[gy][gx] = 1;
        }

    clearLines();

    for (int y = 0; y <= KILL_LINE_Y; y++)
      for (int x = 0; x < GW; x++)
        if (field[y][x]) {
          gameOver = true;
          checkAndSaveHigh(highScore, score, "tetris_hi");
          return;
        }

    curPiece = nextPiece;
    nextPiece = drawFromBag();
    rot = 0; px = 3; py = 0;
  }

  void handleInput() {
    if (btn(BTN_LEFT) && !collide(px - 1, py, rot)) px--;
    if (btn(BTN_RIGHT) && !collide(px + 1, py, rot)) px++;
    if (btn(BTN_DOWN) && !collide(px, py + 1, rot)) py++;

    if (btn(BTN_A) || btn(BTN_UP)) {
      int nr = (rot + 1) % 4;
      if (!collide(px, py, nr)) rot = nr;
    }

    if (btn(BTN_B) && !hardDropLatch) {
      while (!collide(px, py + 1, rot)) py++;
      lockPiece();
      hardDropLatch = true;
    }
    if (!btn(BTN_B)) hardDropLatch = false;
  }

  /* ---------- DRAW ---------- */
  void drawField() {
    for (int y = 0; y < GH; y++)
      for (int x = 0; x < GW; x++)
        if (field[y][x])
          display.fillRect(x * BW, y * BH, BW - 1, BH - 1, SSD1306_WHITE);
  }

  void drawPiece() {
    for (int y = 0; y < 4; y++)
      for (int x = 0; x < 4; x++)
        if (PIECES[curPiece][rot][y][x])
          display.fillRect((px + x) * BW, (py + y) * BH,
                           BW - 1, BH - 1, SSD1306_WHITE);
  }

  void drawGhost() {
    if (!settings.ghostBlocks) return;

    int gy = ghostY();
    for (int y = 0; y < 4; y++)
      for (int x = 0; x < 4; x++)
        if (PIECES[curPiece][rot][y][x]) {
          int gx = (px + x) * BW;
          int yy = (gy + y) * BH;
          display.drawRect(gx, yy, BW - 3, BH - 2, SSD1306_WHITE);
        }
  }

  void drawNext() {
    display.setCursor(66, 32);
    display.print("NEXT");
    display.drawRect(NEXT_X - 2, NEXT_Y - 2,
                     4 * NEXT_BW + 4, 4 * NEXT_BH + 4, SSD1306_WHITE);

    for (int y = 0; y < 4; y++)
      for (int x = 0; x < 4; x++)
        if (PIECES[nextPiece][0][y][x])
          display.fillRect(NEXT_X + x * NEXT_BW,
                           NEXT_Y + y * NEXT_BH,
                           NEXT_BW - 1, NEXT_BH - 1, SSD1306_WHITE);
  }

  void drawScore() {
    display.setCursor(66, 0);
    display.print("SCORE");
    display.setCursor(66, 8);
    display.print(score);
    display.setCursor(66, 16);
    display.print("HI:");
    display.setCursor(66, 24);
    display.print(highScore);
    display.setCursor(66, 56);
    display.print("LV:");
    display.print(linesCleared / 5 + 1);
  }
};


