#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include "esp_sleep.h"
#include "driver/gpio.h"
Preferences prefs;

/* ================= DISPLAY ================= */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define SDA_PIN 8
#define SCL_PIN 9

/* ================= BUTTONS ================= */
#define BTN_UP     0
#define BTN_RIGHT  1
#define BTN_LEFT   2
#define BTN_DOWN   3
#define BTN_START  21
#define BTN_SELECT 20
#define BTN_A      10
#define BTN_B      7
#define KEEP_ALIVE_PIN 5

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

/* ================= Snake CONST ================= */
#define SNAKE_CELL 6
#define BORDER_PX 1
#define HUD_H 10
#define SNAKE_W ((SCREEN_WIDTH - 2*BORDER_PX) / SNAKE_CELL)
#define SNAKE_H ((SCREEN_HEIGHT - HUD_H - 2*BORDER_PX) / SNAKE_CELL)
#define PLAY_X BORDER_PX
#define PLAY_Y HUD_H

/* ================= Pac-Man CONST ================= */
#define PM_TILE   5
#define PM_W      28
#define PM_H      31
#define PM_HUD    8
#define PM_OX     0
#define PM_MAX_GHOSTS 4
#define PM_TUNNEL_ROW 14

#define PM_EMPTY  0
#define PM_WALL   1
#define PM_DOT    2
#define PM_POWER  3

#define GS_HOUSE  0
#define GS_ACTIVE 1
#define GS_FRIGHT 2
#define GS_EATEN  3

#define DIR_UP    0
#define DIR_DOWN  1
#define DIR_LEFT  2
#define DIR_RIGHT 3

/* ================= Pong CONST ================= */
#define PONG_HUD       10
#define PONG_PADDLE_W  2
#define PONG_PADDLE_H  14
#define PONG_BALL_SZ   2
#define PONG_WIN_SCORE 5

/* ================= Breakout CONST ================= */
#define BRK_HUD        8
#define BRK_PADDLE_W   18
#define BRK_PADDLE_H   2
#define BRK_BALL_SZ    2
#define BRK_BRICK_W    10
#define BRK_BRICK_H    4
#define BRK_COLS       11
#define BRK_ROWS       5

/* ================= Asteroids CONST ================= */
#define AST_MAX_BULLETS   5
#define AST_MAX_ASTEROIDS 8
#define AST_SHIP_HALF     4   // half-size of ship bounding box

/* ================= 2048 CONST ================= */
#define T48_GRID  4
#define T48_CELL  14
#define T48_OX    ((128 - T48_GRID * T48_CELL) / 2)
#define T48_OY    10

// ---- Forward declarations for game switching ----
class Menu;
class Game;
extern Game* currentGame;
extern Menu menu;
class Settings;
extern Settings settingsGame;

/*================= Settings struct =====================*/
struct SettingsData {
  uint8_t brightness;     // 0-9
  uint8_t sleepIndex;     // 0..3 -> 10/15/30/45s
  bool ghostBlocks;       // on/off
  uint8_t ghostCount;     // 2..4
  uint8_t pongSpeed;      // 1..3
  uint8_t minesCount;     // 5..25
};

SettingsData settings;

/*======== Sleep time ===========*/
const uint32_t SLEEP_TIMES_MS[4] = {
  10000,  // 10s
  15000,  // 15s
  30000,  // 30s
  45000   // 45s
};
unsigned long lastInputTime = 0;

bool btn(int p) {
  if (digitalRead(p) == LOW) {
    lastInputTime = millis();
    return true;
  }
  return false;
}

/*======= Keep Alive =======*/
unsigned long lastKick = 0;
unsigned long kickStart = 0;
bool kicking = false;

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

/* ================= Utility Functions ================= */
long loadHighScore(const char* key) {
  prefs.begin("hiscores", true);   // read-only
  long v = prefs.getLong(key, 0);
  prefs.end();
  return v;
}

void saveHighScore(const char* key, long value) {
  prefs.begin("hiscores", false);  // write
  prefs.putLong(key, value);
  prefs.end();
}

void checkAndSaveHigh(long &hi, long current, const char* key) {
  if (current > hi) {
    hi = current;
    saveHighScore(key, hi);
  }
}

void loadSettings() {
  prefs.begin("settings", true);
  settings.brightness  = prefs.getUChar("bright", 5);
  settings.sleepIndex  = prefs.getUChar("sleep", 1);
  settings.ghostBlocks = prefs.getBool("ghosts", true);
  settings.ghostCount  = prefs.getUChar("gcount", 2);
  settings.pongSpeed   = prefs.getUChar("pspeed", 1);
  settings.minesCount  = prefs.getUChar("mines", 10);
  prefs.end();
}

