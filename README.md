# ESP32 Rain Sensor Dashboard

Real-time rain sensor monitoring with an ESP32, I2C LCD, two LEDs, push button, and a standalone web dashboard.

---

## Hardware

### Components
| Part | Details |
|------|---------|
| ESP32 DevKit | Any 38-pin variant |
| Rain sensor module | With both AO (analog) and DO (digital) outputs |
| I2C LCD 16x2 | Address `0x27` (most common) |
| Red LED | 5mm or SMD, any standard LED |
| Blue LED | 5mm or SMD, any standard LED |
| 2x 220 Ohm resistors | Current limiting for LEDs |
| 4-pin micro push switch | Momentary, normally open |

### Pin Wiring
| Component | ESP32 Pin | Notes |
|-----------|-----------|-------|
| Rain DO | GPIO 34 | Digital rain threshold (input-only pin) |
| Rain AO | GPIO 35 | Analog rain voltage (input-only pin) |
| LCD SDA | GPIO 21 | I2C data |
| LCD SCL | GPIO 22 | I2C clock |
| Red LED | GPIO 25 | Anode → 220 Ohm → GPIO 25 · Cathode → GND |
| Blue LED | GPIO 26 | Anode → 220 Ohm → GPIO 26 · Cathode → GND |
| Push Button | GPIO 27 | One pair to GPIO 27, other pair to GND · Uses INPUT_PULLUP |

> GPIO 34 and 35 are input-only on ESP32 — correct for sensor use.

### LED Wiring Detail
```
GPIO 25 ──[220Ω]──┤RED LED├── GND
GPIO 26 ──[220Ω]──┤BLUE LED├── GND
```

### Button Wiring Detail
```
GPIO 27 ──┤SW pin 1├──┤SW pin 2├── GND
(INPUT_PULLUP pulls GPIO 27 HIGH when idle; press pulls it LOW)
```

---

## Firmware (LCD_01.ino)

### Required Libraries
Install via Arduino Library Manager or PlatformIO:

| Library | Author |
|---------|--------|
| `LiquidCrystal_I2C` | Frank de Brabander |
| `ArduinoJson` | Benoit Blanchon (v6 or v7) |
| `WiFi` | Built-in (ESP32 Arduino core) |
| `WebServer` | Built-in (ESP32 Arduino core) |

### Configuration
Open `LCD_01.ino` and set your WiFi credentials at the top:

```cpp
#define WIFI_SSID "YOUR_SSID_HERE"
#define WIFI_PASS "YOUR_PASSWORD_HERE"
```

### Flash Steps (Arduino IDE)
1. Board: **ESP32 Dev Module** (Tools > Board)
2. Port: select your COM/tty port
3. Upload speed: 115200 (or higher if stable)
4. Click **Upload**
5. Open Serial Monitor at 115200 baud to see the IP address

### Flash Steps (PlatformIO)
```bash
pio run --target upload
pio device monitor --baud 115200
```

---

## Behavior

### Modes
The push button (GPIO 27) toggles between two modes:

| Mode | LED Behavior | Dashboard Toggles |
|------|-------------|-------------------|
| **Auto** | LEDs follow sensor logic (see below) | Disabled (greyed out) |
| **Manual** | LEDs controlled from dashboard | Enabled |

The LCD briefly shows `Mode: Auto` or `Mode: Manual` for 2 seconds when the button is pressed.

### Auto Mode LED Logic
| Condition | Red LED | Blue LED |
|-----------|---------|----------|
| Rain detected (`rain_percent >= 40` OR `digital_val == 0`) | ON | — |
| No rain | OFF | — |
| WiFi connected | — | ON |
| WiFi disconnected / connecting | — | OFF / blink |

### Button Debounce
50 ms hardware debounce prevents false triggers from contact bounce.

---

## HTTP API

Once connected to WiFi the ESP32 exposes:

| Endpoint | Method | Response |
|----------|--------|----------|
| `GET /` | HTTP 302 | Redirects to `/data` |
| `GET /data` | JSON | Sensor readings + LED state + mode |
| `GET /led?red=0|1&blue=0|1` | JSON | Set LED state (Manual Mode only) |
| `OPTIONS /data` | 204 | CORS preflight |
| `OPTIONS /led` | 204 | CORS preflight |

