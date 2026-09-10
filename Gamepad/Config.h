#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include "esp_sleep.h"
#include "driver/gpio.h"

/* ================= DISPLAY ================= */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

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

/* ================= SETTINGS DATA ================= */
struct SettingsData {
  uint8_t brightness;     // 0-9
  uint8_t sleepIndex;     // 0..3 -> 10/15/30/45s
  bool ghostBlocks;       // on/off
  uint8_t ghostCount;     // 2..4
  uint8_t pongSpeed;      // 1..3
  uint8_t minesCount;     // 5..25
};

extern Adafruit_SSD1306 display;
extern Preferences prefs;
extern SettingsData settings;
extern const uint32_t SLEEP_TIMES_MS[4];
extern unsigned long lastInputTime;

extern unsigned long lastKick;
extern unsigned long kickStart;
extern bool kicking;

inline bool btn(int p) {
  if (digitalRead(p) == LOW) {
    lastInputTime = millis();
    return true;
  }
  return false;
}

inline long loadHighScore(const char* key) {
  prefs.begin("hiscores", true);   // read-only
  long v = prefs.getLong(key, 0);
  prefs.end();
  return v;
}

inline void saveHighScore(const char* key, long value) {
  prefs.begin("hiscores", false);  // write
  prefs.putLong(key, value);
  prefs.end();
}

inline void checkAndSaveHigh(long &hi, long current, const char* key) {
  if (current > hi) {
    hi = current;
    saveHighScore(key, hi);
  }
}

inline void loadSettings() {
  prefs.begin("settings", true);
  settings.brightness  = prefs.getUChar("bright", 5);
  settings.sleepIndex  = prefs.getUChar("sleep", 1);
  settings.ghostBlocks = prefs.getBool("ghosts", true);
  settings.ghostCount  = prefs.getUChar("gcount", 2);
  settings.pongSpeed   = prefs.getUChar("pspeed", 1);
  settings.minesCount  = prefs.getUChar("mines", 10);
  prefs.end();
}

inline void saveSettings() {
  prefs.begin("settings", false);
  prefs.putUChar("bright", settings.brightness);
  prefs.putUChar("sleep", settings.sleepIndex);
  prefs.putBool("ghosts", settings.ghostBlocks);
  prefs.putUChar("gcount", settings.ghostCount);
  prefs.putUChar("pspeed", settings.pongSpeed);
  prefs.putUChar("mines", settings.minesCount);
  prefs.end();
}

inline void applyBrightness() {
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(map(settings.brightness, 0, 9, 10, 255));
}

void bootAnimation();