void saveSettings() {
  prefs.begin("settings", false);
  prefs.putUChar("bright", settings.brightness);
  prefs.putUChar("sleep", settings.sleepIndex);
  prefs.putBool("ghosts", settings.ghostBlocks);
  prefs.putUChar("gcount", settings.ghostCount);
  prefs.putUChar("pspeed", settings.pongSpeed);
  prefs.putUChar("mines", settings.minesCount);
  prefs.end();
}

void applyBrightness() {
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(map(settings.brightness, 0, 9, 10, 255));
}

/* ====== Base Game Class ====== */
class Game {
public:
  virtual void init() = 0;
  virtual void update() = 0;
  virtual void draw() = 0;
};

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

/* ====== Menu Class ====== */
class Menu : public Game {
public:
  void init() override {
    selected = 0;
  }

  void update() override {
    if (btn(BTN_UP)) {
      selected = (selected + itemCount - 1) % itemCount;
      delay(150);
    }

    if (btn(BTN_DOWN)) {
      selected = (selected + 1) % itemCount;
      delay(150);
    }

    static bool startLatch = false;

    if ((btn(BTN_A) || btn(BTN_START)) && !startLatch) {
      launchSelected();
      startLatch = true;
    }

    if (!btn(BTN_A) && !btn(BTN_START)) {
      startLatch = false;
    }
  }

  void draw() override {
    display.clearDisplay();

    display.setCursor(28, 2);
    display.print("GAME MENU");

    // Scroll window logic
    int maxVisible = 5;
    int startIndex = 0;
    if (selected >= maxVisible) {
      startIndex = selected - maxVisible + 1;
    }

    // Draw visible menu items
    for (int i = 0; i < maxVisible && (startIndex + i) < itemCount; i++) {
      int idx = startIndex + i;
      display.setCursor(20, 14 + i * 10);
      if (idx == selected) display.print(">");
      else display.print(" ");
      display.print(items[idx]);
    }

    // Draw scrollbar
    int trackTop = 14;
    int trackHeight = 48;
    int thumbHeight = 8;
    int thumbY = trackTop + selected * (trackHeight - thumbHeight) / (itemCount - 1);
    
    // Track line
    display.drawFastVLine(124, trackTop, trackHeight, SSD1306_WHITE);
    // Thumb block
    display.fillRect(122, thumbY, 5, thumbHeight, SSD1306_WHITE);

    display.display();
  }

private:
  int selected;
  static const int itemCount = 10;
  const char* items[itemCount] = {
    "Tetris",
    "Snake",
    "PacMan",
    "Pong",
    "Flappy",
    "Mines",
    "Breakout",
    "Asteroids",
    "2048",
    "Settings"
  };

  void launchSelected();
};

/*============ Pause Menu ============*/
class PauseMenu {
public:
  void open() {
    active = true;
    choice = 0;
    inputLatch = false;
  }

  void close() {
    active = false;
  }

  bool isActive() const {
    return active;
  }

  void update() {
    if (!active) return;

    // Navigation
    if (btn(BTN_UP) && !inputLatch) {
      choice = (choice + itemCount - 1) % itemCount;
      inputLatch = true;
    }

    if (btn(BTN_DOWN) && !inputLatch) {
      choice = (choice + 1) % itemCount;
      inputLatch = true;
    }

    if (!btn(BTN_UP) && !btn(BTN_DOWN)) {
      inputLatch = false;
    }

    // Confirm
    if (btn(BTN_A)) {
      selected = choice;
      confirmed = true;
      active = false;
      delay(180);
    }
  }

  void draw() {
    if (!active) return;

    display.fillRect(18, 16, 92, 36, SSD1306_BLACK);
    display.drawRect(18, 16, 92, 36, SSD1306_WHITE);

    for (int i = 0; i < itemCount; i++) {
      display.setCursor(32, 22 + i * 10);
      display.print(i == choice ? ">" : " ");
      display.print(items[i]);
    }
  }

  bool hasSelection() const {
    return confirmed;
  }

  int getSelection() {
    confirmed = false;
    return selected;
  }

private:
  static const int itemCount = 3;
  const char* items[itemCount] = { "RESUME", "RESTART", "QUIT" };

  bool active = false;
  bool inputLatch = false;
  bool confirmed = false;

