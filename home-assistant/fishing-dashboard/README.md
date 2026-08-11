[README.md](https://github.com/user-attachments/files/30940155/README.md)
# Fishing Conditions Dashboard

Anyone who fishes knows the problem: tide charts are on one tab, weather is on another, and by the time you've cross-referenced everything it's already time to leave. This dashboard puts the information that actually matters — tide state, wind, swell, and a composite conditions score — in one place, readable at a glance before you decide whether to load the truck.

Built for fishing Long Island Sound out of coastal Connecticut. The scoring weights and NOAA station are tuned for that area, but adjusting for your spot is straightforward.

---

## What It Shows

- **Tide state and next turning point** — current height, rising or falling, hours to next high or low
- **Wind speed and direction** — sourced from Open-Meteo
- **Conditions score** — a weighted composite (0–100) factoring tide phase, wind, and time of day
- **Single-view layout** — no tabs, no digging; everything visible without scrolling

The goal is a quick yes/no read on whether conditions are worth it, without opening five other apps or tabs.

---

## Requirements

- Home Assistant 2024.x or later
- The following integrations enabled:
  - [Open-Meteo](https://www.home-assistant.io/integrations/open_meteo/) (built-in, no API key needed)
  - [RESTful sensor](https://www.home-assistant.io/integrations/rest/) for NOAA tide data

---

## Setup

### 1. Find your NOAA station ID

Go to [tidesandcurrents.noaa.gov](https://tidesandcurrents.noaa.gov/), find your nearest station, and copy the numeric ID from the URL.

### 2. Fill in placeholders

Search the YAML for the following and replace each with your real values:

| Placeholder | What to put here |
|---|---|
| `YOUR_NOAA_STATION_ID` | Your 7-digit NOAA station ID (e.g. `8467150`) |
| `YOUR_LATITUDE` | Decimal latitude for Open-Meteo weather pull |
| `YOUR_LONGITUDE` | Decimal longitude for Open-Meteo weather pull |

### 3. Add to your HA dashboard

In the HA dashboard editor, open the raw config view and paste in the contents of `fishing_dashboard.yaml`. Any supporting sensor definitions are included in the same file.

### 4. Adjust the scoring weights

The scoring model is in the template sensor block within the YAML. Weights are commented — tweak them to match what actually matters at your spot. Incoming tide before dawn hits differently than a midday outgoing with wind chop.

---

## Notes

- Open-Meteo and the NOAA tide prediction API are both free with no authentication required
- Tide times are returned in UTC; HA handles timezone conversion if your instance timezone is set correctly