### `GET /data` response
```json
{
  "ip":           "192.168.1.42",
  "rain_percent": 45,
  "analog_val":   2200,
  "digital_val":  0,
  "status":       "Moderate",
  "uptime":       12345,
  "led_red":      true,
  "led_blue":     false,
  "mode":         "auto"
}
```

| Field | Description |
|-------|-------------|
| `rain_percent` | 0–100 %, mapped from analog reading |
| `analog_val` | Raw ADC value 0–4095 |
| `digital_val` | DO pin: `0` = rain detected, `1` = dry |
| `status` | `Dry` / `Light Rain` / `Moderate` / `Heavy Rain!` |
| `uptime` | Seconds since boot |
| `led_red` | Current red LED state (`true` = ON) |
| `led_blue` | Current blue LED state (`true` = ON) |
| `mode` | `"auto"` or `"manual"` |

### `GET /led` response
```json
{
  "led_red":  true,
  "led_blue": false,
  "mode":     "manual",
  "accepted": true
}
```

`accepted` is `false` when called while in Auto Mode (command is ignored, current state is returned).

### Status Thresholds
| Rain % | Status |
|--------|--------|
| < 10 | Dry |
| 10–39 | Light Rain |
| 40–69 | Moderate |
| >= 70 | Heavy Rain! |

All responses include `Access-Control-Allow-Origin: *` so the dashboard can fetch from any origin.

---

## Dashboard (dashboard/index.html)

A single HTML file — no build step, no server needed.

### Open Locally
```bash
# Just double-click the file, or:
open dashboard/index.html        # macOS
xdg-open dashboard/index.html   # Linux
```

Or serve it over HTTP if your browser blocks `file://` fetches:
```bash
cd dashboard
python3 -m http.server 8080
# Then open http://localhost:8080
```

### Usage
1. Power on the ESP32 and wait for it to connect to WiFi.
2. Note the IP address shown on the LCD (or Serial Monitor).
3. Open `dashboard/index.html` in a browser.
4. Type the ESP32 IP into the **IP Address** field and click **Connect**.
5. The IP is saved to `localStorage` — no need to re-enter it next time.

### Dashboard Features
- Animated circular gauge showing rain percentage
- Color-coded status badge (green / yellow / orange / red)
- **Mode badge**: shows "Auto" (green) or "Manual" (orange) — updates every poll
- **LED toggles**: iOS-style toggle switches for Red and Blue LEDs
  - Disabled with tooltip in Auto Mode
  - Enabled in Manual Mode — sends `GET /led` to ESP32
  - Visual state stays in sync with `/data` poll every second
- Raw analog and digital readings
- ESP32 uptime
- Line chart of last 60 readings (Chart.js via CDN)
- Connection status indicator
- Graceful CORS error handling with troubleshooting tips
- Dark UI, Thai/English labels

### CORS Note
If the browser shows a CORS error, verify:
1. The firmware is the updated `LCD_01.ino` with `addCORSHeaders()`.
2. The ESP32 and the machine running the browser are on the same LAN.
3. You are **not** accessing the dashboard from an HTTPS origin (mixed-content blocks plain HTTP fetches).

---

## Project Structure

```
ESP-32-Rain/
├── LCD_01.ino          # ESP32 firmware
├── README.md           # This file
└── dashboard/
    └── index.html      # Standalone web dashboard
```

---

## Troubleshooting

| Problem | Fix |
|---------|-----|
| LCD shows nothing | Check I2C address with I2C scanner sketch; try `0x3F` |
| `AO` always 0 or 4095 | Check sensor VCC — some modules need 5 V |
| WiFi never connects | Confirm SSID/password, 2.4 GHz only (ESP32 has no 5 GHz) |
| Dashboard cannot reach ESP32 | Same network? Try pinging the IP from your machine |
| CORS error in browser | Re-flash with updated firmware; check CORS headers in browser DevTools |
| LED toggles always greyed out | Board is in Auto Mode — press the physical button to switch to Manual |
| LED command returns `accepted: false` | Board is in Auto Mode — toggles are intentionally disabled |
| Button not responding | Check wiring: one pin pair to GPIO 27, other pair to GND |