  int choice = 0;
  int selected = 0;
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

/* ================= PAC-MAN CLASS ================= */
/* OG-faithful Pac-Man: 25x11 maze, ghost house, tunnels,
   4 ghost personalities, animated mouth, blinking pellets */
class PacMan : public Game {
public:
  void init() override {
    score = 0;
    lives = 3;
    level = 0;
    highScore = loadHighScore("pacman_hi");
    gameOver = false;
    restartLatch = false;
    mouthOpen = true;
    startLevel();
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

    pause.update();
    if (pause.hasSelection()) {
      int s = pause.getSelection();
      if (s == 1) { init(); return; }
      else if (s == 2) {
        currentGame = &menu;
        currentGame->init();
        return;
      }
    }
    if (pause.isActive()) return;

    handleInput();

    // OG scatter/chase mode cycling
    updateModeTimer();

    // Ghost house exit logic
    releaseGhosts();

    // Frightened timer expiry
    if (frightTimer > 0 && millis() > frightTimer) {
      frightTimer = 0;
      int gc = settings.ghostCount;
      for (int i = 0; i < gc; i++) {
        if (gstate[i] == GS_FRIGHT) gstate[i] = GS_ACTIVE;
      }
    }

    // Move pac-man
    if (millis() - lastMove >= 150) {
      lastMove = millis();
      movePacman();
      checkGhostCollisions();
    }

    // Move ghosts (speed increases with level)
    unsigned long ghostDelay = 200 - level * 15;
    if (ghostDelay < 100) ghostDelay = 100;
    if (millis() - lastGhostMove >= ghostDelay) {
      lastGhostMove = millis();
      moveAllGhosts();
      checkGhostCollisions();
    }

    // Level complete — all dots eaten
    if (dotsLeft <= 0) {
      level++;
      display.clearDisplay();
      display.setCursor(36, 20);
      display.print("LEVEL ");
      display.print(level + 1);
      display.setCursor(34, 36);
      display.print("GET READY");
      display.display();
      delay(1200);
      startLevel();
    }
  }

