#pragma once
#include "Config.h"

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

