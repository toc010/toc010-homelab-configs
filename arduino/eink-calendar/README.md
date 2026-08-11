[README-eink-calendar.md](https://github.com/user-attachments/files/30939759/README-eink-calendar.md)
# E-Ink Desk Calendar

A desk calendar running on an Adafruit HUZZAH32 (ESP32) with a Waveshare 4.2" black-and-white e-ink display. It shows the day name, numeric date, a monthly grid with today highlighted, battery level, and a rotating Basho haiku — one per day, cycling through 30. Refreshes once at midnight via deep sleep wake, so the display is completely static between updates.

The display is sharp, readable from across a desk, and barely touches the battery between refreshes. It's been running on my desk for months without needing attention.

---

## Hardware

| Component | Details |
|---|---|
| Microcontroller | [Adafruit HUZZAH32 – ESP32 Feather](https://www.adafruit.com/product/3405) |
| Display | [Waveshare 4.2" E-Ink Display (B/W)](https://www.waveshare.com/4.2inch-e-paper.htm) — GDEY042T81 driver |
| Connection | SPI — pin assignments in comments at top of sketch |
| Power | LiPo battery via HUZZAH32 onboard charger, or USB |

---

## What It Displays

- **Day name** — large, upper left
- **Numeric date** — `M/D/YY` format, lower left
- **Battery level** — icon + percentage, lower left panel
- **Monthly calendar grid** — full month, today filled/inverted
- **Basho haiku** — italic, right panel, rotates daily by day-of-year (30 haiku, pre-wrapped)

---

## Dependencies

Install all of these via Arduino Library Manager or manually:

| Library | Purpose |
|---|---|
| [GxEPD2](https://github.com/ZinggJM/GxEPD2) | E-ink display driver |
| [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library) | Font rendering (FreeSans family) |
| [NTPClient](https://github.com/arduino-libraries/NTPClient) | NTP time sync |
| [Time](https://github.com/PaulStoffregen/Time) | `TimeLib.h` — `year()`, `month()`, `day()`, `weekday()` |
| Arduino ESP32 core | ESP32 board support for Arduino IDE |

---

## Setup

### 1. Fill in your WiFi credentials

At the top of the sketch:

```cpp
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

### 2. Set your UTC offset

Also near the top:

```cpp
const long utcOffsetSeconds = -14400;  // EDT (UTC-4)
// Use -18000 for EST (UTC-5, Nov–Mar)
```

There's no automatic DST handling — flip this manually twice a year, or add a DST library if you want to automate it.

### 3. Flash the device

First flash must be wired over USB. Select **Adafruit ESP32 Feather** as your board in Arduino IDE, then upload normally. After first flash, the device wakes at midnight and re-flashes are OTA if you add OTA support — otherwise just re-flash by wire when needed.

### 4. SPI wiring

Pin assignments are in comments at the top of the sketch:

```
CS   → GPIO 15
DC   → GPIO 33
RST  → GPIO 27
BUSY → GPIO 32
```

Double-check these against your specific Waveshare panel revision before powering on.

---

## How It Works

On boot, the device:
1. Connects to WiFi
2. Syncs time via NTP (retries up to 3x, validates year is 2024–2099 to catch epoch bugs)
3. Reads battery voltage from pin A13 (GPIO35), maps it to a percentage via a LiPo discharge curve
4. Selects a haiku by `(day_of_year - 1) % 30`
5. Renders the full display
6. Disconnects WiFi, hibernates the display, and deep sleeps until midnight

If WiFi fails or NTP returns a bad year, it falls back to a 1-hour sleep and retries.

---

## Known Issues & Things Fixed Along the Way

- **NTP epoch bug** — early versions displayed garbage dates on first boot before sync completed. Fixed with a retry loop and year-range validation before rendering.
- **Haiku rendering** — multi-line text rendering needed explicit pre-wrapping at 22 chars for `FreeSansOblique9pt7b` in the 194px right column. Runtime pixel-measurement wrapping was silently dropping lines.
- **Battery percentage** — the HUZZAH32 ADC is noisy and nonlinear, especially at low voltages. The lookup table in the sketch is a calibrated approximation against a real LiPo discharge curve — yours may read slightly differently depending on your battery.
- **Unsigned sleep math** — sleep duration calculation uses `unsigned long` throughout to avoid a 2106 overflow bug in epoch arithmetic.

---

## Notes

- E-ink panels have a limited full-refresh cycle count. This sketch uses full refresh once per day, which should give years of life at that rate.
- Display takes ~2 seconds to fully refresh — normal for e-ink.
- If the display shows artifacts after flashing, a full power cycle (not just reset) usually clears it.
- Longer write-up on the build and layout iterations at [toc010.com](https://toc010.com)
