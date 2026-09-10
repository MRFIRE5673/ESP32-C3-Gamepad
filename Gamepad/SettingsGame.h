#pragma once
#include "Config.h"
#include "Game.h"
#include "Menu.h"
#include "PauseMenu.h"

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