  void draw() override {
    // --- Camera Logic ---
    int camX = (px * PM_TILE) - (128 / 2);
    int camY = (py * PM_TILE) - ((64 - PM_HUD) / 2);

    // Clamp camera
    if (camX < 0) camX = 0;
    if (camX > (PM_W * PM_TILE) - 128) camX = (PM_W * PM_TILE) - 128;
    if (camY < 0) camY = 0;
    if (camY > (PM_H * PM_TILE) - (64 - PM_HUD)) camY = (PM_H * PM_TILE) - (64 - PM_HUD);

    display.clearDisplay();
    drawMaze(camX, camY);

    // Draw ghosts
    int gc = settings.ghostCount;
    for (int i = 0; i < gc; i++)
      drawOneGhost(i, camX, camY);

    drawPacman(camX, camY);
    drawHUD();

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
  uint8_t maze[PM_H][PM_W];
  int px, py, dir, nextDir;
  int gx[PM_MAX_GHOSTS], gy[PM_MAX_GHOSTS], gdir[PM_MAX_GHOSTS];
  uint8_t gstate[PM_MAX_GHOSTS];
  long score, highScore;
  int lives, level, dotsLeft, dotsEaten, ghostsEaten;
  bool gameOver, restartLatch, mouthOpen;
  bool scatterMode;
  int modePhase;
  unsigned long modeTimer, frightTimer, lastMove, lastGhostMove;
  PauseMenu pause;

  /* ---------- SETUP ---------- */
  void startLevel() {
    loadMaze();
    countDots();
    dotsEaten = 0;
    ghostsEaten = 0;
    frightTimer = 0;
    scatterMode = true;
    modePhase = 0;
    modeTimer = millis();
    px = 14; py = 23;
    dir = DIR_LEFT;
    nextDir = DIR_LEFT;
    mouthOpen = true;
    resetGhosts();
    lastMove = millis();
    lastGhostMove = millis();
  }

  void resetGhosts() {
    // OG: Blinky outside house, others inside
    const int spX[] = {14, 14, 12, 16};
    const int spY[] = {11, 14, 14, 14};
    const int spD[] = {DIR_LEFT, DIR_DOWN, DIR_UP, DIR_UP};
    int gc = settings.ghostCount;
    for (int i = 0; i < gc; i++) {
      gx[i] = spX[i];
      gy[i] = spY[i];
      gdir[i] = spD[i];
      gstate[i] = (i == 0) ? GS_ACTIVE : GS_HOUSE;
    }
  }

  void countDots() {
    dotsLeft = 0;
    for (int y = 0; y < PM_H; y++)
      for (int x = 0; x < PM_W; x++)
        if (maze[y][x] == PM_DOT || maze[y][x] == PM_POWER)
          dotsLeft++;
  }

  /* ---------- INPUT ---------- */
  void handleInput() {
    if (btn(BTN_UP))    nextDir = DIR_UP;
    if (btn(BTN_DOWN))  nextDir = DIR_DOWN;
    if (btn(BTN_LEFT))  nextDir = DIR_LEFT;
    if (btn(BTN_RIGHT)) nextDir = DIR_RIGHT;
  }

  /* ---------- MOVEMENT HELPERS ---------- */
  void stepDir(int &x, int &y, int d) {
    if (d == DIR_UP)    y--;
    if (d == DIR_DOWN)  y++;
    if (d == DIR_LEFT)  x--;
    if (d == DIR_RIGHT) x++;
  }

  bool canMoveDir(int x, int y, int d) {
    stepDir(x, y, d);
    // Tunnel wrap-around
    if (y == PM_TUNNEL_ROW) {
      if (x < 0) x = PM_W - 1;
      else if (x >= PM_W) x = 0;
    }
    if (x < 0 || x >= PM_W || y < 0 || y >= PM_H) return false;
    return maze[y][x] != PM_WALL;
  }

  /* ---------- PAC-MAN LOGIC ---------- */
  void movePacman() {
    if (canMoveDir(px, py, nextDir)) dir = nextDir;
    if (!canMoveDir(px, py, dir)) return;
    stepDir(px, py, dir);

    // Tunnel wrap-around
    if (py == PM_TUNNEL_ROW) {
      if (px < 0) px = PM_W - 1;
      else if (px >= PM_W) px = 0;
    }

    // Animate mouth
    mouthOpen = !mouthOpen;

    if (maze[py][px] == PM_DOT) {
      maze[py][px] = PM_EMPTY;
      score += 10;
      dotsLeft--;
      dotsEaten++;
    }
    if (maze[py][px] == PM_POWER) {
      maze[py][px] = PM_EMPTY;
      score += 50;
      dotsLeft--;
      dotsEaten++;
      frightTimer = millis() + 7000;
      ghostsEaten = 0;
      int gc = settings.ghostCount;
      for (int i = 0; i < gc; i++) {
        if (gstate[i] == GS_ACTIVE) {
          gstate[i] = GS_FRIGHT;
          gdir[i] ^= 1; // OG: reverse direction on fright
        }
      }
    }
  }

  /* ---------- GHOST AI (OG-faithful) ---------- */

  // OG scatter/chase mode schedule
  void updateModeTimer() {
    if (modePhase >= 7) return; // Phase 7 = chase forever
    static const unsigned long durations[] = {7000,20000,7000,20000,5000,20000,5000};
    if (millis() - modeTimer >= durations[modePhase]) {
      modePhase++;
      scatterMode = (modePhase % 2 == 0) && (modePhase < 7);
      modeTimer = millis();
      // OG: all active ghosts reverse direction on mode switch
      int gc = settings.ghostCount;
      for (int i = 0; i < gc; i++) {
        if (gstate[i] == GS_ACTIVE) gdir[i] ^= 1;
      }
    }
  }

  // Release ghosts from house based on dot counters (OG thresholds)
  void releaseGhosts() {
    const int dotThresh[] = {0, 0, 30, 60};
    int gc = settings.ghostCount;
    for (int i = 0; i < gc; i++) {
      if (gstate[i] != GS_HOUSE) continue;
      if (dotsEaten >= dotThresh[i]) {
        gx[i] = 14; gy[i] = 11; // Teleport above gate
        gdir[i] = DIR_LEFT;
        gstate[i] = (frightTimer > 0 && millis() < frightTimer) ? GS_FRIGHT : GS_ACTIVE;
      }
    }
  }

  // Ghost house gate is blocked for non-eaten/non-house ghosts
  bool isGhostBlocked(int x, int y, int idx) {
    if (x < 0 || x >= PM_W || y < 0 || y >= PM_H) return true;
    if (maze[y][x] == PM_WALL) return true;
    if (gstate[idx] != GS_EATEN && gstate[idx] != GS_HOUSE) {
      if (y == 12 && (x == 13 || x == 14)) return true;
    }
    return false;
  }

  void moveAllGhosts() {
    int gc = settings.ghostCount;
    for (int i = 0; i < gc; i++) {
      if (gstate[i] == GS_HOUSE) continue;
      moveOneGhost(i);
      if (gstate[i] == GS_EATEN) moveOneGhost(i); // Eaten = double speed
    }
  }

  // OG scatter corner targets
  void getScatterTarget(int idx, int &tx, int &ty) {
    switch (idx % 4) {
      case 0: tx = PM_W-3; ty = 0;      break; // Blinky: top-right
      case 1: tx = 2;      ty = 0;      break; // Pinky: top-left
      case 2: tx = PM_W-1; ty = PM_H-1; break; // Inky: bottom-right
      case 3: tx = 0;      ty = PM_H-1; break; // Clyde: bottom-left
    }
  }

  // OG chase targeting per ghost personality
  void getChaseTarget(int idx, int &tx, int &ty) {
    switch (idx % 4) {
      case 0: // BLINKY — direct chase
        tx = px; ty = py;
        break;
      case 1: // PINKY — 4 tiles ahead (OG overflow bug when facing UP)
        tx = px; ty = py;
        if (dir == DIR_UP)    { ty -= 4; tx -= 4; }
        else if (dir == DIR_DOWN)  ty += 4;
        else if (dir == DIR_LEFT)  tx -= 4;
        else if (dir == DIR_RIGHT) tx += 4;
        if (tx < 0) tx = 0;
        if (tx >= PM_W) tx = PM_W - 1;
        if (ty < 0) ty = 0;
        if (ty >= PM_H) ty = PM_H - 1;
        break;
      case 2: // INKY — double vector from Blinky to 2 ahead of Pac-Man
        {
          int ax = px, ay = py;
          if (dir == DIR_UP) { ay -= 2; ax -= 2; } // OG overflow bug
          else if (dir == DIR_DOWN) ay += 2;
          else if (dir == DIR_LEFT) ax -= 2;
          else if (dir == DIR_RIGHT) ax += 2;
          tx = ax + (ax - gx[0]);
          ty = ay + (ay - gy[0]);
          if (tx < 0) tx = 0;
          if (tx >= PM_W) tx = PM_W - 1;
          if (ty < 0) ty = 0;
          if (ty >= PM_H) ty = PM_H - 1;
        }
        break;
      case 3: // CLYDE — chase if far, scatter if within 8 tiles
        {
          int dx = gx[idx] - px;
          int dy = gy[idx] - py;
          if (dx*dx + dy*dy > 64) { // OG uses squared Euclidean
            tx = px; ty = py;
          } else {
            getScatterTarget(idx, tx, ty);
          }
        }
        break;
    }
  }

  void moveOneGhost(int idx) {
    int targetX, targetY;

    if (gstate[idx] == GS_EATEN) {
      // Eyes return to ghost house
      targetX = 13; targetY = 11;
      if (gx[idx] >= 12 && gx[idx] <= 15 && gy[idx] == 11) {
        // Arrived at house — respawn
        gx[idx] = 14; gy[idx] = 14;
        gstate[idx] = GS_ACTIVE;
        gdir[idx] = DIR_UP;
        return;
      }
    } else if (gstate[idx] == GS_FRIGHT) {
      // OG: frightened ghosts pick a random valid direction
      const int dirOrder[] = {DIR_UP, DIR_LEFT, DIR_DOWN, DIR_RIGHT};
      int validDirs[4];
      int validCount = 0;
      for (int i = 0; i < 4; i++) {
        int d = dirOrder[i];
        if (d == (gdir[idx] ^ 1)) continue;
        int nx = gx[idx], ny = gy[idx];
        stepDir(nx, ny, d);
        if (ny == PM_TUNNEL_ROW) {
          if (nx < 0) nx = PM_W - 1;
          else if (nx >= PM_W) nx = 0;
        }
        if (isGhostBlocked(nx, ny, idx)) continue;
        validDirs[validCount++] = d;
      }
      if (validCount > 0) {
        int chosen = validDirs[random(validCount)];
        int nx = gx[idx], ny = gy[idx];
        stepDir(nx, ny, chosen);
        if (ny == PM_TUNNEL_ROW) {
          if (nx < 0) nx = PM_W - 1;
          else if (nx >= PM_W) nx = 0;
        }
        gx[idx] = nx; gy[idx] = ny;
        gdir[idx] = chosen;
      } else {
        // Fallback: reverse
        int d = gdir[idx] ^ 1;
        int nx = gx[idx], ny = gy[idx];
        stepDir(nx, ny, d);
        if (ny == PM_TUNNEL_ROW) {
          if (nx < 0) nx = PM_W - 1;
          else if (nx >= PM_W) nx = 0;
        }
        if (!isGhostBlocked(nx, ny, idx)) {
          gx[idx] = nx; gy[idx] = ny;
          gdir[idx] = d;
        }
      }
      return;
    } else {
      // Active ghost: scatter or chase mode
      if (scatterMode) getScatterTarget(idx, targetX, targetY);
      else getChaseTarget(idx, targetX, targetY);
    }

    // OG direction priority: UP > LEFT > DOWN > RIGHT
    const int dirOrder[] = {DIR_UP, DIR_LEFT, DIR_DOWN, DIR_RIGHT};
    int bestDir = -1;
    long bestDist = 999999;
    int fallbackDir = -1;

    for (int i = 0; i < 4; i++) {
      int d = dirOrder[i];
      int nx = gx[idx], ny = gy[idx];
      stepDir(nx, ny, d);
      if (ny == PM_TUNNEL_ROW) {
        if (nx < 0) nx = PM_W - 1;
        else if (nx >= PM_W) nx = 0;
      }
      if (isGhostBlocked(nx, ny, idx)) continue;
      if (d == (gdir[idx] ^ 1)) { fallbackDir = d; continue; } // No 180
      long dx2 = (long)(nx - targetX);
      long dy2 = (long)(ny - targetY);
      long dist = dx2*dx2 + dy2*dy2; // OG: squared Euclidean
      if (dist < bestDist) {
        bestDist = dist;
        bestDir = d;
      }
    }

    if (bestDir < 0) bestDir = fallbackDir;
    if (bestDir < 0) return;

    int nx = gx[idx], ny = gy[idx];
    stepDir(nx, ny, bestDir);
    if (ny == PM_TUNNEL_ROW) {
      if (nx < 0) nx = PM_W - 1;
      else if (nx >= PM_W) nx = 0;
    }
    gx[idx] = nx;
    gy[idx] = ny;
    gdir[idx] = bestDir;
  }

  /* ---------- COLLISION ---------- */
  void checkGhostCollisions() {
    if (gameOver) return;
    int gc = settings.ghostCount;
    for (int i = 0; i < gc; i++) {
      if (gstate[i] == GS_HOUSE || gstate[i] == GS_EATEN) continue;
      if (gx[i] == px && gy[i] == py) {
        if (gstate[i] == GS_FRIGHT) {
          // OG: 200, 400, 800, 1600 for consecutive eats
          ghostsEaten++;
          int bonus = 200;
          for (int j = 1; j < ghostsEaten; j++) bonus *= 2;
          score += bonus;
          gstate[i] = GS_EATEN;
        } else {
          lives--;
          if (lives <= 0) {
            gameOver = true;
            checkAndSaveHigh(highScore, score, "pacman_hi");
          } else {
            px = 14; py = 23;
            dir = DIR_LEFT;
            nextDir = DIR_LEFT;
            resetGhosts();
            delay(500);
          }
          return;
        }
      }
    }
  }

  /* ---------- DRAW ---------- */
  void drawHUD() {
    // Score
    display.setCursor(0, 0);
    display.print("S:");
    display.print(score);

    // Lives as pac-man icons
    int lx = 52;
    for (int i = 0; i < lives - 1 && i < 4; i++) {
      display.fillRect(lx + i * 6, 0, 3, 3, SSD1306_WHITE);
      display.drawPixel(lx + i * 6 + 2, 1, SSD1306_BLACK); // mouth
    }

    // High score
    display.setCursor(86, 0);
    display.print("H:");
    display.print(highScore);
  }

  void drawMaze(int camX, int camY) {
    bool pelletBlink = (millis() / 200) % 2 == 0;  // blink power pellets

    for (int y = 0; y < PM_H; y++) {
      for (int x = 0; x < PM_W; x++) {
        int sx = PM_OX + x * PM_TILE - camX;
        int sy = PM_HUD + y * PM_TILE - camY;

        // Cull off-screen tiles
        if (sx < -PM_TILE || sx >= 128 || sy < PM_HUD || sy >= 64) continue;

        if (maze[y][x] == PM_WALL)
          display.drawRect(sx, sy, PM_TILE, PM_TILE, SSD1306_WHITE);
        else if (maze[y][x] == PM_DOT)
          display.drawPixel(sx + PM_TILE / 2, sy + PM_TILE / 2, SSD1306_WHITE);
        else if (maze[y][x] == PM_POWER && pelletBlink)
          display.fillRect(sx + PM_TILE / 2 - 1, sy + PM_TILE / 2 - 1, 2, 2, SSD1306_WHITE);
      }
    }
  }

  void drawPacman(int camX, int camY) {
    int sx = PM_OX + px * PM_TILE + 1 - camX;
    int sy = PM_HUD + py * PM_TILE + 1 - camY;
    int w = PM_TILE - 2;  // 3px for 5px tiles

    display.fillRect(sx, sy, w, w, SSD1306_WHITE);

    // Animated mouth (open on even steps)
    if (mouthOpen) {
      if (dir == DIR_RIGHT)     display.drawPixel(sx + w - 1, sy + w / 2, SSD1306_BLACK);
      else if (dir == DIR_LEFT) display.drawPixel(sx, sy + w / 2, SSD1306_BLACK);
      else if (dir == DIR_UP)   display.drawPixel(sx + w / 2, sy, SSD1306_BLACK);
      else if (dir == DIR_DOWN) display.drawPixel(sx + w / 2, sy + w - 1, SSD1306_BLACK);
    }
  }

  void drawOneGhost(int idx, int camX, int camY) {
    if (gstate[idx] == GS_HOUSE) return; // Don't draw house ghosts
    int sx = PM_OX + gx[idx] * PM_TILE + 1 - camX;
    int sy = PM_HUD + gy[idx] * PM_TILE + 1 - camY;
    int w = PM_TILE - 2;  // 3px

    // Cull off-screen
    if (sx < -PM_TILE || sx >= 128 || sy < PM_HUD || sy >= 64) return;

    if (gstate[idx] == GS_EATEN) {
      // Eyes only — two dots returning to house
      display.drawPixel(sx, sy, SSD1306_WHITE);
      display.drawPixel(sx + w - 1, sy, SSD1306_WHITE);
    } else if (gstate[idx] == GS_FRIGHT) {
      // Frightened: outline only, blinks near timeout
      bool show = true;
      if (frightTimer > 0 && millis() < frightTimer && frightTimer - millis() < 2000)
        show = (millis() / 150) % 2 == 0;
      if (show)
        display.drawRect(sx, sy, w, w, SSD1306_WHITE);
    } else {
      // Normal ghost body
      display.fillRect(sx, sy, w, w - 1, SSD1306_WHITE);
      // Wavy feet — alternate pattern per ghost
      if (idx % 2 == 0) {
        display.drawPixel(sx, sy + w - 1, SSD1306_WHITE);
        display.drawPixel(sx + 2, sy + w - 1, SSD1306_WHITE);
      } else {
        display.drawPixel(sx + 1, sy + w - 1, SSD1306_WHITE);
        if (w > 2) display.drawPixel(sx + w - 1, sy + w - 1, SSD1306_WHITE);
      }
      // Eyes
      display.drawPixel(sx, sy, SSD1306_BLACK);
      display.drawPixel(sx + w - 1, sy, SSD1306_BLACK);
    }
  }

  /* ---------- STATIC MAZE ---------- */
  void loadMaze() {
    static const uint8_t classic_map[PM_H][PM_W] = {
      {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
      {1,2,2,2,2,2,2,2,2,2,2,2,2,1,1,2,2,2,2,2,2,2,2,2,2,2,2,1},
      {1,2,1,1,1,1,2,1,1,1,1,1,2,1,1,2,1,1,1,1,1,2,1,1,1,1,2,1},
      {1,3,1,1,1,1,2,1,1,1,1,1,2,1,1,2,1,1,1,1,1,2,1,1,1,1,3,1},
      {1,2,1,1,1,1,2,1,1,1,1,1,2,1,1,2,1,1,1,1,1,2,1,1,1,1,2,1},
      {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
      {1,2,1,1,1,1,2,1,1,2,1,1,1,1,1,1,1,1,2,1,1,2,1,1,1,1,2,1},
      {1,2,1,1,1,1,2,1,1,2,1,1,1,1,1,1,1,1,2,1,1,2,1,1,1,1,2,1},
      {1,2,2,2,2,2,2,1,1,2,2,2,2,1,1,2,2,2,2,1,1,2,2,2,2,2,2,1},
      {1,1,1,1,1,1,2,1,1,1,1,1,0,1,1,0,1,1,1,1,1,2,1,1,1,1,1,1},
      {0,0,0,0,1,1,2,1,1,1,1,1,0,1,1,0,1,1,1,1,1,2,1,1,0,0,0,0},
      {0,0,0,0,1,1,2,1,1,0,0,0,0,0,0,0,0,0,0,1,1,2,1,1,0,0,0,0},
      {0,0,0,0,1,1,2,1,1,0,1,1,1,0,0,1,1,1,0,1,1,2,1,1,0,0,0,0},
      {1,1,1,1,1,1,2,1,1,0,1,0,0,0,0,0,0,1,0,1,1,2,1,1,1,1,1,1},
      {0,0,0,0,0,0,2,0,0,0,1,0,0,0,0,0,0,1,0,0,0,2,0,0,0,0,0,0},
      {1,1,1,1,1,1,2,1,1,0,1,0,0,0,0,0,0,1,0,1,1,2,1,1,1,1,1,1},
      {0,0,0,0,1,1,2,1,1,0,1,1,1,1,1,1,1,1,0,1,1,2,1,1,0,0,0,0},
      {0,0,0,0,1,1,2,1,1,0,0,0,0,0,0,0,0,0,0,1,1,2,1,1,0,0,0,0},
      {0,0,0,0,1,1,2,1,1,0,1,1,1,1,1,1,1,1,0,1,1,2,1,1,0,0,0,0},
      {1,1,1,1,1,1,2,1,1,0,1,1,1,1,1,1,1,1,0,1,1,2,1,1,1,1,1,1},
      {1,2,2,2,2,2,2,2,2,2,2,2,2,1,1,2,2,2,2,2,2,2,2,2,2,2,2,1},
      {1,2,1,1,1,1,2,1,1,1,1,1,2,1,1,2,1,1,1,1,1,2,1,1,1,1,2,1},
      {1,2,1,1,1,1,2,1,1,1,1,1,2,1,1,2,1,1,1,1,1,2,1,1,1,1,2,1},
      {1,3,2,2,1,1,2,2,2,2,2,2,2,0,0,2,2,2,2,2,2,2,1,1,2,2,3,1},
      {1,1,1,2,1,1,2,1,1,2,1,1,1,1,1,1,1,1,2,1,1,2,1,1,2,1,1,1},
      {1,1,1,2,1,1,2,1,1,2,1,1,1,1,1,1,1,1,2,1,1,2,1,1,2,1,1,1},
      {1,2,2,2,2,2,2,1,1,2,2,2,2,1,1,2,2,2,2,1,1,2,2,2,2,2,2,1},
      {1,2,1,1,1,1,1,1,1,1,1,1,2,1,1,2,1,1,1,1,1,1,1,1,1,1,2,1},
      {1,2,1,1,1,1,1,1,1,1,1,1,2,1,1,2,1,1,1,1,1,1,1,1,1,1,2,1},
      {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
      {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
    };
    memcpy(maze, classic_map, sizeof(maze));
  }
};

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

/* ================= Settings CLASS ================= */

class Settings : public Game {
public:
  void init() override {
    selected = 0;
    confirmReset = false;
  }

  void update() override {
    if (btn(BTN_UP)) {
      selected = (selected + itemCount - 1) % itemCount;
      delay(150);
    }

    if (btn(BTN_DOWN)) {
      selected = (selected + 1) % itemCount;
      delay(150);
    }

    if (btn(BTN_A)) {
      handleAction();
      delay(180);
    }

    if (btn(BTN_B) || btn(BTN_START)) {
      saveSettings();
      applyBrightness();
      currentGame = &menu;
      currentGame->init();
    }
  }

  void draw() override {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("SETTINGS");

    int maxVisible = 5;
    int startIndex = 0;
    if (selected >= maxVisible) {
      startIndex = selected - maxVisible + 1;
    }

    for (int i = 0; i < maxVisible && (startIndex + i) < itemCount; i++) {
      int idx = startIndex + i;
      display.setCursor(0, 12 + i * 9);
      display.print(idx == selected ? ">" : " ");
      drawItem(idx);
    }

    display.display();
  }

private:
  int selected;
  bool confirmReset;

  static const int itemCount = 7;
  const char* sleepText[4] = { "10s", "15s", "30s", "45s" };

  void drawItem(int i) {
    switch (i) {
      case 0:
        display.print("Brightness: ");
        display.print(settings.brightness + 1);
        break;
      case 1:
        display.print("Reset HI: ");
        display.print(confirmReset ? "A=YES" : "NO");
        break;
      case 2:
        display.print("Sleep: ");
        display.print(sleepText[settings.sleepIndex]);
        break;
      case 3:
        display.print("Ghost Blocks: ");
        display.print(settings.ghostBlocks ? "YES" : "NO");
        break;
      case 4:
        display.print("Ghost Count: ");
        display.print(settings.ghostCount);
        break;
      case 5:
        display.print("Pong Speed: ");
        display.print(settings.pongSpeed);
        break;
      case 6:
        display.print("Mines Count: ");
        display.print(settings.minesCount);
        break;
    }
  }

  void handleAction() {
    switch (selected) {
      case 0:
        settings.brightness = (settings.brightness + 1) % 10;
        applyBrightness();
        break;

      case 1:
        if (!confirmReset) confirmReset = true;
        else {
          prefs.begin("hiscores", false);
          prefs.clear();
          prefs.end();
          confirmReset = false;
        }
        break;

      case 2:
        settings.sleepIndex = (settings.sleepIndex + 1) % 4;
        break;

      case 3:
        settings.ghostBlocks = !settings.ghostBlocks;
        break;

      case 4:
        settings.ghostCount++;
        if (settings.ghostCount > 4) settings.ghostCount = 2;
        break;

      case 5:
        settings.pongSpeed++;
        if (settings.pongSpeed > 3) settings.pongSpeed = 1;
        break;

      case 6:
        settings.minesCount += 2;
        if (settings.minesCount > 25) settings.minesCount = 5;
        break;
    }
  }
};

Settings settingsGame;

/* ================= MAIN ================= */

Game* currentGame;
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
PauseMenu pauseMenu;

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