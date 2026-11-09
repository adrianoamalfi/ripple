# Architecture

## Layout

```
lib/ripple_core/     framework-independent C++: the algorithm and the pure logic
  ripple_core.*        RSSI -> window variance -> motion / presence (Detector)
  ripple_calibrator.*  the four-step guided calibration (Calibrator)
  ripple_ha.*          MQTT topics and Home Assistant discovery messages
  ripple_wifi.*        WiFi credential validation, setup-network names, JSON escaping
src/                 the ESP8266 firmware
  main.cpp             sampling loop, wiring, serial commands
  wifi_setup.*         joins WiFi or runs the captive portal; mDNS
  web.*                HTTP + WebSocket server
  mqtt.*               MQTT client and Home Assistant publishing
  storage.*            settings in flash (LittleFS, JSON)
  portal_page.h        the WiFi setup page
  page.h               the tiny fallback page (/lite)
web/                 the dashboard (Vite + React + Tailwind + shadcn/ui)
firmware_assets/     the built dashboard, gzipped, embedded in the firmware
site/                the project website (GitHub Pages)
tools/               build scripts, serial monitor, screenshot generator
test/                unit tests that run on the computer
```

The split is deliberate: everything that decides *what the sensor concludes* lives
in `lib/ripple_core` with no Arduino dependency, so it is tested on the computer
and could be reused elsewhere (for example inside an ESPHome component).

## The sensing pipeline

1. **Sample.** Every 100 ms the main loop reads `WiFi.RSSI()` and sends a tiny UDP
   packet to the router. The RSSI only updates when frames arrive from the router;
   the router's reply to that packet keeps it fresh.
2. **Window.** `Detector` keeps the last 20 samples (2 s) and computes their
   variance.
3. **Decide.** Above the threshold: motion. Hysteresis ends it. Presence holds for
   30 s after the last motion.
4. **Calibrate.** `Calibrator` collects the per-second maximum variance of each
   step and computes the threshold and a quality verdict (see
   [calibration](calibration.md)). `Detector::applyCalibration` installs it.

## The firmware

A single cooperative loop, no RTOS. The web server and the MQTT client are
asynchronous (`ESPAsyncWebServer`, `AsyncMqttClient`): their callbacks never touch
the detector directly. Commands from the web, from MQTT and from the serial port
all go through one small queue that the main loop drains, so the detector is only
ever touched from one place.

**Memory** is the tight resource: about 41 KB of static RAM and 25–30 KB of
free heap while running. Per-message JSON is built with `snprintf` into static
buffers instead of the heap.

**Flash** holds the firmware (about 680 KB, including the 235 KB dashboard) and a
1 MB LittleFS partition for the settings. The dashboard is embedded in the
firmware, so a firmware update is also a dashboard update, and the saved settings
live elsewhere and survive it.

### WiFi and the setup portal

`wifi_setup` joins the saved network. With none saved, or after 25 s without
reaching it, it opens the open network `Ripple-xxxxxx` with a DNS server that
answers every name with the board's address; the web server answers unknown paths
with a redirect to the setup page, which is what makes phones open it by
themselves. A network chosen in the portal is tried first and only saved once it
works. The mDNS responder (`ripple-xxxxxx.local`) is restarted whenever the setup
network goes up or down, because it otherwise keeps using a dead interface.

### MQTT

The client connects in the background and retries with a growing delay. After each
connection it publishes availability, the six Home Assistant discovery messages
(spread over half a second), then states. See [home-assistant.md](home-assistant.md)
and [api.md](api.md#mqtt-topics).

## The dashboard

A single-page app that talks to the board over the WebSocket, built into **one
self-contained HTML file** (no fonts, no CDN) and gzipped to about 235 KB. It has a
built-in **demo mode** with a simulated board (`?demo`), used for development, for
the tests, for the screenshots and for the project site.

## Decisions worth knowing

- **No CSI.** The ESP8266 only exposes an integer RSSI. The algorithm is built
  around that, and the limits that follow are documented rather than hidden.
- **Calibration from data, not a formula.** A fixed `mean + 3σ` threshold sat
  inside the noise the calibration itself had seen. The wizard measures walking and
  empty-room noise separately and checks the result.
- **A fork of the MQTT client.** The original `AsyncMqttClient` has a bug that
  resends queued QoS 0 packets with the DUP flag after any disconnect, so brokers
  drop the board in a loop. The ESPHome fork does not have it.
- **Credentials stay on the board.** WiFi and MQTT passwords are stored in plain
  text in flash and are never sent back by the API.
