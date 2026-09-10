# 🎮 ESP32-C3 SuperMini Multi-Game Handheld Console

An ultra-compact, low-power handheld retro console framework built specifically around the **ESP32-C3 SuperMini** form factor and a 0.96" or 1.3" 128x64 SSD1306 OLED display. Features 9 classic retro games, persistent high scores via NVS, low-power automatic light sleep, and an anti-powerbank shutoff kick mechanism.

## ✨ Project Features
* **9 Playable Retro Games:** Tetris (with optional ghost block preview), Snake, Pac-Man (OG-faithful chase/scatter ghost personalities), Pong, Flappy Bird, Minesweeper, Breakout, Asteroids, and 2048.
* **Persistent NVS Preferences:** Local saving of hardware configuration (brightness, sleep intervals, ghost settings) and absolute high scores via `Preferences.h`.
* **Smart Sleep & GPIO Wakeup:** Automatically drops into a `esp_light_sleep_start()` state upon user inactivity timeout. Wakes up immediately upon pressing any of the 8 control buttons.
* **Hardware Power-Bank Keep-Alive:** Pulses `KEEP_ALIVE_PIN 5` low for 150ms every 20 seconds using an internal float-to-output toggle matrix. This prevents smart power banks or lipo chargers from dropping into sleep mode due to low current draw.

---

## 🛠️ Hardware & Pin Configuration (ESP32-C3 SuperMini)

Due to the limited and packed pin layout of the SuperMini board, the system uses the following configuration:

| Control Target | SuperMini GPIO Pin | Notes |
| :--- | :---: | :--- |
| **I2C SDA** | `GPIO 8` | Standard Hardware I2C Data |
| **I2C SCL** | `GPIO 9` | Standard Hardware I2C Clock |
| **Button UP** | `GPIO 0` | Configured as `INPUT_PULLUP` |
| **Button RIGHT**| `GPIO 1` | Configured as `INPUT_PULLUP` |
| **Button LEFT** | `GPIO 2` | Configured as `INPUT_PULLUP` |
| **Button DOWN** | `GPIO 3` | Configured as `INPUT_PULLUP` |
| **Button A**    | `GPIO 10`| Configured as `INPUT_PULLUP` |
| **Button B**    | `GPIO 7` | Configured as `INPUT_PULLUP` |
| **Button START**| `GPIO 21`| Configured as `INPUT_PULLUP` |
| **Button SELECT**| `GPIO 20`| Configured as `INPUT_PULLUP` |
| **Keep-Alive**  | `GPIO 5` | Tied to external dummy load resistor network |

---

## 🚀 Flashing & Arduino IDE Settings

To program the **ESP32-C3 SuperMini**, use the following configuration under **Tools** in your Arduino IDE:

1. **Board:** Select `ESP32C3 Dev Module` (or `LOLIN ESP32-C3 Pico` depending on your core package).
2. **USB CDC On Boot:** `Enabled` *(Crucial for the SuperMini to maintain serial output and flashing capabilities over its integrated Type-C port)*.
3. **Partition Scheme:** `Default 4MB with spiffs` (The codebase heavily uses internal NVS preferences for game saving).
4. **Required Dependencies:**
   * `Adafruit_SSD1306` (OLED driver library)
   * `Adafruit_GFX_Library` (Core graphics library)

---

## 💾 Installation Setup
1. Clone this repository:
   ```bash
   git clone https://github.com
   ```
2. Move into the directory and open `HandheldConsole/HandheldConsole.ino` with your Arduino IDE.
3. Select your ESP32-C3 target port and upload!
