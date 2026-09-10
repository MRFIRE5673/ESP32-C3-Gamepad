#pragma once
#include "Config.h"
#include "Game.h"
#include "Menu.h"
#include "PauseMenu.h"

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

