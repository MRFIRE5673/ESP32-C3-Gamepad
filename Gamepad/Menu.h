#pragma once
#include "Config.h"
#include "Game.h"

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


extern Menu menu;
