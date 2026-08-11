[README-flic-buttons.md](https://github.com/user-attachments/files/30939825/README-flic-buttons.md)
# Flic Button Automations

A set of Flic button automations running on a Flic Hub LR across 4 buttons. Two are phone locators — one per iPhone — with a secondary function most people wouldn't notice: double-click and hold trigger green or red light pulses in different rooms for household signals (dinner, bedtime). No yelling up the stairs required. The other two are bedside buttons, one per bedroom, controlling the room lamp on and off without getting out of bed.

Built around a household with two iPhones and a mix of Lutron switches and RGB/RGBW smart bulbs managed through Home Assistant.

---

## What Each Button Does

### Phone 1 Button
| Gesture | Action |
|---|---|
| Single click | Rings Phone 1 five times, 5 seconds apart (critical alert, full volume) |
| Double click | Basement lamp pulses **green** for 30 seconds |
| Hold | Basement lamp pulses **red** for 60 seconds |

### Phone 2 Button
| Gesture | Action |
|---|---|
| Single click | Rings Phone 2 five times, 5 seconds apart (critical alert, full volume) |
| Double click | Living room lamps pulse **green** for 30 seconds |
| Hold | Living room lamps pulse **red** for 60 seconds |

### Room 1 / Room 2 Buttons

Bedside buttons — single click on, double click off, so the lights can be killed without getting out of bed.

| Gesture | Action |
|---|---|
| Single click | Turns room lamp **on** |
| Double click | Turns room lamp **off** |

---

## The Light Pulse Logic

The visual alert system is designed to be non-destructive — if a lamp was already on at a particular color and brightness when the button fires, it returns to exactly that state when the pulse ends. The sequence:

1. Capture the current state of each lamp before touching it
2. Begin toggling on/off at the specified color and interval
3. On timeout, restore each lamp to its prior state (or off if it was off)

A second button press while a pulse is already running cancels the current pulse and starts a fresh one — no stacking.

---

## Requirements

- [Flic Hub LR](https://flic.io/flic-hub-lr) running the Flic Hub SDK
- Home Assistant instance reachable on the local network
- HA long-lived access token
- RGB or RGBW smart bulbs with `light.` entities in HA (must support `rgb_color`)
- Lutron or other switch entities for room lamps

---

## Setup

### 1. Fill in your values

At the top of the script, replace all placeholder values:

| Placeholder | What to put here |
|---|---|
| `http://X.X.X.X:8123` | Your HA local IP and port |
| `YOUR_HA_TOKEN` | HA profile → Long-Lived Access Tokens |
| `switch.room1_bedroom_lamps` | Your actual Lutron switch entity ID |
| `switch.room2_bedroom_lamps` | Your actual Lutron switch entity ID |
| `light.basement_game_area_...` | Your basement RGB lamp entity |
| `light.family_room_living_room_...` | Your living room lamp entities |
| `XX:XX:XX:XX:XX:XX` (×4) | MAC addresses of your four Flic buttons |

### 2. Find your button MAC addresses

In the Flic app, tap a button → gear icon → the address is listed under the button name in `XXXX-XXXXXX` format. Convert to colon-separated hex for the script (e.g. `ABCD-EF1234` → `AB:CD:EF:12:34`... use the format shown in the Flic Hub SDK docs for your firmware version).

### 3. Find your entity IDs

In HA: **Developer Tools → States**, filter by `light.` or `switch.` to find exact entity IDs. Names must match exactly — a trailing space or capitalization difference will silently fail.

### 4. Deploy via Flic Hub SDK

Upload the script through the Flic Hub SDK editor (hub.flic.io → your hub → SDK Apps). The script starts automatically and logs to the SDK console.

---

## Pulse Colors and Timing

Colors and durations are set as constants at the top of the script:

```js
const COLOR_GREEN = [0, 255, 0];
const COLOR_RED   = [255, 0, 0];
```

Pulse duration and toggle interval are passed per-call in `pulseLights()`. Current defaults:
- Double-click: 30 seconds, toggling every 5 seconds
- Hold: 60 seconds, toggling every 5 seconds

Adjust the `pulseLights(...)` calls in the button event handler to change these.

---

## Notes

- Phone ring notifications use HA's `notify` service and rely on the HA Companion app being installed on the target phone. The `critical: 1` flag bypasses Do Not Disturb on iOS.
- The pulse state restore is best-effort — if HA is unreachable when the pulse ends, the lamps will be left off rather than restored.
- RGB lamps that don't support `rgb_color` (e.g. tunable white only) will fail silently on the color call. Check your bulb's supported features in HA before assigning it to a pulse group.
- Longer write-up on the build and button placement at [toc010.com](https://toc010.com)
