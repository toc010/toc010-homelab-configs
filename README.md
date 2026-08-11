# toc010-homelab-configs

A collection of configurations, firmware, and automations from my homelab. Everything here is something I actually run — shared in the spirit of the homelab community that helped me build it.

Each project lives in its own subdirectory with a dedicated README covering what it does, what you'll need, and what to substitute before you deploy it.

---

## Projects

| Project | Description |
|---|---|
| [`home-assistant/fishing-dashboard`](./home-assistant/fishing-dashboard/) | HA dashboard for real-time fishing conditions on Long Island Sound — NOAA tides, Open-Meteo weather, weighted scoring |
| [`arduino/eink-calendar`](./arduino/eink-calendar/) | E-ink desk calendar on an Adafruit HUZZAH32 + Waveshare 4.2" display — date, day name, monthly grid, battery level, and a rotating Basho haiku |
| [`flic/flic-buttons`](./flic/flic-buttons/) | Flic Hub automations for two bedside buttons — phone finding with critical alerts, visual light pulses for household signals, and bedside lamp control |

---

## Environment

My homelab runs on Proxmox with Home Assistant in a dedicated VM. Details on the broader infrastructure aren't covered here, but may be relevant context if something doesn't behave as expected in a different network topology.

---

## Using These Configs

All secrets have been replaced with `YOUR_*` placeholders. Search each file for `YOUR_` before deploying — anything that needs a real value will surface.

Nothing here is plug-and-play. Treat these as starting points and reference implementations, not drop-in replacements. Your entity names, network topology, and hardware revisions will differ.

---

## Lab Notebook

Longer write-ups, iterations, and build notes for most of these projects live at [toc010.com](https://toc010.com).

---

## License

MIT
